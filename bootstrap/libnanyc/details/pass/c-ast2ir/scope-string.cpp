#include "scope.h"
#include "details/grammar/nany.h"
#include "details/ast/ast.h"
#include "details/ir/emit.h"
#include <limits>

using namespace Yuni;


namespace ny {
namespace ir {
namespace Producer {


namespace {


bool convertCharExtended(char& irout, char c) {
	switch (c) {
		case 'r':
		irout = '\r';
			break;
		case 'n':
		irout = '\n';
			break;
		case 't':
		irout = '\t';
			break;
		case 'v':
		irout = '\v';
			break;
		case '\\':
		irout = '\\';
			break;
		case '"':
		irout = '"';
			break;
		case 'a':
		irout = '\a';
			break;
		case 'b':
		irout = '\b';
			break;
		case 'f':
		irout = '\f';
			break;
		case '0':
		irout = '\0';
			break;
		case 'c':  /* produce no further output */
			break;
		default:
			return false;
	}
	return true;
}


} // anonymous namespace


bool Scope::visitASTExprChar(AST::Node& node, uint32_t& localvar) {
	char c /*= '\0'*/;
	switch (node.children.size()) {
		case 0: {
			// simple char
			if (unlikely(node.text.size() != 3)) // 'X'
				return (ice(node) << "invalid char node");
			c = node.text[1];
			break;
		}
		case 1: {
			auto& extended = node.children[0];
			switch (extended.rule) {
				case AST::rgCharExtended: {
					if (extended.text.size() == 1) {
						char extC = '\0';
						if (not convertCharExtended(extC, extended.text[0]))
							return (error(extended) << "invalid escaped character '\\" << extended.text << '\'');
						c = extC;
					}
					else {
						error(extended) << "invalid escaped character '\\" << extended.text << '\'';
						return false;
					}
					break;
				}
				default:
					return unexpectedNode(node, "[char/extended]");
			}
			break;
		}
		default:
			return unexpectedNode(node, "[char]");
	}
	if (!context.reuse.ascii.node)
		context.reuse.prepareReuseForAsciis();
	uint32_t hardcodedlvid = createLocalBuiltinInt(node, CType::t_u8, static_cast<uint8_t>(c));
	ShortString16 lvidstr;
	lvidstr = hardcodedlvid;
	context.reuse.ascii.lvidnode->text = lvidstr;
	bool success = visitASTExprNew(*(context.reuse.ascii.node), localvar);
	// avoid crap in the debugger
	context.reuse.ascii.lvidnode->text.clear();
	return success;
}


bool Scope::visitASTExprStringLiteral(AST::Node& node, uint32_t& localvar) {
	// when called, this rule represents an internal cstring
	// thus, this function can not be called by an user-defined string
	emitDebugpos(node);
	localvar = ir::emit::alloctext(ircode(), nextvar(), node.text);
	return true;
}


bool Scope::visitASTExprString(AST::Node& node, uint32_t& localvar) {
	assert(node.rule == AST::rgString);
	emitDebugpos(node);
	// transform string literals into implicit `new string()`
	//
	// several patterns can arise for declaring a string:
	// 1. an empty string: like `var s = ""`
	// 2. a string with a simple literal: `var s = "hello world"`
	// 3. or a more complex string, with string interpolation for example
	//   `var w = "world"; var s = "hello \(w) another literal";` (where \(w) is an expr)
	// create a string object
	if (!context.reuse.string.createObject)
		context.reuse.prepareReuseForStrings();
	bool success = visitASTExprNew(*(context.reuse.string.createObject), localvar);
	if (unlikely(not success))
		return false;
	auto& irout = ircode();
	uint32_t idlvid = ir::emit::alloc(irout, nextvar());
	ir::emit::identify(irout, idlvid, "append", localvar);
	uint32_t calllvid = ir::emit::alloc(irout, nextvar());
	ir::emit::identify(irout, calllvid, "^()", idlvid); // functor
	context.reuse.string.text.clear();
	AST::Node* firstLiteralNode = nullptr;
	auto flush = [&]() {
		emitDebugpos(*firstLiteralNode);
		uint32_t sid = ir::emit::alloctext(irout, nextvar(), context.reuse.string.text);
		uint32_t lid = ir::emit::allocu64(irout, nextvar(), CType::t_u32, context.reuse.string.text.size());
		uint32_t ret = ir::emit::alloc(irout, nextvar(), CType::t_void);
		ir::emit::push(irout, sid); // text: __text
		ir::emit::push(irout, lid); // size: __u64
		ir::emit::call(irout, ret, calllvid);
	};
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgStringLiteral: {
				if (not child.text.empty()) {
					context.reuse.string.text += child.text;
					if (nullptr == firstLiteralNode)
						firstLiteralNode = &child;
				}
				break;
			}
			case AST::rgStringInterpolation: {
				if (child.children.empty())
					break;
				if (not context.reuse.string.text.empty()) {
					flush();
					firstLiteralNode = nullptr;
					context.reuse.string.text.clear();
				}
				for (auto& expr : child.children) {
					if (unlikely(expr.rule != AST::rgExpr))
						return unexpectedNode(expr, "[string-interpolation]");
					// creating a scope for temporary expression (string interpolation)
					ir::emit::ScopeLocker opscope{irout};
					emitDebugpos(expr);
					uint32_t lvid;
					if (not visitASTExpr(expr, lvid))
						return false;
					emitDebugpos(expr);
					uint32_t ret = ir::emit::alloc(irout, nextvar(), CType::t_void);
					ir::emit::push(irout, lvid);
					ir::emit::call(irout, ret, calllvid);
				}
				break;
			}
			case AST::rgCharExtended: {
				if (nullptr == firstLiteralNode)
					firstLiteralNode = &child;
				if (child.text.size() == 1) {
					char c = '\0';
					if (not convertCharExtended(c, child.text[0]))
						return (error(child) << "invalid escaped character '\\" << child.text << '\'');
					context.reuse.string.text += c;
				}
				else {
					error(child) << "invalid escaped character '\\" << child.text << '\'';
					return false;
				}
				break;
			}
			default: {
				return unexpectedNode(child, "[expr-string]");
			}
		}
	}
	if (firstLiteralNode != nullptr)
		flush();
	return true;
}


} // namespace Producer
} // namespace ir
} // namespace ny
