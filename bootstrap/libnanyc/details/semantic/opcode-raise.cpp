#include "semantic-analysis.h"
#include "details/semantic/ref-unref.h"

using namespace Yuni;

namespace ny::semantic {

void Analyzer::visit(const ir::isa::Operand<ir::isa::Op::raise>& operands) {
	auto& localfunc = frame->atom;
	if (unlikely(not localfunc.isFunction()))
		return (void)(error() << "errors can only be raised from a function body");
	if (unlikely(localfunc.isDtor()))
		return (void)(error() << "'raise' not allowed in destructor");
	auto& cdef  = cdeftable.classdef(CLID{frame->atomid, operands.lvid});
	auto* atomError = cdeftable.findClassdefAtom(cdef);
	if (atomError == nullptr)
		return (void)(error() << "only user-defined classes can be used for raising an error");
	ir::emit::trace(out, "begin 'raise'");
	uint32_t labelid = 0;
	if (onScopeFail.empty()) {
		// not within an error handler defined by the current function
		frame->atom.funcinfo.raisedErrors.add(*atomError, frame->atom, currentLine, currentOffset);
		ir::emit::ref(out, operands.lvid);
		releaseAllScopedVariables();
	}
	else {
		// within an error handler defined by the function
		auto* handler = onScopeFail.find(atomError);
		if (unlikely(handler == nullptr))
			return complainNoErrorHandler(*atomError, nullptr, {});
		handler->used = true;
		labelid = handler->label;
		if (*handler != onScopeFail.any())
			ir::emit::ref(out, operands.lvid);
		releaseScopedVariables(handler->scope);
	}
	++codeGenerationLock;
	tryUnrefObject(*this, operands.lvid); // ensure the presence of the dtor in 'Atom::classinfo'
	--codeGenerationLock;
	ir::emit::raise(out, operands.lvid, labelid, atomError->atomid);
}

} // ny::semantic
