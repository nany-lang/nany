#include "scope.h"
#include "details/utils/check-for-valid-identifier-name.h"

using namespace Yuni;


namespace ny {
namespace ir {
namespace Producer {


bool Scope::visitASTExprClosure(AST::Node& node, uint32_t& localvar) {
	assert(node.rule == AST::rgFunction);
	AST::Node* body = nullptr;
	AST::Node* rettype = nullptr;
	AST::Node* params = nullptr;
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgFuncBody: {
				body = &child;
				break;
			}
			case AST::rgFuncParams: {
				params = &child;
				break;
			}
			case AST::rgFuncReturnType: {
				rettype = &child;
				break;
			}
			case AST::rgFunctionKind: {
				if (child.children.size() != 1 or child.children[0].rule != AST::rgFunctionKindFunction)
					return error(child) << "only function definition is allowed for closure";
				break;
			}
			default:
				return unexpectedNode(child, "[closure]");
		}
	}
	if (unlikely(!body))
		return error(node) << "empty body not allowed in closures";
	if (!context.reuse.closure.node)
		context.reuse.prepareReuseForClosures();
	emitDebugpos(node);
	auto& expr = *context.reuse.closure.node;
	auto& targetBody = *context.reuse.closure.funcbody;
	targetBody.children = body->children;
	if (params)
		*context.reuse.closure.params = *params;
	else
		context.reuse.closure.params->children.clear();
	if (rettype)
		*context.reuse.closure.rettype = *rettype;
	else
		context.reuse.closure.rettype->children.clear();
	// for debugging
	context.reuse.closure.classdef->offset = node.offset;
	context.reuse.closure.classdef->offsetEnd = node.offsetEnd;
	context.reuse.closure.func->offset = node.offset;
	context.reuse.closure.func->offsetEnd = node.offsetEnd;
	bool success = visitASTExpr(expr, localvar);
	// avoid crap in the debugger
	context.reuse.closure.params->children.clear();
	context.reuse.closure.rettype->children.clear();
	targetBody.children.clear();
	return success;
}


} // namespace Producer
} // namespace ir
} // namespace ny
