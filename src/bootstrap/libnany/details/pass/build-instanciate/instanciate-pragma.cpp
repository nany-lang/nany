#include "instanciate.h"

using namespace Yuni;




namespace Nany
{
namespace Pass
{
namespace Instanciate
{


	void SequenceBuilder::pragmaBodyStart()
	{
		assert(frame != nullptr);
		auto& atom = frame->atom;
		if (atom.isFunction())
		{
			//
			// cloning all function parameters (only non-ref parameters)
			//
			uint32_t count = atom.parameters.size();
			atom.parameters.each([&](uint32_t i, const AnyString& name, const Vardef& vardef)
			{
				if (name[0] == '^') // captured
					return;

				// lvid for the given parameter
				uint32_t lvid  = i + 1 + 1; // 1: return type, 2: first parameter
				assert(lvid < frame->lvids.size());
				frame->lvids[lvid].synthetic = false;

				if (not cdeftable.classdef(vardef.clid).qualifiers.ref)
				{
					if (debugmode and canGenerateCode())
						out.emitComment(String{} << "\n ----- ---- deep copy parameter " << i);

					// a register has already been reserved for cloning parameters
					uint32_t clone = 2 + count + i; // 1: return type, 2: first parameter
					assert(clone < frame->lvids.size());
					// the new value is not synthetic
					frame->lvids[clone].synthetic = false;

					bool r = instanciateAssignment(*frame, clone, lvid, false, false, true);
					if (unlikely(not r))
						frame->invalidate(clone);

					if (canBeAcquired(lvid))
					{
						frame->lvids[lvid].autorelease = true;
						frame->lvids[clone].autorelease = false;
					}

					if (canGenerateCode())
					{
						out.emitStore(lvid, clone); // register swap
						if (debugmode)
							out.emitComment("--\n");
					}
				}
			});

			// generating some code on the fly
			if (atom.isOperator() and canGenerateCode())
			{
				// variables initialization (for a ctor)
				if (generateClassVarsAutoInit)
				{
					generateClassVarsAutoInit = false;
					generateMemberVarDefaultInitialization();
				}

				// variables destruction (for dtor)
				if (generateClassVarsAutoRelease)
				{
					generateClassVarsAutoRelease = false;
					generateMemberVarDefaultDispose();
				}

				// variables cloning (copy a ctor)
				if (generateClassVarsAutoClone)
				{
					generateClassVarsAutoClone = false;
					generateMemberVarDefaultClone();
				}
			}
		}
	}


	void SequenceBuilder::visit(const IR::ISA::Operand<IR::ISA::Op::pragma>& operands)
	{
		assert(static_cast<uint32_t>(operands.pragma) < static_cast<uint32_t>(IR::ISA::Pragma::max));

		auto pragma = static_cast<IR::ISA::Pragma>(operands.pragma);
		switch (pragma)
		{
			case IR::ISA::Pragma::codegen:
			{
				if (operands.value.codegen != 0)
				{
					if (codeGenerationLock > 0) // re-enable code generation
						--codeGenerationLock;
				}
				else
					++codeGenerationLock;
				break;
			}

			case IR::ISA::Pragma::bodystart:
			{
				pragmaBodyStart();
				break;
			}

			case IR::ISA::Pragma::blueprintsize:
			{
				if (0 == layerDepthLimit)
				{
					// ignore the current blueprint
					//*cursor += operands.value.blueprintsize;
					assert(frame != nullptr);
					assert(frame->offsetOpcodeBlueprint != (uint32_t) -1);
					auto startOffset = frame->offsetOpcodeBlueprint;
					auto count = operands.value.blueprintsize;

					if (unlikely(count < 3))
					{
						ice() << "invalid blueprint size when instanciating atom";
						break;
					}

					// goto the end of the blueprint
					// -2: the final 'end' opcode must be interpreted
					*cursor = &currentSequence.at(startOffset + count - 1 - 1);
				}
				break;
			}

			case IR::ISA::Pragma::visibility:
			{
				assert(frame != nullptr);
				break;
			}

			case IR::ISA::Pragma::shortcircuitOpNopOffset:
			{
				shortcircuit.label = operands.value.shortcircuitMetadata.label;
				break;
			}

			case IR::ISA::Pragma::shortcircuitMutateToBool:
			{
				uint32_t lvid = operands.value.shortcircuitMutate.lvid;
				uint32_t source = operands.value.shortcircuitMutate.source;

				frame->lvids[lvid].synthetic = false;

				if (true)
				{
					auto& instr = *(*cursor - 1);
					assert(instr.opcodes[0] == static_cast<uint32_t>(IR::ISA::Op::stackalloc));
					uint32_t sizeoflvid = instr.to<IR::ISA::Op::stackalloc>().lvid;

					// sizeof
					auto& atombool = *(cdeftable.atoms().core.object[nyt_bool]);
					out.emitSizeof(sizeoflvid, atombool.atomid);

					auto& opc = cdeftable.substitute(lvid);
					opc.mutateToAtom(&atombool);
					opc.qualifiers.ref = true;

					// ALLOC: memory allocation of the new temporary object
					out.emitMemalloc(lvid, sizeoflvid);
					out.emitRef(lvid);
					frame->lvids[lvid].autorelease = true;
					// reset the internal value of the object
					out.emitFieldset(source, /*self*/lvid, 0); // builtin
				}
				else
					out.emitStore(lvid, source);
				break;
			}

			case IR::ISA::Pragma::synthetic:
			{
				uint32_t lvid = operands.value.synthetic.lvid;
				bool onoff = (operands.value.synthetic.onoff != 0);
				frame->lvids[lvid].synthetic = onoff;
				break;
			}

			case IR::ISA::Pragma::suggest:
			case IR::ISA::Pragma::builtinalias:
			case IR::ISA::Pragma::shortcircuit:
			case IR::ISA::Pragma::unknown:
			case IR::ISA::Pragma::max:
				break;
		}
	}






} // namespace Instanciate
} // namespace Pass
} // namespace Nany
