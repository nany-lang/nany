#include "scope.h"
#include "details/ir/emit.h"

using namespace Yuni;

namespace ny {
namespace ir {
namespace Producer {

namespace {

bool appendSingleType(Scope& scope, AST::Node& expr, ir::Sequence& out, uint32_t& previous) {
	scope.emitDebugpos(expr);
	uint32_t lvid = 0;
	bool ok = scope.visitASTExpr(expr, lvid);
	if (previous != 0) {
		scope.emitDebugpos(expr);
		ir::emit::type::common(out, lvid, previous);
	}
	previous = lvid;
	return ok;
}

} // namespace

bool Scope::visitASTExprTypeof(AST::Node& node, uint32_t& localvar) {
	assert(node.rule == AST::rgTypeof);
	uint32_t previous = 0;
	bool success = true;
	auto& irout = ircode();
	ir::Producer::Scope scope{*this};
	ir::emit::CodegenLocker codegen{irout};
	for (auto& child : node.children) {
		switch (child.rule) {
			case AST::rgCall: { // typeof
				for (auto& param : child.children) {
					if (param.rule == AST::rgCallParameter) {
						success &= appendSingleType(scope, param.firstChild(), irout, previous);
						break;
					}
				}
				break;
			}
			default:
				return unexpectedNode(child, "[typeof]");
		}
	}
	if (unlikely(previous == 0))
		success = (error(node) << "at least one type is required for 'typeof'");
	localvar = previous;
	return success;
}

} // namespace Producer
} // namespace ir
} // namespace ny
