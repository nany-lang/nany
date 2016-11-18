#include "instanciate.h"

using namespace Yuni;




namespace ny
{
namespace Pass
{
namespace Instanciate
{

	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::stackalloc>& operands)
	{
		assert(frame != nullptr);
		if (not frame->verify(operands.lvid))
			return;

		auto& lvidinfo = frame->lvids[operands.lvid];
		lvidinfo.scope = frame->scope;

		nytype_t type = static_cast<nytype_t>(operands.type);

		// reset the underlying type, to make sure that the current layer has
		// the accurate information
		auto& spare = cdeftable.substitute(operands.lvid);
		switch (type)
		{
			case nyt_any:
			{
				spare.mutateToAny();
				break;
			}
			default:
			{
				lvidinfo.synthetic = false;
				spare.mutateToBuiltin(type);
				break;
			}
			case nyt_void:
			{
				spare.mutateToVoid();
				break;
			}
		}

		// copy only variable instances
		if (canGenerateCode())
		{
			lvidinfo.offsetDeclOut = out->opcodeCount();
			out->emitStackalloc(operands.lvid, type);
		}
	}




} // namespace Instanciate
} // namespace Pass
} // namespace ny
