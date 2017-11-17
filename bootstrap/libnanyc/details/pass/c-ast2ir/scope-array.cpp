#include "scope.h"
#include "details/ir/emit.h"

using namespace Yuni;

namespace ny {
namespace ir {
namespace Producer {

bool Scope::visitASTArray(AST::Node& node, uint32_t& localvar) {
	assert(node.rule == AST::rgArray);
	if (unlikely(!context.reuse.shorthandArray.node))
		context.reuse.prepareReuseForShorthandArray();
	auto& typeofcall = *context.reuse.shorthandArray.typeofcall;
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgCallParameter: {
				typeofcall.children.push_back(&child);
				break;
			}
			default:
				return unexpectedNode(child, "[array-shorthand]");
		}
	}
	if (unlikely(typeofcall.children.empty()))
		return (error(node) << "at least one type is required for shorthand array declarations");
	emitDebugpos(node);
	bool success = visitASTExprNew(*context.reuse.shorthandArray.node, localvar);
	typeofcall.children.clear();
	if (success) {
		auto& irout = ircode();
		ir::emit::ScopeLocker opscope{irout};
		emitDebugpos(node);
		uint32_t lvidappend = ir::emit::alloc(irout, nextvar());
		ir::emit::identify(irout, lvidappend, "append", localvar);
		uint32_t func = ir::emit::alloc(irout, nextvar());
		ir::emit::identify(irout, func, "^()", lvidappend);
		for (auto& child : node.children) {
			if (child.rule == AST::rgCallParameter) {
				uint32_t lvid = 0;
				success &= visitASTExpr(child.firstChild(), lvid);
				if (success) {
					auto callret = ir::emit::alloc(irout, nextvar());
					ir::emit::push(irout, lvid);
					ir::emit::call(irout, callret, func);
				}
			}
		}
	}
	return success;
}

} // namespace Producer
} // namespace ir
} // namespace ny
