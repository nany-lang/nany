#pragma once
#include "sequence.h"
#include "details/ir/isa/data.h"


namespace ny
{
namespace ir
{
namespace emit
{
namespace
{

	struct SequenceRef final {
		SequenceRef(Sequence& sequence) :sequence(sequence) {}
		SequenceRef(Sequence* sequence) :sequence(*sequence) {
			assert(sequence != nullptr);
		}

		Sequence& sequence;
	};


	inline void nop(SequenceRef ref) {
		ref.sequence.emit<ISA::Op::nop>();
	}


	//! Copy two register
	inline void copy(SequenceRef ref, uint32_t lvid, uint32_t source) {
		auto& operands = ref.sequence.emit<ISA::Op::store>();
		operands.lvid = lvid;
		operands.source = source;
	}


	inline void constantu64(SequenceRef ref, uint32_t lvid, uint64_t value) {
		auto& operands = ref.sequence.emit<ISA::Op::storeConstant>();
		operands.lvid = lvid;
		operands.value.u64 = value;
	}


	inline void constantf64(SequenceRef ref, uint32_t lvid, double value) {
		auto& operands = ref.sequence.emit<ISA::Op::storeConstant>();
		operands.lvid  = lvid;
		operands.value.f64 = value;
	}


	inline void constantbool(SequenceRef ref, uint32_t lvid, bool value) {
		constantu64(ref, lvid, value ? 1 : 0);
	}


	inline uint32_t constantText(SequenceRef ref, uint32_t lvid, const AnyString& text) {
		auto& operands = ref.sequence.emit<ISA::Op::storeText>();
		operands.lvid = lvid;
		operands.text = ref.sequence.stringrefs.ref(text);
		return operands.text;
	}


	//! Allocate a new variable on the stack and get the register
	inline uint32_t alloc(SequenceRef ref, uint32_t lvid, nytype_t type = nyt_any) {
		auto& operands  = ref.sequence.emit<ISA::Op::stackalloc>();
		operands.lvid   = lvid;
		operands.type   = static_cast<uint32_t>(type);
		operands.atomid = (uint32_t) -1;
		return lvid;
	}


	//! Allocate a new variable on the stack and assign a value to it and get the register
	inline uint32_t allocu64(SequenceRef ref, uint32_t lvid, nytype_t type, uint64_t value) {
		ir::emit::alloc(ref, lvid, type);
		ir::emit::constantu64(ref, lvid, value);
		return lvid;
	}
	

	//! Allocate a new variable on the stack and assign a value to it and get the register
	inline uint32_t allocf64(SequenceRef ref, uint32_t lvid, nytype_t type, double value) {
		ir::emit::alloc(ref, lvid, type);
		ir::emit::constantf64(ref, lvid, value);
		return lvid;
	}


	//! Allocate a new variable on the stack and assign a text to it and get the register
	inline uint32_t alloctext(SequenceRef ref, uint32_t lvid, const AnyString& text) {
		ir::emit::alloc(ref, lvid, nyt_ptr);
		ir::emit::constantText(ref, lvid, text);
		return lvid;
	}


	inline void cassert(SequenceRef ref, uint32_t lvid) {
		ref.sequence.emit<ISA::Op::opassert>().lvid = lvid;
	}


	inline uint32_t objectAlloc(SequenceRef ref, uint32_t lvid, uint32_t atomid) {
		auto& operands = ref.sequence.emit<ISA::Op::allocate>();
		operands.lvid = lvid;
		operands.atomid = atomid;
		return lvid;
	}


	inline void ref(SequenceRef ref, uint32_t lvid) {
		ref.sequence.emit<ISA::Op::ref>().lvid = lvid;
	}


	inline void unref(SequenceRef ref, uint32_t lvid, uint32_t atomid, uint32_t instanceid) {
		auto& operands = ref.sequence.emit<ISA::Op::unref>();
		operands.lvid = lvid;
		operands.atomid = atomid;
		operands.instanceid = instanceid;
	}


	inline void scopeBegin(SequenceRef ref) {
		ref.sequence.emit<ISA::Op::scope>();
	}


	inline void scopeEnd(SequenceRef ref) {
		ref.sequence.emit<ISA::Op::end>();
	}


	inline uint32_t label(SequenceRef ref, uint32_t labelid) {
		ref.sequence.emit<ISA::Op::label>().label = labelid;
		return labelid;
	}


	//! Unconditional jump
	inline void jmp(SequenceRef ref, uint32_t label) {
		ref.sequence.emit<ISA::Op::jmp>().label = label;
	}


	//! jump if zero
	inline void jz(SequenceRef ref, uint32_t lvid, uint32_t result, uint32_t label) {
		auto& opc  = ref.sequence.emit<ISA::Op::jz>();
		opc.lvid   = lvid;
		opc.result = result;
		opc.label  = label;
	}


	//! jump if not zero
	inline void jnz(SequenceRef ref, uint32_t lvid, uint32_t result, uint32_t label) {
		auto& opc  = ref.sequence.emit<ISA::Op::jnz>();
		opc.lvid   = lvid;
		opc.result = result;
		opc.label  = label;
	}


	inline void identify(SequenceRef ref, uint32_t lvid, const AnyString& name, uint32_t self) {
		auto& sequence = ref.sequence;
		auto& operands = sequence.emit<ISA::Op::identify>();
		operands.lvid  = lvid;
		operands.self  = self;
		operands.text  = sequence.stringrefs.ref(name);
	}


	inline void ensureResolvedType(SequenceRef ref, uint32_t lvid) {
		ref.sequence.emit<ISA::Op::ensureresolved>().lvid = lvid;
	}


	inline void namealias(SequenceRef ref, uint32_t lvid, const AnyString& name) {
		auto& sequence = ref.sequence;
		auto& operands = sequence.emit<ISA::Op::namealias>();
		operands.lvid  = lvid;
		operands.name  = sequence.stringrefs.ref(name);
	}


	//! Return with no value
	inline void ret(SequenceRef ref) {
		auto& operands   = ref.sequence.emit<ISA::Op::ret>();
		operands.lvid    = 0;
		operands.tmplvid = 0;
	}


	//! Return from value
	inline void ret(SequenceRef ref, uint32_t lvid, uint32_t tmplvid) {
		auto& operands   = ref.sequence.emit<ISA::Op::ret>();
		operands.lvid    = lvid;
		operands.tmplvid = tmplvid;
	}


	template<class T> struct TraceWriter final {
		static void emit(SequenceRef ref, const T& value) {
			auto& sequence = ref.sequence;
			sequence.emit<ISA::Op::comment>().text = sequence.stringrefs.ref(value());
		}
	};


	template<uint32_t N>
	struct TraceWriter<char[N]> final {
		static void emit(SequenceRef ref, const char* value) {
			auto& sequence = ref.sequence;
			sequence.emit<ISA::Op::comment>().text = sequence.stringrefs.ref(AnyString{value, N});
		}
	};


	//! Insert a comment in the IR code
	template<class T>
	inline void trace(SequenceRef ref, const T& value) {
		//! ir::emit::trace(out, [](){return "some comments here";});
		if (yuni::debugmode)
			TraceWriter<T>::emit(std::move(ref), value);
	}


	//! Insert a comment in the IR code
	template<class T>
	inline void trace(SequenceRef ref, bool condition, const T& value) {
		//! ir::emit::trace(out, [](){return "some comments here";});
		if (yuni::debugmode and condition)
			TraceWriter<T>::emit(std::move(ref), value);
	}


	//! Insert empty comment line in the IR code
	inline void trace(SequenceRef ref) {
		if (yuni::debugmode)
			ref.sequence.emit<ISA::Op::comment>().text = 0;
	}


} // namespace
} // namespace emit
} // namespace ir
} // namespace ny


namespace ny
{
namespace ir
{
namespace emit
{
namespace memory
{
namespace
{


	inline uint32_t allocate(SequenceRef ref, uint32_t lvid, uint32_t regsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memalloc>();
		operands.lvid    = lvid;
		operands.regsize = regsize;
		return lvid;
	}


	inline void reallocate(SequenceRef ref, uint32_t lvid, uint32_t oldsize, uint32_t newsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memrealloc>();
		operands.lvid    = lvid;
		operands.oldsize = oldsize;
		operands.newsize = newsize;
	}


	inline void dispose(SequenceRef ref, uint32_t lvid, uint32_t regsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memfree>();
		operands.lvid    = lvid;
		operands.regsize = regsize;
	}


	inline void fill(SequenceRef ref, uint32_t lvid, uint32_t regsize, uint32_t pattern) {
		auto& operands   = ref.sequence.emit<ISA::Op::memfill>();
		operands.lvid    = lvid;
		operands.regsize = regsize;
		operands.pattern = pattern;
	}


	inline void copyNoOverlap(SequenceRef ref, uint32_t lvid, uint32_t srclvid, uint32_t regsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memcopy>();
		operands.lvid    = lvid;
		operands.srclvid = srclvid;
		operands.regsize = regsize;
	}


	inline void copy(SequenceRef ref, uint32_t lvid, uint32_t srclvid, uint32_t regsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memmove>();
		operands.lvid    = lvid;
		operands.srclvid = srclvid;
		operands.regsize = regsize;
	}


	inline void compare(SequenceRef ref, uint32_t lvid, uint32_t srclvid, uint32_t regsize) {
		auto& operands   = ref.sequence.emit<ISA::Op::memcmp>();
		operands.lvid    = lvid;
		operands.srclvid = srclvid;
		operands.regsize = regsize;
	}


	inline void cstrlen(SequenceRef ref, uint32_t lvid, uint32_t bits, uint32_t ptr) {
		auto& operands = ref.sequence.emit<ISA::Op::cstrlen>();
		operands.lvid  = lvid;
		operands.bits  = bits;
		operands.ptr   = ptr;
	}


} // namespace
} // namespace memory
} // namespace emit
} // namespace ir
} // namespace ny


namespace ny
{
namespace ir
{
namespace emit
{
namespace dbginfo
{
namespace
{


	//! Emit a debug filename opcode
	inline void filename(SequenceRef ref, const AnyString& path) {
		auto& sequence = ref.sequence;
		sequence.emit<ISA::Op::debugfile>().filename = sequence.stringrefs.ref(path);
	}


	inline void position(SequenceRef ref, uint32_t line, uint32_t offset) {
		auto& operands  = ref.sequence.emit<ISA::Op::debugpos>();
		operands.line   = line;
		operands.offset = offset;
	}


} // namespace
} // namespace dbginfo
} // namespace emit
} // namespace ir
} // namespace ny
