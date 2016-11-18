#include "thread-context.h"
#include "details/vm/types.h"
#include "stack.h"
#include "stacktrace.h"
#include "memchecker.h"
#include "dyncall/dyncall.h"
#include "details/ir/isa/data.h"
#include <iostream>
#include <csetjmp>
#include <limits>

using namespace Yuni;



//! Print opcodes executed by the vm
#define ny_vm_PRINT_OPCODES 0



#define vm_CHECK_POINTER(P,LVID)  if (YUNI_UNLIKELY(not memchecker.has((P)))) \
	{ \
		emitUnknownPointer((P)); /*assert(false and "invalid pointer");*/ \
	}



#if ny_vm_PRINT_OPCODES != 0
#define vm_PRINT_OPCODE(O)  do { std::cout << "== ny:vm +" \
	<< sequence.get().offsetOf(opr) << "  == "  \
	<< ny::IR::ISA::print(sequence.get(), opr, &map) << '\n';} while (0)
#else
#define vm_PRINT_OPCODE(O)
#endif

#define ASSERT_LVID(L)  assert((L) > 0 and (L) < registerCount)





namespace ny
{
namespace vm
{

	//! Pattern for memset alloc regions (debug)
	constexpr static const int patternAlloc = 0xCD;

	//! Pattern for memset free regions (debug)
	constexpr static const int patternFree = 0xCD;




	namespace {


	struct Executor final
	{
		nyallocator_t& allocator;
		DCCallVM* dyncall = nullptr;

		//! For C calls
		nyvm_t cfvm;
		//! Program configuratio
		nyprogram_cf_t cf;
		//! Current thread context
		ThreadContext& threadContext;
		//! Registers for the current stack frame
		DataRegister* registers = nullptr;
		//! Return value
		uint64_t retRegister = 0;
		//! Number of pushed parameters
		uint32_t funcparamCount = 0; // parameters are 2-based
		//! all pushed parameters
		DataRegister funcparams[Config::maxPushedParameters];
		Stack stack;
		//! Stack trace
		Stacktrace<true> stacktrace;
		//! Memory checker
		MemChecker<true> memchecker;
		//! upper label id encountered so far
		uint32_t upperLabelID = 0;
		//! Atom collection references
		const AtomMap& map;
		//! Source sequence
		std::reference_wrapper<const IR::Sequence> sequence;
		//! All user-defined intrinsics
		const IntrinsicTable& userDefinedIntrinsics;
		//! Reference to the current iterator
		const IR::Instruction** cursor = nullptr;
		//! Jump buffer, to handle exceptions during the execution of the program
		std::jmp_buf jump_buffer;
		#ifndef NDEBUG
		//! Total number of registers in the current frame
		uint32_t registerCount = 0;
		#endif

	public:
		Executor(ThreadContext& threadContext, const IR::Sequence& callee)
			: allocator(threadContext.program.cf.allocator)
			, cf(threadContext.program.cf)
			, threadContext(threadContext)
			, map(threadContext.program.map)
			, sequence(std::cref(callee))
			, userDefinedIntrinsics(ny::ref(threadContext.program.build).intrinsics)
		{
			// dynamic C calls
			dyncall = dcNewCallVM(4096);
			dcMode(dyncall, DC_CALL_C_DEFAULT);

			// prepare the current context for native C calls
			cfvm.allocator = &allocator;
			cfvm.program = threadContext.program.self();
			cfvm.tctx = threadContext.self();
			cfvm.console = &cf.console;
		}

		~Executor()
		{
			if (dyncall)
				dcFree(dyncall);
			// memory leaks ?
			memchecker.printLeaksIfAny(threadContext.cf);
		}

		[[noreturn]] void abortMission()
		{
			// dump information on the console
			stacktrace.dump(ny::ref(threadContext.program.build), map);
			// avoid spurious err messages and memory leaks when used
			// as scripting engine
			memchecker.releaseAll(allocator);
			// abort the execution of the program
			std::longjmp(jump_buffer, 666);
		}

		[[noreturn]] void emitBadAlloc()
		{
			threadContext.cerrException("failed to allocate memory");
			abortMission();
		}

		[[noreturn]] void emitPointerSizeMismatch(void* object, size_t size)
		{
			ShortString128 msg;
			msg << "pointer " << object << " size mismatch: got "
				<< size << " bytes, expected "
				<< memchecker.fetchObjectSize(reinterpret_cast<const uint64_t*>(object))
				<< " bytes";
			threadContext.cerrException(msg);
			abortMission();
		}

		[[noreturn]] void emitAssert()
		{
			threadContext.cerrException("assertion failed");
			abortMission();
		}

		[[noreturn]] void emitUnexpectedOpcode(const AnyString& name)
		{
			ShortString64 msg;
			msg << "error: vm: unexpected opcode '" << name << '\'';
			threadContext.cerrException(msg);
			abortMission();
		}

		[[noreturn]] void emitInvalidIntrinsicParamType()
		{
			threadContext.cerrException("intrinsic invalid parameter type");
			abortMission();
		}

		[[noreturn]] void emitInvalidReturnType()
		{
			threadContext.cerrException("intrinsic invalid return type");
			abortMission();
		}

		[[noreturn]] void emitDividedByZero()
		{
			threadContext.cerrException("division by zero");
			abortMission();
		}

		[[noreturn]] void emitUnknownPointer(void* p)
		{
			threadContext.cerrUnknownPointer(p, sequence.get().offsetOf(**cursor));
			abortMission();
		}

		[[noreturn]] void emitLabelError(uint32_t label)
		{
			ShortString256 msg;
			msg << "invalid label %" << label;
			msg << " (upper: %" << upperLabelID;
			msg << "), opcode: +" << sequence.get().offsetOf(**cursor);
			threadContext.cerrException(msg);
			abortMission();
		}


		template<class T> inline T* allocateraw(size_t size)
		{
			return (T*) allocator.allocate(&allocator, size);
		}

		inline void deallocate(void* object, size_t size)
		{
			assert(object != nullptr);
			allocator.deallocate(&allocator, object, size);
		}


		void destroy(uint64_t* object, uint32_t dtorid, uint32_t instanceid)
		{
			// the dtor function to call
			const Atom* dtor = map.findAtom(dtorid);
			assert(dtor != nullptr);

			if (false) // traces
			{
				std::cout << " .. DESTROY " << (void*) object << " aka '"
					<< dtor->caption() << "' at opc+" << sequence.get().offsetOf(**cursor) << '\n';
				stacktrace.dump(ny::ref(threadContext.program.build), map);
				std::cout << '\n';
			}

			// the parent class
			const Atom* classobject = dtor->parent;
			assert(classobject != nullptr);

			// its size
			uint64_t classsizeof = classobject->runtimeSizeof();
			classsizeof += Config::extraObjectSize;

			if (instanceid != (uint32_t) -1)
			{
				// reset parameters for func call
				funcparamCount = 1;
				funcparams[0].u64 = reinterpret_cast<uint64_t>(object); // self
				// func call
				call(0, dtor->atomid, instanceid);
			}

			if (debugmode)
				memset(object, patternFree, classsizeof);

			// sandbox release
			deallocate(object, static_cast<size_t>(classsizeof));
			memchecker.forget(object);
		}


		inline void gotoLabel(uint32_t label)
		{
			bool jmpsuccess = (label > upperLabelID)
				? sequence.get().jumpToLabelForward(*cursor, label)
				: sequence.get().jumpToLabelBackward(*cursor, label);
			if (unlikely(not jmpsuccess))
				emitLabelError(label);

			// the labels are strictly ordered
			upperLabelID = label;
		}


		// accept those opcode for debugging purposes
		inline void visit(const IR::ISA::Operand<IR::ISA::Op::comment>&) {}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::scope>&) {}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::end>&) {}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::nop>&) {}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::negation>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			registers[opr.lvid].u64 = not registers[opr.lhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::intrinsic>& opr)
		{
			vm_PRINT_OPCODE(opr);
			dcReset(dyncall);
			dcArgPointer(dyncall, &cfvm);

			auto& intrinsic = userDefinedIntrinsics[opr.iid];
			for (uint32_t i = 0; i != funcparamCount; ++i)
			{
				auto r = funcparams[i];
				switch (intrinsic.params[i])
				{
					case nyt_u64:
						dcArgLongLong(dyncall, static_cast<DClonglong>(r.u64));
						break;
					case nyt_i64:
						dcArgLongLong(dyncall, static_cast<DClonglong>(r.i64));
						break;
					case nyt_u32:
						dcArgInt(dyncall, static_cast<DCint>(r.u64));
						break;
					case nyt_i32:
						dcArgInt(dyncall, static_cast<DCint>(r.i64));
						break;
					case nyt_ptr:
						dcArgPointer(dyncall, reinterpret_cast<DCpointer>(r.u64));
						break;
					case nyt_u16:
						dcArgShort(dyncall, static_cast<DCshort>(r.u64));
						break;
					case nyt_i16:
						dcArgShort(dyncall, static_cast<DCshort>(r.i64));
						break;
					case nyt_u8:
						dcArgChar(dyncall, static_cast<DCchar>(r.u64));
						break;
					case nyt_i8:
						dcArgChar(dyncall, static_cast<DCchar>(r.i64));
						break;
					case nyt_f32:
						dcArgFloat(dyncall, static_cast<DCfloat>(r.f64));
						break;
					case nyt_f64:
						dcArgDouble(dyncall, static_cast<DCdouble>(r.f64));
						break;
					case nyt_bool:
						dcArgBool(dyncall, static_cast<DCbool>(r.u64));
						break;
					case nyt_void:
					case nyt_any:
						emitInvalidIntrinsicParamType();
				}
			}
			funcparamCount = 0;

			switch (intrinsic.rettype)
			{
				case nyt_u64:
					registers[opr.lvid].u64 = static_cast<uint64_t>(dcCallLongLong(dyncall, intrinsic.callback));
					break;
				case nyt_i64:
					registers[opr.lvid].i64 = static_cast<int64_t>(dcCallLongLong(dyncall, intrinsic.callback));
					break;
				case nyt_u32:
					registers[opr.lvid].u64 = static_cast<uint64_t>(dcCallInt(dyncall, intrinsic.callback));
					break;
				case nyt_i32:
					registers[opr.lvid].i64 = static_cast<int64_t>(dcCallInt(dyncall, intrinsic.callback));
					break;
				case nyt_ptr:
					registers[opr.lvid].u64 = reinterpret_cast<uint64_t>(dcCallPointer(dyncall, intrinsic.callback));
					break;
				case nyt_u16:
					registers[opr.lvid].u64 = static_cast<uint64_t>(dcCallShort(dyncall, intrinsic.callback));
					break;
				case nyt_i16:
					registers[opr.lvid].i64 = static_cast<int64_t>(dcCallShort(dyncall, intrinsic.callback));
					break;
				case nyt_u8:
					registers[opr.lvid].u64 = static_cast<uint64_t>(dcCallChar(dyncall, intrinsic.callback));
					break;
				case nyt_i8:
					registers[opr.lvid].i64 = static_cast<int64_t>(dcCallChar(dyncall, intrinsic.callback));
					break;
				case nyt_f32:
					registers[opr.lvid].f64 = static_cast<float>(dcCallFloat(dyncall, intrinsic.callback));
					break;
				case nyt_f64:
					registers[opr.lvid].f64 = static_cast<double>(dcCallDouble(dyncall, intrinsic.callback));
					break;
				case nyt_bool:
					registers[opr.lvid].u64 = (dcCallBool(dyncall, intrinsic.callback) ? 1 : 0);
					break;
				case nyt_void:
					dcCallVoid(dyncall, intrinsic.callback);
					break;
				case nyt_any:
					emitInvalidReturnType();
			}
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fadd>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].f64 = registers[opr.lhs].f64 + registers[opr.rhs].f64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fsub>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].f64 = registers[opr.lhs].f64 - registers[opr.rhs].f64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fmul>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].f64 = registers[opr.lhs].f64 * registers[opr.rhs].f64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fdiv>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			auto r = registers[opr.rhs].f64;
			if (YUNI_UNLIKELY((uint64_t)r == 0))
				emitDividedByZero();
			registers[opr.lvid].f64 = registers[opr.lhs].f64 / r;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::add>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 + registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::sub>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 - registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::mul>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 * registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::div>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			auto r = registers[opr.rhs].u64;
			if (YUNI_UNLIKELY(r == 0))
				emitDividedByZero();
			registers[opr.lvid].u64 = registers[opr.lhs].u64 / r;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::imul>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 =
				static_cast<uint64_t>(static_cast<int64_t>(registers[opr.lhs].u64) * static_cast<int64_t>(registers[opr.rhs].u64) );
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::idiv>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			auto r = static_cast<int64_t>(registers[opr.rhs].u64);
			if (YUNI_UNLIKELY(r == 0))
				emitDividedByZero();
			registers[opr.lvid].u64 = static_cast<uint64_t>(static_cast<int64_t>(registers[opr.lhs].u64) / r);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::eq>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 == registers[opr.rhs].u64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::neq>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 != registers[opr.rhs].u64) ? 1 : 0;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::lt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 < registers[opr.rhs].u64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::lte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 <= registers[opr.rhs].u64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::ilt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].i64 < registers[opr.rhs].i64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::ilte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].i64 <= registers[opr.rhs].i64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::gt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 > registers[opr.rhs].u64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::gte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].u64 >= registers[opr.rhs].u64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::igt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].i64 > registers[opr.rhs].i64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::igte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].i64 >= registers[opr.rhs].i64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::flt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].f64 < registers[opr.rhs].f64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::flte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].f64 <= registers[opr.rhs].f64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fgt>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].f64 > registers[opr.rhs].f64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fgte>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = (registers[opr.lhs].f64 >= registers[opr.rhs].f64) ? 1 : 0;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::opand>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 & registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::opor>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 | registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::opxor>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 ^ registers[opr.rhs].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::opmod>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lhs);
			ASSERT_LVID(opr.rhs);
			registers[opr.lvid].u64 = registers[opr.lhs].u64 % registers[opr.rhs].u64;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::push>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.lvid);
			funcparams[funcparamCount++].u64 = registers[opr.lvid].u64;
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::ret>& opr)
		{
			vm_PRINT_OPCODE(opr);
			assert(opr.lvid == 0 or opr.lvid < registerCount);
			retRegister = registers[opr.lvid].u64;
			sequence.get().invalidateCursor(*cursor);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::store>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.source);
			registers[opr.lvid] = registers[opr.source];
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::storeText>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			registers[opr.lvid].u64 =
				reinterpret_cast<uint64_t>(sequence.get().stringrefs[opr.text].c_str());
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::storeConstant>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			registers[opr.lvid].u64 = opr.value.u64;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::classdefsizeof>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			assert(map.findAtom(opr.type) != nullptr);
			registers[opr.lvid].u64 = map.findAtom(opr.type)->runtimeSizeof();
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::call>& opr)
		{
			ASSERT_LVID(opr.lvid);
			vm_PRINT_OPCODE(opr);
			call(opr.lvid, opr.ptr2func, opr.instanceid);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fieldset>& opr)
		{
			assert(opr.self < registerCount);
			ASSERT_LVID(opr.lvid);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.self].u64);
			vm_CHECK_POINTER(object, opr);
			object[1 + opr.var] = registers[opr.lvid].u64;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::fieldget>& opr)
		{
			ASSERT_LVID(opr.self);
			ASSERT_LVID(opr.lvid);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.self].u64);
			vm_CHECK_POINTER(object, opr);
			registers[opr.lvid].u64 = object[1 + opr.var];
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::label>& opr)
		{
			if (opr.label > upperLabelID)
				upperLabelID = opr.label;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::jmp>& opr)
		{
			gotoLabel(opr.label);
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::jnz>& opr)
		{
			if (registers[opr.lvid].u64 != 0)
			{
				registers[opr.result].u64 = 1;
				gotoLabel(opr.label);
			}
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::jz>& opr)
		{
			if (registers[opr.lvid].u64 == 0)
			{
				registers[opr.result].u64 = 0;
				gotoLabel(opr.label);
			}
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::ref>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			vm_CHECK_POINTER(object, opr);
			++(object[0]); // +ref
		}

		inline void visit(const IR::ISA::Operand<IR::ISA::Op::unref>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			vm_CHECK_POINTER(object, opr);
			if (0 == --(object[0])) // -unref
				destroy(object, opr.atomid, opr.instanceid);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::dispose>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			vm_CHECK_POINTER(object, opr);
			destroy(object, opr.atomid, opr.instanceid);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::stackalloc>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			(void) opr;
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::memalloc>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.regsize);

			size_t size;
			if (sizeof(size_t) < sizeof(uint64_t))
			{
				// size_t is less than a u64 (32bits platform probably)
				// so it is possible to ask more than the system can provide
				auto request = registers[opr.regsize].u64;
				if (std::numeric_limits<size_t>::max() <= request)
					return emitBadAlloc();
				size = static_cast<size_t>(request);
			}
			else
			{
				// 64bits platform
				size = static_cast<size_t>(registers[opr.regsize].u64);
			}

			size += Config::extraObjectSize;

			uint64_t* pointer = allocateraw<uint64_t>(size);
			if (YUNI_UNLIKELY(!pointer))
				return emitBadAlloc();
			if (debugmode)
				memset(pointer, patternAlloc, size);

			pointer[0] = 0; // init ref counter
			registers[opr.lvid].u64 = reinterpret_cast<uint64_t>(pointer);

			memchecker.hold(pointer, size, opr.lvid);
		}


		inline void visit(const IR::ISA::Operand<IR::ISA::Op::memrealloc>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.oldsize);
			ASSERT_LVID(opr.newsize);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);

			size_t oldsize = static_cast<size_t>(registers[opr.oldsize].u64);
			size_t newsize = static_cast<size_t>(registers[opr.newsize].u64);
			oldsize += Config::extraObjectSize;
			newsize += Config::extraObjectSize;

			if (object)
			{
				// checking the input pointer
				vm_CHECK_POINTER(object, opr);
				if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(oldsize))))
					return emitPointerSizeMismatch(object, oldsize);

				memchecker.forget(object);
			}

			auto* newptr = (uint64_t*) allocator.reallocate(&allocator, object, oldsize, newsize);
			registers[opr.lvid].u64 = reinterpret_cast<uintptr_t>(newptr);

			if (newptr)
			{
				// the nointer has been successfully reallocated - keeping a reference
				memchecker.hold(newptr, newsize, opr.lvid);
			}
			else
			{
				// `realloc` may not release the input pointer if the
				// allocation failed
				deallocate(object, oldsize);
				emitBadAlloc();
			}
		}


		void visit(const IR::ISA::Operand<IR::ISA::Op::memcheckhold>& opr)
		{
			uint64_t* ptr = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			uint64_t size = registers[opr.size].u64 + Config::extraObjectSize;
			memchecker.hold(ptr, size, opr.lvid);
		}


		void visit(const IR::ISA::Operand<IR::ISA::Op::memfree>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.regsize);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			if (object)
			{
				vm_CHECK_POINTER(object, opr);
				size_t size = static_cast<size_t>(registers[opr.regsize].u64);
				size += Config::extraObjectSize;

				if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(size))))
					return emitPointerSizeMismatch(object, size);

				if (debugmode)
					memset(object, patternFree, size);

				deallocate(object, size);
				memchecker.forget(object);
			}
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::memfill>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.regsize);
			ASSERT_LVID(opr.pattern);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			vm_CHECK_POINTER(object, opr);
			size_t size = static_cast<size_t>(registers[opr.regsize].u64);
			size += Config::extraObjectSize;
			uint8_t pattern = static_cast<uint8_t>(registers[opr.pattern].u64);

			if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(size))))
				return emitPointerSizeMismatch(object, size);

			memset(object, pattern, size);
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::memcopy>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.srclvid);
			ASSERT_LVID(opr.regsize);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			uint64_t* src = reinterpret_cast<uint64_t*>(registers[opr.srclvid].u64);

			size_t size = static_cast<size_t>(registers[opr.regsize].u64);
			memcpy(object, src, size);
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::memmove>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.srclvid);
			ASSERT_LVID(opr.regsize);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			uint64_t* src = reinterpret_cast<uint64_t*>(registers[opr.srclvid].u64);

			size_t size = static_cast<size_t>(registers[opr.regsize].u64);
			memmove(object, src, size);
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::memcmp>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.srclvid);
			ASSERT_LVID(opr.regsize);

			uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
			uint64_t* src = reinterpret_cast<uint64_t*>(registers[opr.srclvid].u64);

			size_t size = static_cast<size_t>(registers[opr.regsize].u64);
			int cmp = memcmp(object, src, size);
			registers[opr.regsize].u64 = (cmp == 0) ? 0 : ((cmp < 0) ? 2 : 1);
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::cstrlen>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptr);
			auto* cstring = reinterpret_cast<const char*>(registers[opr.ptr].u64);
			size_t clen = cstring ? strlen(cstring) : 0u;
			registers[opr.lvid].u64 = clen;
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::load_u64>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			registers[opr.lvid].u64 = *(reinterpret_cast<uint64_t*>(registers[opr.ptrlvid].u64));
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::load_u32>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			registers[opr.lvid].u64 = *(reinterpret_cast<uint32_t*>(registers[opr.ptrlvid].u64));
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::load_u8>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			registers[opr.lvid].u64 = *(reinterpret_cast<uint8_t*>(registers[opr.ptrlvid].u64));
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::store_u64>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			*(reinterpret_cast<uint64_t*>(registers[opr.ptrlvid].u64)) = registers[opr.lvid].u64;
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::store_u32>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			*(reinterpret_cast<uint32_t*>(registers[opr.ptrlvid].u64)) = static_cast<uint32_t>(registers[opr.lvid].u64);
		}

		void visit(const IR::ISA::Operand<IR::ISA::Op::store_u8>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);
			ASSERT_LVID(opr.ptrlvid);
			*(reinterpret_cast<uint8_t*>(registers[opr.ptrlvid].u64)) = static_cast<uint8_t>(registers[opr.lvid].u64);
		}


		void visit(const IR::ISA::Operand<IR::ISA::Op::opassert>& opr)
		{
			vm_PRINT_OPCODE(opr);
			ASSERT_LVID(opr.lvid);

			if (YUNI_UNLIKELY(registers[opr.lvid].u64 == 0))
				return emitAssert();
		}

		template<IR::ISA::Op O> void visit(const IR::ISA::Operand<O>& opr)
		{
			vm_PRINT_OPCODE(opr); // FALLBACK
			(void) opr; // unused
			return emitUnexpectedOpcode(IR::ISA::Operand<O>::opname());
		}


		uint64_t invoke(const IR::Sequence& callee)
		{
			const uint32_t framesize = callee.at<IR::ISA::Op::stacksize>(0).add;
			#ifndef NDEBUG
			assert(framesize < 1024 * 1024);
			registerCount = framesize;
			assert(callee.at<IR::ISA::Op::stacksize>(0).opcode == (uint32_t) IR::ISA::Op::stacksize);
			#endif

			registers = stack.push(framesize);
			registers[0].u64 = 0;
			sequence = std::cref(callee);
			upperLabelID = 0;

			// retrieve parameters for the func
			for (uint32_t i = 0; i != funcparamCount; ++i)
				registers[i + 2].u64 = funcparams[i].u64; // 2-based
			funcparamCount = 0;

			//
			// iterate through all opcodes
			//
			callee.each(*this, 1); // offset: 1, avoid blueprint pragma

			stack.pop(framesize);
			return retRegister;
		}


		void call(uint32_t retlvid, uint32_t atomfunc, uint32_t instanceid)
		{
			assert(retlvid == 0 or retlvid < registerCount);

			#if ny_vm_PRINT_OPCODES != 0
			std::cout << "== ny:vm >>  registers before call\n";
			for (uint32_t r = 0; r != registerCount; ++r)
				std::cout << "== ny:vm >>     reg %" << r << " = " << registers[r].u64 << "\n";
			std::cout << "== ny:vm >>  entering func atom:" << atomfunc
				<< ", instance: " << instanceid << '\n';
			#endif

			// save the current stack frame
			auto* storestackptr = registers;
			auto storesequence = sequence;
			auto* storecursor = cursor;
			#ifndef NDEBUG
			auto  storestckfrmsize = registerCount;
			#endif
			auto labelid = upperLabelID;

			uint32_t memcheckPreviousAtomid = memchecker.atomid();
			stacktrace.push(atomfunc, instanceid);

			// call
			uint64_t ret = invoke(map.sequence(atomfunc, instanceid));

			// restore the previous stack frame and store the result of the call
			upperLabelID = labelid;
			registers = storestackptr;
			registers[retlvid].u64 = ret;
			sequence = storesequence;
			cursor = storecursor;
			#ifndef NDEBUG
			registerCount = storestckfrmsize;
			#endif
			stacktrace.pop();
			memchecker.atomid(memcheckPreviousAtomid);

			#if ny_vm_PRINT_OPCODES != 0
			std::cout << "== ny:vm <<  returned from func call\n";
			#endif
		}

	}; // struct Executor


	} // anonymous namespace




	bool ThreadContext::invoke(uint64_t& exitstatus, const IR::Sequence& callee, uint32_t atomid, uint32_t instanceid)
	{
		// if something happens
		exitstatus = static_cast<uint64_t>(-1);

		if (!cf.on_thread_create
			or nyfalse != cf.on_thread_create(program.self(), self(), nullptr, name.c_str(), name.size()))
		{
			Executor executor{*this, callee};
			executor.stacktrace.push(atomid, instanceid);
			if (setjmp(executor.jump_buffer) != 666)
			{
				executor.invoke(callee);
				exitstatus = executor.retRegister;

				if (cf.on_thread_destroy)
					cf.on_thread_destroy(program.self(), self());
				return true;
			}
			else
			{
				// execution of the program failed
			}

			if (cf.on_thread_destroy)
				cf.on_thread_destroy(program.self(), self());
		}
		return false;
	}




} // namespace vm
} // namespace ny
