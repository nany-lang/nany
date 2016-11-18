#include "instanciate.h"
#include "instanciate-error.h"

using namespace Yuni;




namespace ny
{
namespace Pass
{
namespace Instanciate
{

	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::typeisobject>& operands)
	{
		assert(frame != nullptr);

		bool ok = [&]() -> bool
		{
			if (not frame->verify(operands.lvid))
				return false;
			auto& cdef = cdeftable.classdef(CLID{frame->atomid, operands.lvid});
			if (likely(not cdef.isBuiltinOrVoid()))
			{
				auto* atom = cdeftable.findClassdefAtom(cdef);
				if (likely(nullptr != atom))
				{
					if (unlikely(atom->isClass() or atom->isFunction()))
					{
						if (canGenerateCode()) // checking for real object only when they exist
						{
							if (unlikely(atom->isClass() and not atom->classinfo.isInstanciated))
							return complain::classNotInstanciated(*atom);
						}
						return true;
					}
				}
			}
			return complain::classOrFuncExpected(cdef);
		}();

		if (unlikely(not ok))
			frame->invalidate(operands.lvid);
	}





} // namespace Instanciate
} // namespace Pass
} // namespace ny
