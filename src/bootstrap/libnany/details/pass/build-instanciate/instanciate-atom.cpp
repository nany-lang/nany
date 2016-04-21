#include "instanciate.h"
#include <memory>
#include "details/reporting/report.h"
#include "details/atom/func-overload-match.h"
#include "details/reporting/message.h"
#include "details/utils/origin.h"
#include "details/pass/build-attach-program/mapping.h"
#include "libnany-traces.h"
#include "instanciate-atom.h"
#include <iostream>

using namespace Yuni;






namespace Nany
{
namespace Pass
{
namespace Instanciate
{


	bool SequenceBuilder::instanciateAtomClassClone(Atom& atom, uint32_t lvid, uint32_t rhs)
	{
		assert(atom.isClass());
		if (unlikely(atom.flags(Atom::Flags::error)))
			return false;

		// first, try to find the user-defined clone function (if any)
		Atom* userDefinedClone = nullptr;
		switch (atom.findFuncAtom(userDefinedClone, "^clone"))
		{
			case 0:  break;
			case 1:
			{
				assert(userDefinedClone != nullptr);
				uint32_t instanceid = (uint32_t) -1;
				if (not instanciateAtomFunc(instanceid, (*userDefinedClone), /*void*/0, /*self*/lvid, rhs))
					return false;
				break;
			}
			default: return complainMultipleDefinitions(atom, "operator 'clone'");
		}


		Atom* clone = nullptr;
		switch (atom.findFuncAtom(clone, "^obj-clone"))
		{
			case 1:
			{
				assert(clone != nullptr);
				uint32_t instanceid = (uint32_t) -1;
				if (instanciateAtomFunc(instanceid, (*clone), /*void*/0, /*self*/lvid, rhs))
				{
					atom.classinfo.clone.atomid     = clone->atomid;
					atom.classinfo.clone.instanceid = instanceid;
					return true;
				}
				break;
			}
			case 0:
			{
				return complainMissingOperator(atom, "clone");
			}
			default: return complainMultipleDefinitions(atom, "operator 'obj-clone'");
		}
		return false;
	}


	bool SequenceBuilder::instanciateAtomClassDestructor(Atom& atom, uint32_t lvid)
	{
		// if the IR code produced when transforming the AST is invalid,
		// a common scenario is that the code tries to destroy something (via unref)
		// which is not be a real class
		if (unlikely(not atom.isClass()))
			return (ICE() << "trying to call the destructor of a non-class atom");

		if (unlikely(atom.flags(Atom::Flags::error)))
			return false;

		// first, try to find the user-defined dtor function (if any)
		Atom* userDefinedDtor = nullptr;
		switch (atom.findFuncAtom(userDefinedDtor, "^dispose"))
		{
			case 0: break;
			case 1:
			{
				assert(userDefinedDtor != nullptr);
				uint32_t instanceid = static_cast<uint32_t>(-1);
				if (not instanciateAtomFunc(instanceid, (*userDefinedDtor), /*void*/0, /*self*/lvid))
					return false;
				break;
			}
			default: return complainMultipleDefinitions(atom, "operator 'dispose'");
		}

		Atom* dtor = nullptr;
		switch (atom.findFuncAtom(dtor, "^obj-dispose"))
		{
			case 1:
			{
				assert(dtor != nullptr);
				uint32_t instanceid = static_cast<uint32_t>(-1);
				if (instanciateAtomFunc(instanceid, (*dtor), /*void*/0, /*self*/lvid))
				{
					atom.classinfo.dtor.atomid     = dtor->atomid;
					atom.classinfo.dtor.instanceid = instanceid;
					return true;
				}
				break;
			}
			case 0:
			{
				return complainMissingOperator(atom, "obj-dispose");
			}
			default: return complainMultipleDefinitions(atom, "operator 'obj-dispose'");
		}
		return false;
	}


	Atom* SequenceBuilder::instanciateAtomClass(Atom& atom)
	{
		assert(atom.isClass());

		if (atom.tmplparams.empty())
			atom.classinfo.isInstanciated = true;


		overloadMatch.clear(); // reset

		for (auto& param: pushedparams.gentypes.indexed)
			overloadMatch.input.tmplparams.indexed.emplace_back(frame->atomid, param.lvid);

		// disable any hint or error report for now, to avoid spurious messages
		// if there is no error.
		overloadMatch.canGenerateReport = false;

		TypeCheck::Match match = overloadMatch.validate(atom);
		if (unlikely(TypeCheck::Match::none == match))
		{
			// fail - try again to produce error message, hint, and any suggestion
			auto err = (error() << "cannot instanciate '" << atom.keyword() << ' ');
			atom.retrieveFullname(err.data().message);
			err << '\'';
			overloadMatch.canGenerateReport = true;
			overloadMatch.report = std::ref(err);
			overloadMatch.validate(atom);
			return nullptr;
		}

		decltype(FuncOverloadMatch::result.params) params;
		decltype(FuncOverloadMatch::result.params) tmplparams;
		params.swap(overloadMatch.result.params);
		tmplparams.swap(overloadMatch.result.tmplparams);

		Logs::Message::Ptr newReport;

		// determine captured variables before instanciating the class
		if (!!atom.candidatesForCapture)
			captureVariables(atom);


		Pass::Instanciate::InstanciateData info{newReport, atom, cdeftable, build, params, tmplparams};
		info.parentAtom = &(frame->atom);
		info.shouldMergeLayer = true;
		info.parent = this;

		auto* sequence = Pass::Instanciate::InstanciateAtom(info);
		report.subgroup().appendEntry(newReport);

		// !!
		// !! The target atom may have changed here (for generic classes)
		// !!
		auto& resAtom = info.atom.get();

		if (sequence != nullptr)
		{
			// the user may already have provided one or more constructors
			// but if not the case, the default implementatio will be used instead
			if (not resAtom.hasMember("^new"))
				resAtom.renameChild("^default-new", "^new");
			return &resAtom;
		}

		atom.flags += Atom::Flags::error;
		resAtom.flags += Atom::Flags::error;
		return nullptr;
	}


	bool SequenceBuilder::instanciateAtomFunc(uint32_t& instanceid, Atom& funcAtom, uint32_t retlvid, uint32_t p1, uint32_t p2)
	{
		assert(funcAtom.isFunction());
		assert(frame != nullptr);
		auto& frameAtom = frame->atom;

		if (unlikely((p1 != 0 and not frame->verify(p1)) or (p2 != 0 and not frame->verify(p2))))
		{
			if (retlvid != 0)
				frame->invalidate(retlvid);
			return false;
		}

		overloadMatch.clear(); // reset
		if (p1 != 0) // first parameter
		{
			overloadMatch.input.params.indexed.emplace_back(frameAtom.atomid, p1);

			if (p2 != 0) // second parameter
				overloadMatch.input.params.indexed.emplace_back(frameAtom.atomid, p2);
		}

		if (retlvid != 0) // return value
			overloadMatch.input.rettype.push_back(CLID{frameAtom.atomid, retlvid});

		// disable any hint or error report for now, to avoid spurious messages
		// if there is no error.
		overloadMatch.canGenerateReport = false;

		TypeCheck::Match match = overloadMatch.validate(funcAtom);
		if (unlikely(TypeCheck::Match::none == match))
		{
			// fail - try again to produce error message, hint, and any suggestion
			auto err = (error() << "cannot call '" << funcAtom.keyword() << ' ');
			funcAtom.retrieveFullname(err.data().message);
			err << '\'';
			overloadMatch.canGenerateReport = true;
			overloadMatch.report = std::ref(err);
			overloadMatch.validate(funcAtom);
			if (retlvid != 0)
				frame->invalidate(retlvid);
			return false;
		}

		decltype(FuncOverloadMatch::result.params) params;
		decltype(FuncOverloadMatch::result.params) tmplparams;
		params.swap(overloadMatch.result.params);
		tmplparams.swap(overloadMatch.result.tmplparams);

		// instanciate the called func
		Logs::Message::Ptr subreport;
		InstanciateData info{subreport, funcAtom, cdeftable, build, params, tmplparams};
		bool instok = doInstanciateAtomFunc(subreport, info, retlvid);
		instanceid = info.instanceid;

		if (unlikely(not instok and retlvid != 0))
			frame->invalidate(retlvid);
		return instok;
	}


	bool SequenceBuilder::doInstanciateAtomFunc(Logs::Message::Ptr& subreport, InstanciateData& info, uint32_t retlvid)
	{
		// even within a typeof, any new instanciation must see their code generated
		// (and its errors generated)
		info.canGenerateCode = true; // canGenerateCode();
		info.parent = this;

		auto* sequence = InstanciateAtom(info);
		report.appendEntry(subreport);
		if (unlikely(nullptr == sequence))
			return (success = false);

		if (unlikely(info.instanceid == static_cast<uint32_t>(-1)))
		{
			ICE(info.returnType, "return: invalid instance id");
			return false;
		}

		// import the return type of the instanciated sequence
		auto& spare = cdeftable.substitute(retlvid);
		spare.kind  = info.returnType.kind;
		spare.atom  = info.returnType.atom;
		if (not info.returnType.isVoid())
		{
			spare.instance = true; // force some values just in case
			if (unlikely(not spare.isBuiltinOrVoid() and spare.atom == nullptr))
			{
				ICE() << "return: invalid atom for return type";
				return false;
			}
		}
		return true;
	}


	inline void SequenceBuilder::pushParametersFromSignature(LVID atomid, const Signature& signature)
	{
		assert(frame == NULL);
		assert(atomid != 0);
		// magic constant +2
		//  * +1: all clid are 1-based (0 is reserved for the atom itself, not for an internal var)
		//  * +1: the CLID{X, 1} is reserved for the return type

		// unused pseudo/invalid register
		CLID clid{atomid, 0};
		cdeftable.addSubstitute(nyt_void, nullptr, Qualifiers()); // unused, 1-based

		// redefine return type {atomid,1}
		clid.reclass(1);
		auto& rettype = cdeftable.rawclassdef(clid);
		assert(atomid == rettype.clid.atomid());
		Atom* atom = likely(not rettype.isBuiltinOrVoid()) ? cdeftable.findRawClassdefAtom(rettype) : nullptr;
		cdeftable.addSubstitute(rettype.kind, atom, rettype.qualifiers);

		// adding parameters
		uint32_t count = signature.parameters.size();
		for (uint32_t i = 0; i != count; ++i)
		{
			auto& param = signature.parameters[i];
			cdeftable.addSubstitute(param.kind, param.atom, param.qualifiers);
		}
		// adding reserved variables for cloning parameters (after normal parameters)
		for (uint32_t i = 0; i != count; ++i)
		{
			auto& param = signature.parameters[i];
			cdeftable.addSubstitute(param.kind, param.atom, param.qualifiers);
		}

		// template parameters
		count = signature.tmplparams.size();
		for (uint32_t i = 0; i != count; ++i)
		{
			auto& param = signature.tmplparams[i];
			cdeftable.addSubstitute(param.kind, param.atom, param.qualifiers);
		}
	}



	namespace // anonymous
	{

		struct PostProcessStackAllocWalker final
		{
			PostProcessStackAllocWalker(ClassdefTableView& table, uint32_t atomid)
				: table(table)
				, atomid(atomid)
			{}

			void visit(IR::ISA::Operand<IR::ISA::Op::stackalloc>& opc)
			{
				auto& cdef = table.classdef(CLID{atomid, opc.lvid});
				if (not cdef.isBuiltinOrVoid())
				{
					auto* atom = table.findClassdefAtom(cdef);
					if (atom != nullptr)
					{
						assert(opc.atomid == 0 or opc.atomid == (uint32_t) -1 or opc.atomid == atom->atomid);
						opc.type = static_cast<uint32_t>(nyt_pointer);
						opc.atomid = atom->atomid;
					}
				}
				else
					opc.type = static_cast<uint32_t>(cdef.kind);
			}

			template<IR::ISA::Op O> void visit(const IR::ISA::Operand<O>&)
			{
				// nothing to do
			}

			ClassdefTableView& table;
			uint32_t atomid;
			IR::Instruction** cursor = nullptr;
		};

		static void reinitStackAllocTypes(IR::Sequence& out, ClassdefTableView& table, uint32_t atomid)
		{
			PostProcessStackAllocWalker walker{table, atomid};
			out.each(walker);
		}


		template<class R>
		static inline void printGeneratedIRSequence(R& report, const String& symbolName,
			const IR::Sequence& out, const ClassdefTableView& newView)
		{
			report.info();
			auto trace = report.subgroup();
			auto entry = trace.trace();
			entry.message.prefix << symbolName;

			String text;
			out.print(text, &newView.atoms());
			text.replace("\n", "\n    ");
			text.trimRight();
			trace.trace() << "{\n    " << text << "\n}";
			trace.info(); // for beauty
			trace.info(); // for beauty
			trace.info(); // for beauty
		}


		static inline void prepareSignature(Signature& signature, InstanciateData& info)
		{
			uint32_t count = static_cast<uint32_t>(info.params.size());
			if (count != 0)
			{
				signature.parameters.resize(count);

				for (uint32_t i = 0; i != count; ++i)
				{
					assert(info.params[i].cdef != nullptr);
					auto& cdef  = *(info.params[i].cdef);
					auto& param = signature.parameters[i];

					param.atom = const_cast<Atom*>(info.cdeftable.findClassdefAtom(cdef));
					param.kind = cdef.kind;
					param.qualifiers = cdef.qualifiers;
				}
			}

			count = static_cast<uint32_t>(info.tmplparams.size());
			if (count != 0)
			{
				signature.tmplparams.resize(count);

				for (uint32_t i = 0; i != count; ++i)
				{
					assert(info.tmplparams[i].cdef != nullptr);
					auto& cdef  = *(info.tmplparams[i].cdef);
					auto& param = signature.tmplparams[i];

					param.atom = const_cast<Atom*>(info.cdeftable.findClassdefAtom(cdef));
					param.kind = cdef.kind;
					param.qualifiers = cdef.qualifiers;
				}
			}
		}


		static bool createNewAtom(InstanciateData& info, Atom& atom, Logs::Report& report)
		{
			auto& sequence  = *atom.opcodes.sequence;
			auto& cdeftable = info.cdeftable.originalTable();
			// the mutex is useless here since the code instanciation is mono-threaded
			// but it's required by the mapping (which must be thread-safe in the first passes)
			// TODO remove this mutex
			Mutex mutex;

			Pass::Mapping::SequenceMapping mapper{cdeftable, mutex, report, sequence};
			mapper.evaluateWholeSequence = false;
			mapper.prefixNameForFirstAtomCreated = "^";

			auto& parentAtom = *atom.parent;
			mapper.map(parentAtom, atom.opcodes.offset);

			if (unlikely(!mapper.firstAtomCreated))
				return (report.error() << "failed to remap atom '" << atom.caption() << "'");

			assert(info.atom.get().atomid != mapper.firstAtomCreated->atomid);
			assert(&info.atom.get() != mapper.firstAtomCreated);
			info.atom = std::ref(*mapper.firstAtomCreated);

			// the generic parameters are fully resolved now, avoid
			// any new attempt to check them
			auto& newAtom = info.atom.get();
			newAtom.tmplparamsForPrinting.swap(newAtom.tmplparams);
			return true;
		}


		static IR::Sequence* performAtomInstanciation(InstanciateData& info, Signature& signature)
		{
			// No IR sequence attached for the given signature,
			// let's instanciate the function or the class !

			// reporting for the current instanciation
			info.report = new Logs::Message(Logs::Level::none); // TODO remove alloc for reporting
			Logs::Report report{*info.report};

			auto& previousAtom = info.atom.get();
			if (unlikely(!previousAtom.opcodes.sequence or !previousAtom.parent))
			{
				report.ICE() << "invalid atom";
				return nullptr;
			}

			// creaa new atom branch for the new instanciation if the atom is a generic class
			if (previousAtom.isClass() and previousAtom.hasGenericParameters())
			{
				if (not createNewAtom(info, previousAtom, report))
					return nullptr;
			}


			// the current atom
			auto& atom = info.atom.get();
			// the new IR sequence for the instanciated function
			auto outIR = std::make_unique<IR::Sequence>();
			// the original IR sequence generated from the AST
			auto& inputIR = *(atom.opcodes.sequence);

			// new layer for the cdeftable
			ClassdefTableView newView{info.cdeftable, atom.atomid, signature.parameters.size()};

			// instanciate the sequence attached to the atom
			auto builder = std::make_unique<SequenceBuilder>
				(report.subgroup(), newView, info.build, *outIR, inputIR, info.parent);

			builder->pushParametersFromSignature(atom.atomid, signature);
			if (info.parentAtom)
				builder->layerDepthLimit = 2; // allow the first one

			// Read the input IR sequence, resolve all types, and generate
			// a new IR sequence ready for execution ! (with or without optimization passes)
			// (everything happens here)
			bool success = builder->readAndInstanciate(atom.opcodes.offset);


			// post-processing regarding 'stackalloc' opcodes
			// those opcodes may not have the good declared type (most likely
			// something like 'any')
			// (always update even if sometimes not necessary, easier for debugging)
			reinitStackAllocTypes(*outIR, newView, atom.atomid);

			// keep all deduced types
			if (likely(success) and info.shouldMergeLayer)
				newView.mergeSubstitutes();

			// Generating the full human readable name of the symbol with the
			// apropriate types & qualifiers for all parameters
			// (example: "func A.foo(b: cref __i32): ref __i32")
			String symbolName;
			if (success or Config::Traces::printGeneratedOpcodeSequence)
			{
				symbolName << newView.keyword(info.atom) << ' '; // ex: func
				atom.retrieveCaption(symbolName, newView);  // ex: A.foo(...)...
			}
			if (Config::Traces::printGeneratedOpcodeSequence)
				printGeneratedIRSequence(report, symbolName, *outIR.get(), newView);

			if (success)
			{
				if (atom.isFunction())
				{
					// if the IR code belongs to a function, get the return type
					// and put it into the signature (for later reuse) and into
					// the 'info' structure for immediate use
					auto& cdefReturn = newView.classdef(CLID{atom.atomid, 1});
					if (not cdefReturn.isBuiltinOrVoid())
					{
						auto* atom = newView.findClassdefAtom(cdefReturn);
						if (atom)
						{
							signature.returnType.mutateToAtom(atom);
							info.returnType.mutateToAtom(atom);
						}
						else
						{
							report.ICE() << "invalid atom pointer in func return type for '"
								<< symbolName << '\'';
							success = false;
						}
					}
					else
					{
						signature.returnType.kind = cdefReturn.kind;
						info.returnType.kind = cdefReturn.kind;
					}
				}
				else
				{
					// not a function, so no return value (unlikely to be used anyway)
					signature.returnType.mutateToVoid();
					info.returnType.mutateToVoid();
				}

				if (likely(success))
				{
					info.instanceid = atom.assignInstance(signature, outIR.get(), symbolName, nullptr);
					return outIR.release();
				}
			}

			// failed to instanciate the input IR sequence. This can be expected, if trying
			// to not instanciate the appropriate function (if several overloads are present
			// for example). Anyway, remember that this signature is a 'no-go'.
			info.instanceid = atom.assignInvalidInstance(signature);
			return nullptr;
		}


	} // anonymous namespace




	IR::Sequence* InstanciateAtom(InstanciateData& info)
	{
		// prepare the matching signature
		Signature signature;
		prepareSignature(signature, info);
		assert(info.params.size() == signature.parameters.size());

		// try to pick an existing instanciation
		{
			IR::Sequence* sequence = nullptr;
			auto& atom = info.atom.get();
			Atom* remapAtom;
			uint32_t ix = atom.findInstance(sequence, remapAtom, signature);
			if (ix != static_cast<uint32_t>(-1))
			{
				info.returnType.import(signature.returnType);
				info.instanceid = ix;
				if (remapAtom)
					info.atom = std::ref(*remapAtom);
				return sequence;
			}
		}

		// instanciate the function
		return performAtomInstanciation(info, signature);
	}






} // namespace Instanciate
} // namespace Pass
} // namespace Nany
