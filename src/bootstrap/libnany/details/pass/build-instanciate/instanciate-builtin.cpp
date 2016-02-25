#include "instanciate.h"

using namespace Yuni;




namespace Nany
{
namespace Pass
{
namespace Instanciate
{

	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::stackalloc>& operands)
	{
		auto& frame = atomStack.back();
		if (not frame.verify(operands.lvid))
			return;

		auto& lvidinfo = frame.lvids[operands.lvid];
		lvidinfo.scope = frame.scope;

		nytype_t type = (nytype_t) operands.type;
		if (type != nyt_any)
		{
			assert(not atomStack.empty());
			auto& cdef  = cdeftable.classdef(CLID{frame.atomid, operands.lvid});
			if (cdef.isAny())
			{
				// type propagation
				auto& spare = cdeftable.substitute(operands.lvid);
				if (type != nyt_void)
				{
					lvidinfo.synthetic = false;
					spare.mutateToBuiltin(type);
				}
				else
					spare.mutateToVoid();
			}
		}

		// copy only variable instances
		if (canGenerateCode())
		{
			lvidinfo.offsetDeclOut = out.opcodeCount();
			out.emitStackalloc(operands.lvid, type);
		}
	}




} // namespace Instanciate
} // namespace Pass
} // namespace Nany
