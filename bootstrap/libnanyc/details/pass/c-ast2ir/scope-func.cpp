#include "scope.h"
#include <yuni/core/noncopyable.h>
#include "details/utils/check-for-valid-identifier-name.h"
#include "details/grammar/nany.h"
#include "details/ast/ast.h"
#include "details/utils/dataregister.h"
#include "details/atom/visibility.h"

using namespace Yuni;

namespace ny {
namespace ir {
namespace Producer {

namespace {

struct FuncInspector final : public Yuni::NonCopyable<FuncInspector> {
	using FuncnameType = CString<config::maxSymbolNameLength, false>;

	//! Default
	FuncInspector(Scope& scope);

	bool inspect(AST::Node& node);

	//! Parent scope
	Scope& scope;
	//! Func body
	AST::Node* body = nullptr;

	//! name of the function
	FuncnameType funcname;
	//! Flag to remember if the function is the new operator
	// (for 'self' parameters for example)
	bool isNewOperator = false;

private:
	bool inspectVisibility(AST::Node&);
	bool inspectKind(AST::Node&);
	bool inspectParameters(AST::Node*, AST::Node*);
	bool inspectReturnType(AST::Node&);
	bool inspectSingleParameter(uint pindex, AST::Node&, uint32_t paramoffset);
	bool inspectAttributes(Attributes&);

	//! Flag to determine whether the 'self' parameter is implicit
	// (simply dictated if within a class)
	bool hasImplicitSelf = false;

}; // class FuncInspector

FuncInspector::FuncInspector(Scope& scope)
	: scope(scope)
	, hasImplicitSelf(scope.isWithinClass()) {
}

bool FuncInspector::inspectVisibility(AST::Node& node) {
	bool success = true;
	if (false /* TODO within a class */) {
		// visibility per attribute is not allowed inside a class
		// (the visibility if set by region)
		error(node) << "visibility oer attribute is not allowed inside a class";
		success = false;
	}
	else {
		// visibility from the node
		ny::Visibility visibility = ny::toVisibility(node.text);
		if (likely(visibility != Visibility::undefined)) {
			// in the global namespace, only 'public' and 'internal' are accepted
			if (unlikely(visibility != Visibility::vpublic and visibility != Visibility::vinternal)) {
				visibility = Visibility::vinternal;
				error(node) << "invalid visibility '" << node.text << "'";
				success = false;
			}
			// set the visibility
			scope.emitDebugpos(node);
			ir::emit::pragma::visibility(scope.ircode(), visibility);
		}
		else {
			// should really never happen...
			error(node) << "invalid visibility '" << node.text << "'";
			success = false;
		}
	}
	return success;
}

bool FuncInspector::inspectKind(AST::Node& node) {
	if (unlikely(node.children.size() != 1))
		return unexpectedNode(node, "[funckind/child]");
	auto& kindchild = node.children.front();
	switch (kindchild.rule) {
		//
		// func/operator definition
		//
		case AST::rgFunctionKindFunction: {
			if (unlikely(kindchild.children.size() != 1))
				return unexpectedNode(kindchild, "[funckindfunc/child]");
			auto& symbolname = kindchild.children.front();
			AnyString name = scope.getSymbolNameFromASTNode(symbolname);
			if (not checkForValidIdentifierName(symbolname, name))
				return false;
			funcname = name;
			break;
		}
		case AST::rgFunctionKindOperator: {
			if (unlikely(kindchild.children.size() != 1))
				return unexpectedNode(kindchild, "[funckindfunc/child]");
			auto& opname = kindchild.children.front();
			if (unlikely(opname.rule != AST::rgFunctionKindOpname or not opname.children.empty()))
				return unexpectedNode(opname, "[funckindfunc/child]");
			Flags<IdNameFlag> flags = IdNameFlag::isOperator;
			if (hasImplicitSelf) // not within a class
				flags += IdNameFlag::isInClass;
			if (not checkForValidIdentifierName(opname, opname.text, flags))
				return false;
			funcname = '^';
			if (opname.text != "dispose")
				funcname << opname.text;
			else
				funcname << "#user-dispose";
			isNewOperator = (opname.text == "new");
			break;
		}
		case AST::rgFunctionKindView: {
			if (kindchild.children.empty()) {
				// default view
				funcname = "^view^default";
			}
			else {
				auto& symbolname = kindchild.children.front();
				AnyString name = scope.getSymbolNameFromASTNode(symbolname);
				if (not checkForValidIdentifierName(symbolname, name))
					return false;
				funcname.clear();
				funcname << "^view^" << name;
			}
			break;
		}
		default:
			return unexpectedNode(kindchild, "[funckind]");
	}
	return true;
}

bool FuncInspector::inspectSingleParameter(uint pindex, AST::Node& node, uint32_t paramoffset) {
	assert(node.rule == AST::rgFuncParam and "invalid func param node");
	// lvid for the parameter
	uint32_t lvid = pindex + 1 + 1; // params are 2-based (1 is the return type)
	// name of the parameter
	AnyString paramname;
	// exit status
	bool success = true;
	// operator new (self x)
	bool autoMemberAssignment = false;
	auto& irout = scope.ircode();
	// we can have the following pattern: ref a: sometype
	// the qualifiers will be reset by the type definition (if any)
	// thus, if 'ref' or 'const' is provided, the opcodes must be generated _after_
	// ref parameter
	bool ref = false;
	// constant parameter
	bool constant = false;
	scope.emitDebugpos(node);
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgRef:
				ref = true;
				break;
			case AST::rgConst:
				constant = true;
				break;
			case AST::rgCref:
				ref = constant = true;
				break;
			case AST::rgIdentifier: { // name of the parameter
				if (checkForValidIdentifierName(child, child.text))
					paramname = child.text;
				else {
					paramname = scope.acquireString(String() << "_p_" << (void*)(&child));
					success = false;
				}
				break;
			}
			case AST::rgVarType: {
				uint32_t paramtype = 0;
				for (auto& childtype : child.children) {
					switch (childtype.rule) {
						case AST::rgType: {
							success &= scope.visitASTType(childtype, paramtype);
							break;
						}
						default:
							success &= unexpectedNode(childtype, "[func/param-type]");
					}
				}
				if (isValidLocalvar(paramtype)) {
					auto& operands    = irout.emit<isa::Op::follow>();
					operands.follower = lvid;
					operands.lvid     = paramtype;
					operands.symlink  = 1;
				}
				break;
			}
			case AST::rgFuncParamSelf: {
				if (likely(isNewOperator))
					autoMemberAssignment = true;
				else {
					success = false;
					error(child)
							<< "automatic variable assignment is only allowed in the operator 'new'";
				}
				break;
			}
			case AST::rgFuncParamVariadic: {
				error(child) << "variadic parameters not implemented";
				success = false;
				break;
			}
			default:
				success = unexpectedNode(child, "[param]");
		}
	}
	if (unlikely(paramname.empty())) {
		// generating a pseudo identifier name {should never happen}
		ice(node) << "anonymous variable not allowed";
		success   = false;
		paramname = scope.acquireString(String() << "_p_" << (void*)(&node));
	}
	// update the parameter name within the ir code (whatever previous result)
	auto sid = irout.stringrefs.ref(paramname);
	// update the parameter opcode
	{
		auto& opparam = irout.at<isa::Op::blueprint>(paramoffset);
		opparam.name  = sid;
		if (autoMemberAssignment)
			opparam.kind = (uint32_t) ir::isa::Blueprint::paramself;
	}
	// the qualifiers may have been set by the type definition
	// thus they must be overriden and not always reset
	if (ref)
		ir::emit::type::qualifierRef(irout, lvid, true);
	if (constant)
		ir::emit::type::qualifierConst(irout, lvid, true);
	return success;
}

bool FuncInspector::inspectParameters(AST::Node* node, AST::Node* nodeTypeParams) {
	// total number of parameters
	uint32_t paramCount;
	if (node != nullptr) {
		assert(node->rule == AST::rgFuncParams and "invalid func params node");
		// determining the total number of parameters for the function
		// if within a class, a implicit parameter 'self' will be added to each function
		paramCount = node->children.size();
	}
	else
		paramCount = 0u;
	if (hasImplicitSelf) // implicit 'self' parameter
		++paramCount;
	scope.emitDebugpos(node);
	bool success = true;
	if (unlikely(paramCount > config::maxFuncDeclParameterCount - 1)) { // too many parameters ?
		assert(node != nullptr);
		error(*node) << "hard limit: too many parameters. Got "
			<< paramCount << ", expected: " << (config::maxFuncDeclParameterCount - 1);
		success = false;
		paramCount = config::maxFuncDeclParameterCount - 1;
	}
	// creating all blueprint parameters first to have their 'lvid' predictible
	// as a consequence, classdef for parameters start from 2 (0: null, 1: return type)
	// and variables for parameters start from 1
	auto& irout = scope.ircode();
	// dealing first with the implicit parameter 'self'
	if (hasImplicitSelf) {
		uint32_t selfid = scope.nextvar();
		ir::emit::blueprint::param(irout, selfid, "self");
		ir::emit::type::isself(irout, selfid);
	}
	// iterating through all other user-defined parameters
	uint32_t offset = (hasImplicitSelf) ? 1u : 0u;
	bool hasParameters = paramCount - offset > 0u;
	// declare all parameters first
	uint32_t paramOffsets[config::maxPushedParameters];
	if (hasParameters) {
		assert(paramCount - offset < config::maxPushedParameters);
		for (uint32_t i = offset; i < paramCount; ++i) { // reserving lvid for each parameter
			uint32_t opaddr = ir::emit::blueprint::param(irout, scope.nextvar(), nullptr);
			paramOffsets[i - offset] = opaddr;
		}
	}
	// reserve registers (as many as parameters) for cloning parameters
	for (uint32_t i = 0u; i != paramCount; ++i)
		ir::emit::alloc(irout, scope.nextvar());
	// Generating ir for template parameters before the ir code for parameters
	// (especially for being able to use these types)
	if (nodeTypeParams)
		success &= scope.visitASTDeclGenericTypeParameters(*nodeTypeParams);
	if (hasParameters) {
		// inspecting each parameter
		ir::emit::trace(irout, "function parameters");
		// Generate ir (typing and default value) for each parameter
		assert(node != nullptr and "should not be here if there is no real parameter");
		for (uint32_t i = offset; i < paramCount; ++i)
			success &= inspectSingleParameter(i, node->children[i - offset], paramOffsets[i - offset]);
	}
	return success;
}

bool FuncInspector::inspectReturnType(AST::Node& node) {
	assert(node.rule == AST::rgFuncReturnType and "invalid return type node");
	assert(not node.children.empty() and " should been tested already");
	auto& irout = scope.ircode();
	ir::emit::trace(irout, "return type");
	uint32_t rettype = 0;
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgType: {
				bool ok = scope.visitASTType(child, rettype);
				if (unlikely(not ok))
					return false;
				break;
			}
			default:
				return unexpectedNode(child, "[func/ret-type]");
		}
	}
	if (not isAny(rettype)) {
		scope.emitDebugpos(node);
		if (rettype == 0) {
			rettype = scope.createLocalBuiltinVoid(node);
			assert(rettype != 0);
		}
		assert(rettype != 0);
		auto& operands    = irout.emit<isa::Op::follow>();
		operands.follower = 1; // params are 2-based (1 is the return type)
		operands.lvid     = rettype;
		operands.symlink  = 0;
	}
	ir::emit::trace(irout, "end return type");
	return true;
}

bool FuncInspector::inspectAttributes(Attributes& attrs) {
	auto& irout = scope.ircode();
	if (attrs.flags(Attributes::Flag::shortcircuit)) {
		ir::emit::pragma::shortcircuit(irout, true);
		attrs.flags -= Attributes::Flag::shortcircuit;
	}
	if (attrs.flags(Attributes::Flag::builtinAlias)) {
		assert(attrs.builtinAlias != nullptr);
		ShortString64 value;
		if (not AST::appendEntityAsString(value, *attrs.builtinAlias))
			return error(*attrs.builtinAlias) << "invalid builtinalias attribute";
		ir::emit::pragma::builtinAlias(irout, value);
		attrs.flags -= Attributes::Flag::builtinAlias;
	}
	if (attrs.flags(Attributes::Flag::doNotSuggest)) {
		ir::emit::pragma::suggest(irout, false);
		attrs.flags -= Attributes::Flag::doNotSuggest;
		assert(not attrs.flags(Attributes::Flag::doNotSuggest));
	}
	if (attrs.flags(Attributes::Flag::threadproc)) {
		// currently silently ignored
		attrs.flags -= Attributes::Flag::threadproc;
	}
	return true;
}

bool FuncInspector::inspect(AST::Node& node) {
	// exit status
	bool success = true;
	// the node related to parameters
	AST::Node* nodeParams = nullptr;
	// the node related to template parameters
	AST::Node* nodeGenTParams = nullptr;
	// the node related to the return type
	AST::Node* nodeReturnType = nullptr;
	// function attributes, if any
	if (!!scope.attributes)
		success &= inspectAttributes(*scope.attributes);
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgFunctionKind:   {
				success &= inspectKind(child);
				break;
			}
			case AST::rgFuncParams:     {
				nodeParams = &child;
				break;
			}
			case AST::rgVisibility:     {
				success &= inspectVisibility(child);
				break;
			}
			case AST::rgFuncReturnType: {
				if (not child.children.empty()) nodeReturnType = &child;
				break;
			}
			case AST::rgFuncBody:       {
				body = &child;
				break;
			}
			case AST::rgClassTemplateParams: {
				nodeGenTParams = &child;
				break;
			}
			default:
				success &= unexpectedNode(child, "[func]");
		}
	}
	if (funcname.empty())
		funcname = "<anonymous>";
	// the code currently has some prerequisites:
	// lvid %0: <invalid value>
	// lvid %1: the return type
	// lvid %2..N: the N parameters (if any)
	// lvid %2+N+1 .. %2+N+1+N: a reserved local variables for copying parameters
	//    (if required)
	// ...: generic type parameters if any
	// ...: definition of each parameters (and maybe variable cloning)
	// %1: the variable representing the return type (only a very simple definition)
	if (nodeReturnType)
		scope.createLocalBuiltinAny(node); // pre-create the return type
	else
		scope.createLocalBuiltinVoid(node);
	// ...then the parameters
	success &= inspectParameters(nodeParams, nodeGenTParams);
	// generate opcodes for return type verification (after parameters)
	if (nodeReturnType)
		success &= inspectReturnType(*nodeReturnType);
	return success;
}

} // anonymous namespace

bool Scope::visitASTFunc(AST::Node& node) {
	assert(node.rule == AST::rgFunction);
	assert(not node.children.empty());
	if (unlikely(context.ignoreAtoms))
		return true;
	// new scope
	ir::Producer::Scope scope{*this};
	scope.moveAttributes(*this);
	// reset internal counter for generating local classdef in the current scope
	scope.resetLocalCounters();
	scope.kind = Scope::Kind::kfunc;
	scope.broadcastNextVarID = false;
	auto& irout = ircode();
	// creating a new blueprint for the function
	uint32_t bpoffset = ir::emit::blueprint::func(irout);
	uint32_t bpoffsiz = ir::emit::pragma::blueprintSize(irout);
	uint32_t bpoffsck = ir::emit::increaseStacksize(irout);
	// making sure that debug info are available
	context.pPreviousDbgLine = (uint32_t) - 1; // forcing debug infos
	ir::emit::dbginfo::filename(irout, context.dbgSourceFilename);
	scope.emitDebugpos(node);
	bool success = true;
	bool isOperator = false;
	// evaluate the whole function, and grab the node body for continuing evaluation
	auto* body = ([&]() -> AST::Node* {
		FuncInspector inspector{scope};
		success = inspector.inspect(node);

		isOperator = (inspector.funcname.first() == '^');
		// update the func name
		auto sid = irout.stringrefs.ref(inspector.funcname);
		irout.at<isa::Op::blueprint>(bpoffset).name = sid;
		return inspector.body;
	})();
	ir::emit::pragma::funcbody(irout);
	if (likely(body != nullptr)) {
		ir::emit::trace(irout, "\nfunc body");
		// continue evaluating the func body independantly of the previous data and results
		for (auto& stmtnode : body->children)
			success &= scope.visitASTStmt(stmtnode);
	}
	// end of the blueprint
	ir::emit::scopeEnd(irout);
	uint32_t blpsize = irout.opcodeCount() - bpoffset;
	irout.at<isa::Op::pragma>(bpoffsiz).value.blueprintsize = blpsize;
	irout.at<isa::Op::stacksize>(bpoffsck).add = scope.nextVarID + 1u;
	return success;
}

} // namespace Producer
} // namespace ir
} // namespace ny
