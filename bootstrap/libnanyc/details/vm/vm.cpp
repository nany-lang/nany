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
		<< ny::ir::ISA::print(sequence.get(), opr, &map) << '\n';} while (0)
#else
#define vm_PRINT_OPCODE(O)
#endif

#define ASSERT_LVID(L)  assert((L) > 0 and (L) < registerCount)


namespace ny {
namespace vm {


//! Pattern for memset alloc regions (debug)
constexpr static const int patternAlloc = 0xCD;
//! Pattern for memset free regions (debug)
constexpr static const int patternFree = 0xCD;


namespace {


struct Executor final {
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
	DataRegister funcparams[config::maxPushedParameters];
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
	std::reference_wrapper<const ir::Sequence> sequence;
	//! All user-defined intrinsics
	const IntrinsicTable& userDefinedIntrinsics;
	//! Reference to the current iterator
	const ir::Instruction** cursor = nullptr;
	//! Jump buffer, to handle exceptions during the execution of the program
	std::jmp_buf jump_buffer;
	#ifndef NDEBUG
	//! Total number of registers in the current frame
	uint32_t registerCount = 0;
	#endif

public:
	Executor(ThreadContext& threadContext, const ir::Sequence& callee) noexcept
		: allocator(threadContext.program.cf.allocator)
		, cf(threadContext.program.cf)
		, threadContext(threadContext)
		, map(threadContext.program.map)
		, sequence(std::cref(callee))
		, userDefinedIntrinsics(ny::ref(threadContext.program.build).intrinsics) {
		// dynamic C calls
		dyncall = dcNewCallVM(4096);
		dcMode(dyncall, DC_CALL_C_DEFAULT);
		// prepare the current context for native C calls
		cfvm.allocator = &allocator;
		cfvm.program = threadContext.program.self();
		cfvm.tctx = threadContext.self();
		cfvm.console = &cf.console;
	}

	~Executor() noexcept {
		if (dyncall)
			dcFree(dyncall);
		// memory leaks ?
		memchecker.printLeaksIfAny(threadContext.cf);
	}


	[[noreturn]] void abortMission() noexcept {
		// dump information on the console
		stacktrace.dump(ny::ref(threadContext.program.build), map);
		// avoid spurious err messages and memory leaks when used
		// as scripting engine
		memchecker.releaseAll(allocator);
		// abort the execution of the program
		std::longjmp(jump_buffer, 666);
	}


	[[noreturn]] void emitBadAlloc() noexcept {
		threadContext.cerrException("failed to allocate memory");
		abortMission();
	}


	[[noreturn]] void emitPointerSizeMismatch(void* object, size_t size) noexcept {
		ShortString128 msg;
		msg << "pointer " << object << " size mismatch: got "
			<< size << " bytes, expected "
			<< memchecker.fetchObjectSize(reinterpret_cast<const uint64_t*>(object))
			<< " bytes";
		threadContext.cerrException(msg);
		abortMission();
	}


	[[noreturn]] void emitAssert() noexcept {
		threadContext.cerrException("assertion failed");
		abortMission();
	}


	[[noreturn]] void emitUnexpectedOpcode(const AnyString& name) noexcept {
		ShortString64 msg;
		msg << "error: vm: unexpected opcode '" << name << '\'';
		threadContext.cerrException(msg);
		abortMission();
	}


	[[noreturn]] void emitInvalidIntrinsicParamType() noexcept {
		threadContext.cerrException("intrinsic invalid parameter type");
		abortMission();
	}


	[[noreturn]] void emitInvalidReturnType() noexcept {
		threadContext.cerrException("intrinsic invalid return type");
		abortMission();
	}


	[[noreturn]] void emitDividedByZero() noexcept {
		threadContext.cerrException("division by zero");
		abortMission();
	}


	[[noreturn]] void emitUnknownPointer(void* p) noexcept {
		threadContext.cerrUnknownPointer(p, sequence.get().offsetOf(**cursor));
		abortMission();
	}


	[[noreturn]] void emitLabelError(uint32_t label) noexcept {
		ShortString256 msg;
		msg << "invalid label %" << label;
		msg << " (upper: %" << upperLabelID;
		msg << "), opcode: +" << sequence.get().offsetOf(**cursor);
		threadContext.cerrException(msg);
		abortMission();
	}


	template<class T> inline T* allocateraw(size_t size) noexcept {
		return (T*) allocator.allocate(&allocator, size);
	}


	inline void deallocate(void* object, size_t size) noexcept {
		assert(object != nullptr);
		allocator.deallocate(&allocator, object, size);
	}


	void destroy(uint64_t* object, uint32_t dtorid, uint32_t instanceid) noexcept {
		// the dtor function to call
		auto& dtor = *map.findAtom(dtorid);
		if (false) { // traces
			std::cout << " .. DESTROY " << (void*) object << " aka '"
					  << dtor.caption() << "' at opc+" << sequence.get().offsetOf(**cursor) << '\n';
			stacktrace.dump(ny::ref(threadContext.program.build), map);
			std::cout << '\n';
		}
		// the parent class
		auto* classobject = dtor.parent;
		assert(classobject != nullptr);
		// its size
		uint64_t classsizeof = classobject->runtimeSizeof();
		classsizeof += config::extraObjectSize;
		if (instanceid != (uint32_t) - 1) {
			// reset parameters for func call
			funcparamCount = 1;
			funcparams[0].u64 = reinterpret_cast<uint64_t>(object); // self
			// func call
			call(0, dtor.atomid, instanceid);
		}
		if (debugmode)
			memset(object, patternFree, classsizeof);
		// sandbox release
		deallocate(object, static_cast<size_t>(classsizeof));
		memchecker.forget(object);
	}


	inline void gotoLabel(uint32_t label) noexcept {
		bool jmpsuccess = (label > upperLabelID)
						  ? sequence.get().jumpToLabelForward(*cursor, label)
						  : sequence.get().jumpToLabelBackward(*cursor, label);
		if (unlikely(not jmpsuccess))
			emitLabelError(label);
		// the labels are strictly ordered
		upperLabelID = label;
	}


	// accept those opcode for debugging purposes
	inline void visit(const ir::ISA::Operand<ir::ISA::Op::comment>&) {}
	inline void visit(const ir::ISA::Operand<ir::ISA::Op::scope>&) {}
	inline void visit(const ir::ISA::Operand<ir::ISA::Op::end>&) {}
	inline void visit(const ir::ISA::Operand<ir::ISA::Op::nop>&) {}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::negation>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		registers[opr.lvid].u64 = not registers[opr.lhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::intrinsic>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		dcReset(dyncall);
		dcArgPointer(dyncall, &cfvm);
		auto& intrinsic = userDefinedIntrinsics[opr.iid];
		for (uint32_t i = 0; i != funcparamCount; ++i) {
			auto r = funcparams[i];
			switch (intrinsic.params[i]) {
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
		switch (intrinsic.rettype) {
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


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fadd>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].f64 = registers[opr.lhs].f64 + registers[opr.rhs].f64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fsub>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].f64 = registers[opr.lhs].f64 - registers[opr.rhs].f64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fmul>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].f64 = registers[opr.lhs].f64 * registers[opr.rhs].f64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fdiv>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		auto r = registers[opr.rhs].f64;
		if (YUNI_UNLIKELY((uint64_t)r == 0))
			emitDividedByZero();
		registers[opr.lvid].f64 = registers[opr.lhs].f64 / r;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::add>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 + registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::sub>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 - registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::mul>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 * registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::div>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		auto r = registers[opr.rhs].u64;
		if (YUNI_UNLIKELY(r == 0))
			emitDividedByZero();
		registers[opr.lvid].u64 = registers[opr.lhs].u64 / r;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::imul>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 =
			static_cast<uint64_t>(static_cast<int64_t>(registers[opr.lhs].u64) * static_cast<int64_t>
				(registers[opr.rhs].u64) );
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::idiv>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		auto r = static_cast<int64_t>(registers[opr.rhs].u64);
		if (YUNI_UNLIKELY(r == 0))
			emitDividedByZero();
		registers[opr.lvid].u64 = static_cast<uint64_t>(static_cast<int64_t>(registers[opr.lhs].u64) / r);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::eq>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 == registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::neq>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 != registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::lt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 < registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::lte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 <= registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::ilt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].i64 < registers[opr.rhs].i64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::ilte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].i64 <= registers[opr.rhs].i64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::gt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 > registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::gte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].u64 >= registers[opr.rhs].u64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::igt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].i64 > registers[opr.rhs].i64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::igte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].i64 >= registers[opr.rhs].i64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::flt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].f64 < registers[opr.rhs].f64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::flte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].f64 <= registers[opr.rhs].f64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fgt>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].f64 > registers[opr.rhs].f64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fgte>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = (registers[opr.lhs].f64 >= registers[opr.rhs].f64) ? 1 : 0;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::opand>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 & registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::opor>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 | registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::opxor>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 ^ registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::opmod>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lhs);
		ASSERT_LVID(opr.rhs);
		registers[opr.lvid].u64 = registers[opr.lhs].u64 % registers[opr.rhs].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::push>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.lvid);
		funcparams[funcparamCount++].u64 = registers[opr.lvid].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::ret>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		assert(opr.lvid == 0 or opr.lvid < registerCount);
		retRegister = registers[opr.lvid].u64;
		sequence.get().invalidateCursor(*cursor);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::store>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.source);
		registers[opr.lvid] = registers[opr.source];
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::storeText>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		registers[opr.lvid].u64 =
			reinterpret_cast<uint64_t>(sequence.get().stringrefs[opr.text].c_str());
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::storeConstant>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		registers[opr.lvid].u64 = opr.value.u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::classdefsizeof>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		assert(map.findAtom(opr.type) != nullptr);
		registers[opr.lvid].u64 = map.findAtom(opr.type)->runtimeSizeof();
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::call>& opr) noexcept {
		ASSERT_LVID(opr.lvid);
		vm_PRINT_OPCODE(opr);
		call(opr.lvid, opr.ptr2func, opr.instanceid);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fieldset>& opr) noexcept {
		assert(opr.self < registerCount);
		ASSERT_LVID(opr.lvid);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.self].u64);
		vm_CHECK_POINTER(object, opr);
		object[1 + opr.var] = registers[opr.lvid].u64;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::fieldget>& opr) noexcept {
		ASSERT_LVID(opr.self);
		ASSERT_LVID(opr.lvid);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.self].u64);
		vm_CHECK_POINTER(object, opr);
		registers[opr.lvid].u64 = object[1 + opr.var];
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::label>& opr) noexcept {
		if (opr.label > upperLabelID)
			upperLabelID = opr.label;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::jmp>& opr) noexcept {
		gotoLabel(opr.label);
	}

	inline void visit(const ir::ISA::Operand<ir::ISA::Op::jnz>& opr) noexcept {
		if (registers[opr.lvid].u64 != 0) {
			registers[opr.result].u64 = 1;
			gotoLabel(opr.label);
		}
	}

	inline void visit(const ir::ISA::Operand<ir::ISA::Op::jz>& opr) noexcept {
		if (registers[opr.lvid].u64 == 0) {
			registers[opr.result].u64 = 0;
			gotoLabel(opr.label);
		}
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::ref>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		vm_CHECK_POINTER(object, opr);
		++(object[0]); // +ref
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::unref>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		vm_CHECK_POINTER(object, opr);
		if (0 == --(object[0])) // -unref
			destroy(object, opr.atomid, opr.instanceid);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::dispose>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		vm_CHECK_POINTER(object, opr);
		destroy(object, opr.atomid, opr.instanceid);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::stackalloc>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		(void) opr;
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::memalloc>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.regsize);
		size_t size;
		if (sizeof(size_t) < sizeof(uint64_t)) {
			// size_t is less than a u64 (32bits platform probably)
			// so it is possible to ask more than the system can provide
			auto request = registers[opr.regsize].u64;
			if (std::numeric_limits<size_t>::max() <= request)
				return emitBadAlloc();
			size = static_cast<size_t>(request);
		}
		else {
			// 64bits platform
			size = static_cast<size_t>(registers[opr.regsize].u64);
		}
		size += config::extraObjectSize;
		uint64_t* pointer = allocateraw<uint64_t>(size);
		if (YUNI_UNLIKELY(!pointer))
			return emitBadAlloc();
		if (debugmode)
			memset(pointer, patternAlloc, size);
		pointer[0] = 0; // init ref counter
		registers[opr.lvid].u64 = reinterpret_cast<uint64_t>(pointer);
		memchecker.hold(pointer, size, opr.lvid);
	}


	inline void visit(const ir::ISA::Operand<ir::ISA::Op::memrealloc>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.oldsize);
		ASSERT_LVID(opr.newsize);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		size_t oldsize = static_cast<size_t>(registers[opr.oldsize].u64);
		size_t newsize = static_cast<size_t>(registers[opr.newsize].u64);
		oldsize += config::extraObjectSize;
		newsize += config::extraObjectSize;
		if (object) {
			// checking the input pointer
			vm_CHECK_POINTER(object, opr);
			if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(oldsize))))
				return emitPointerSizeMismatch(object, oldsize);
			memchecker.forget(object);
		}
		auto* newptr = (uint64_t*) allocator.reallocate(&allocator, object, oldsize, newsize);
		registers[opr.lvid].u64 = reinterpret_cast<uintptr_t>(newptr);
		if (newptr) {
			// the nointer has been successfully reallocated - keeping a reference
			memchecker.hold(newptr, newsize, opr.lvid);
		}
		else {
			// `realloc` may not release the input pointer if the
			// allocation failed
			deallocate(object, oldsize);
			emitBadAlloc();
		}
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memcheckhold>& opr) noexcept {
		uint64_t* ptr = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		uint64_t size = registers[opr.size].u64 + config::extraObjectSize;
		memchecker.hold(ptr, size, opr.lvid);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memfree>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.regsize);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		if (object) {
			vm_CHECK_POINTER(object, opr);
			size_t size = static_cast<size_t>(registers[opr.regsize].u64);
			size += config::extraObjectSize;
			if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(size))))
				return emitPointerSizeMismatch(object, size);
			if (debugmode)
				memset(object, patternFree, size);
			deallocate(object, size);
			memchecker.forget(object);
		}
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memfill>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.regsize);
		ASSERT_LVID(opr.pattern);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		vm_CHECK_POINTER(object, opr);
		size_t size = static_cast<size_t>(registers[opr.regsize].u64);
		size += config::extraObjectSize;
		uint8_t pattern = static_cast<uint8_t>(registers[opr.pattern].u64);
		if (YUNI_UNLIKELY(not memchecker.checkObjectSize(object, static_cast<size_t>(size))))
			return emitPointerSizeMismatch(object, size);
		memset(object, pattern, size);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memcopy>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.srclvid);
		ASSERT_LVID(opr.regsize);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		uint64_t* src = reinterpret_cast<uint64_t*>(registers[opr.srclvid].u64);
		size_t size = static_cast<size_t>(registers[opr.regsize].u64);
		memcpy(object, src, size);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memmove>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.srclvid);
		ASSERT_LVID(opr.regsize);
		uint64_t* object = reinterpret_cast<uint64_t*>(registers[opr.lvid].u64);
		uint64_t* src = reinterpret_cast<uint64_t*>(registers[opr.srclvid].u64);
		size_t size = static_cast<size_t>(registers[opr.regsize].u64);
		memmove(object, src, size);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::memcmp>& opr) noexcept {
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


	void visit(const ir::ISA::Operand<ir::ISA::Op::cstrlen>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptr);
		auto* cstring = reinterpret_cast<const char*>(registers[opr.ptr].u64);
		size_t clen = cstring ? strlen(cstring) : 0u;
		registers[opr.lvid].u64 = clen;
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::load_u64>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		registers[opr.lvid].u64 = *(reinterpret_cast<uint64_t*>(registers[opr.ptrlvid].u64));
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::load_u32>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		registers[opr.lvid].u64 = *(reinterpret_cast<uint32_t*>(registers[opr.ptrlvid].u64));
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::load_u8>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		registers[opr.lvid].u64 = *(reinterpret_cast<uint8_t*>(registers[opr.ptrlvid].u64));
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::store_u64>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		*(reinterpret_cast<uint64_t*>(registers[opr.ptrlvid].u64)) = registers[opr.lvid].u64;
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::store_u32>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		*(reinterpret_cast<uint32_t*>(registers[opr.ptrlvid].u64)) = static_cast<uint32_t>(registers[opr.lvid].u64);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::store_u8>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		ASSERT_LVID(opr.ptrlvid);
		*(reinterpret_cast<uint8_t*>(registers[opr.ptrlvid].u64)) = static_cast<uint8_t>(registers[opr.lvid].u64);
	}


	void visit(const ir::ISA::Operand<ir::ISA::Op::opassert>& opr) noexcept {
		vm_PRINT_OPCODE(opr);
		ASSERT_LVID(opr.lvid);
		if (YUNI_UNLIKELY(registers[opr.lvid].u64 == 0))
			return emitAssert();
	}

	template<ir::ISA::Op O> void visit(const ir::ISA::Operand<O>& opr) noexcept {
		vm_PRINT_OPCODE(opr); // FALLBACK
		(void) opr; // unused
		return emitUnexpectedOpcode(ir::ISA::Operand<O>::opname());
	}


	uint64_t invoke(const ir::Sequence& callee) noexcept {
		const uint32_t framesize = callee.at<ir::ISA::Op::stacksize>(0).add;
		#ifndef NDEBUG
		assert(framesize < 1024 * 1024);
		registerCount = framesize;
		assert(callee.at<ir::ISA::Op::stacksize>(0).opcode == (uint32_t) ir::ISA::Op::stacksize);
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


	void call(uint32_t retlvid, uint32_t atomfunc, uint32_t instanceid) noexcept {
		assert(retlvid == 0 or retlvid < registerCount);
		#if ny_vm_PRINT_OPCODES != 0
		std::cout << "== ny:vm >>  registers before call\n";
		for (uint32_t r = 0; r != registerCount; ++r)
			std::cout << "== ny:vm >>     reg %" << r << " = " << registers[r].u64 << "\n";
		std::cout << "== ny:vm >>  entering func atom:" << atomfunc;
		std::cout << ", instance: " << instanceid << '\n';
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


bool ThreadContext::invoke(uint64_t& exitstatus, const ir::Sequence& callee, uint32_t atomid,
		uint32_t instanceid) {
	// if something happens
	exitstatus = static_cast<uint64_t>(-1);
	if (!cf.on_thread_create
		or nyfalse != cf.on_thread_create(program.self(), self(), nullptr, name.c_str(), name.size())) {
		Executor executor{*this, callee};
		executor.stacktrace.push(atomid, instanceid);
		if (setjmp(executor.jump_buffer) != 666) {
			executor.invoke(callee);
			exitstatus = executor.retRegister;
			if (cf.on_thread_destroy)
				cf.on_thread_destroy(program.self(), self());
			return true;
		}
		else {
			// execution of the program failed
		}
		if (cf.on_thread_destroy)
			cf.on_thread_destroy(program.self(), self());
	}
	return false;
}


} // namespace vm
} // namespace ny
