#include "semantic-analysis.h"

using namespace Yuni;

namespace ny::semantic {

void Analyzer::visit(const ir::isa::Operand<ir::isa::Op::stackalloc>& operands) {
	assert(frame != nullptr);
	if (not frame->verify(operands.lvid))
		return;
	auto& lvidinfo = frame->lvids(operands.lvid);
	lvidinfo.scope = frame->scope;
	CType type  = static_cast<CType>(operands.type);
	// reset the underlying type, to make sure that the current layer has
	// the accurate information
	auto& spare = cdeftable.substitute(operands.lvid);
	switch (type) {
		case CType::t_any: {
			spare.mutateToAny();
			break;
		}
		default: {
			lvidinfo.synthetic = false;
			spare.mutateToBuiltin(type);
			break;
		}
		case CType::t_void: {
			spare.mutateToVoid();
			break;
		}
	}
	if (canGenerateCode()) { // copy only variable instances
		lvidinfo.offsetDeclOut = out->opcodeCount();
		ir::emit::alloc(out, operands.lvid, type);
	}
}

} // ny::semantic
