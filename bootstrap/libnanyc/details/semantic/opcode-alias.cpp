#include "semantic-analysis.h"
#include "deprecated-error.h"
#include "ref-unref.h"

using namespace Yuni;

namespace ny::semantic {

void Analyzer::declareNamedVariable(const AnyString& name, uint32_t lvid, bool autoreleased) {
	assert(frame != nullptr);
	if (unlikely(not frame->verify(lvid)))
		return;
	if (unlikely(name.empty())) {
		ice() << "got empty variable name";
		return frame->invalidate(lvid);
	}
	auto& lr = frame->lvids(lvid);
	lr.file.line   = currentLine;
	lr.file.offset = currentOffset;
	lr.file.url    = currentFilename;
	uint32_t previousDecl = frame->findLocalVariable(name);
	if (likely(0 == previousDecl)) { // not found
		lr.scope           = frame->scope;
		lr.userDefinedName = name;
		lr.hasBeenUsed     = false;
		if (name[0] == '%')
			lr.warning.unused = false;
		lr.autorelease = autoreleased
			// If an error has already been reported on this lvid, we
			// should not try to release this variable later, since it can
			// be anything and probably something that can not be released
			and frame->verify(lvid)
			// and of course if the data can be acquired
			and canBeAcquired(*this, cdeftable.classdef(CLID{frame->atomid, lvid}));
	}
	else {
		lr.errorReported = true;
		frame->invalidate(lvid);
		complain::redeclared(name, previousDecl);
	}
}

// inline void Analyzer::visit(const ir::isa::Operand<ir::isa::Op::namealias>& operands)
// see .h

} // ny::semantic
