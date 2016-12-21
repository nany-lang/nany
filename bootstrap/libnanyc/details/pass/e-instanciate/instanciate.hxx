#pragma once
#include "instanciate.h"
#include "details/ir/emit.h"


namespace ny {
namespace Pass {
namespace Instanciate {


inline void SequenceBuilder::pushNewFrame(Atom& atom) {
	auto* newframe = build.allocate<AtomStackFrame>(atom, frame);
	frame = newframe;
}


inline void SequenceBuilder::popFrame() {
	auto* previous = frame->previous;
	build.deallocate(frame);
	frame = previous;
}


inline bool SequenceBuilder::canBeAcquired(const Classdef& cdef) const {
	bool success = not cdef.isBuiltinOrVoid();
	if (yuni::debugmode) {
		if (unlikely(success and cdeftable.findClassdefAtom(cdef) == nullptr)) {
			// do not generate any error if already reported
			if (frame->atomid != cdef.clid.atomid() or frame->verify(cdef.clid.lvid()))
				return (ice() << "canBeAcquired: invalid atom " << cdef.clid << ", " << cdef.print(cdeftable));
		}
	}
	return success;
}


inline bool SequenceBuilder::canBeAcquired(const CLID& clid) const {
	return canBeAcquired(cdeftable.classdef(clid));
}


inline bool SequenceBuilder::canBeAcquired(uint32_t lvid) const {
	assert(frame != nullptr);
	return canBeAcquired(CLID{frame->atomid, lvid});
}


inline bool SequenceBuilder::canGenerateCode() const {
	return (codeGenerationLock == 0);
}


inline bool SequenceBuilder::checkForIntrinsicParamCount(const AnyString& name, uint32_t count) {
	return (pushedparams.func.indexed.size() == count)
		or complainIntrinsicParameterCount(name, count);
}


inline void SequenceBuilder::acquireObject(uint32_t lvid) {
	assert(lvid > 1 and "can not acquire the returned value");
	assert(canBeAcquired(lvid));
	ir::emit::ref(out, lvid);
	frame->lvids(lvid).autorelease = true;
}


inline void SequenceBuilder::tryToAcquireObject(uint32_t lvid) {
	if (canBeAcquired(lvid))
		acquireObject(lvid);
}


template<class T> inline void SequenceBuilder::tryToAcquireObject(uint32_t lvid, const T& type) {
	if (canBeAcquired(type))
		acquireObject(lvid);
}


} // namespace Instanciate
} // namespace Pass
} // namespace ny
