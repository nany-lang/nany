#include "semantic-analysis.h"
#include "details/ir/emit.h"
#include "ref-unref.h"

using namespace Yuni;

namespace ny::semantic {

namespace {

enum class AssignStrategy {
	rawregister,
	ref,
	deepcopy,
};

bool complainInvalidLvid() {
	ice() << "invalid lvid for variable assignment";
	return false;
}

bool complainSyntheticObjectsAreImmutable(uint32_t atomid, uint32_t lhs, uint32_t rhs) {
	auto e = error() << "synthetic objects are immutable";
	if (debugmode)
		e << ' ' << CLID{atomid, lhs} << " = " << CLID{atomid, rhs};
	return false;
}

bool complainCantAssignSyntheticObject(uint32_t atomid, uint32_t lhs, uint32_t rhs) {
	auto e = error() << "can not assign synthetic objects";
	if (debugmode)
		e << ' ' << CLID{atomid, lhs} << " = " << CLID{atomid, rhs};
	return false;
}

bool complainInvalidAtomLeftSideAssign() {
	ice() << "invalid atom for left-side assignment";
	return false;
}

bool complainInvalidAtomRightSideAssign() {
	ice() << "invalid atom for right-side assignment";
	return false;
}

bool complainInvalidSelf(const Classdef& cdeflhs, const Classdef& cdefrhs) {
	auto e = ice() << "invalid member assignment with invalid 'self'";
	if (debugmode)
		e << " (as %" << cdeflhs.clid << " = %" << cdefrhs.clid << ')';
	return false;
}

auto traceAssignStrategy(AssignStrategy strategy, const Classdef& cdeflhs, const ClassdefTableView& cdeftable, uint32_t lhs, uint32 rhs) -> yuni::String {
	String comment;
	switch (strategy) {
		case AssignStrategy::rawregister:
			comment << "raw copy";
			break;
		case AssignStrategy::ref:
			comment << "assign ref";
			break;
		case AssignStrategy::deepcopy:
			comment << "deep copy";
			break;
	}
	comment << " %" << lhs << " = %" << rhs << " aka '";
	cdeflhs.print(comment, cdeftable, false);
	comment << '\'';
	return comment;
}

} // namespace

bool Analyzer::instanciateAssignment(AtomStackFrame& frame, uint32_t lhs, uint32_t rhs,
		bool canDisposeLHS,
		bool checktype, bool forceDeepcopy) {
	// lhs and rhs can not be null, but they can be identical, to force a clone
	// when required for example
	if (unlikely(lhs == 0 or rhs == 0 or lhs == rhs))
		return complainInvalidLvid();
	if (unlikely(not frame.verify(lhs) or not frame.verify(rhs)))
		return false;
	if (checktype and unlikely(frame.lvids(lhs).synthetic))
		return complainSyntheticObjectsAreImmutable(frame.atomid, lhs, rhs);
	if (checktype and unlikely(frame.lvids(rhs).synthetic))
		return complainCantAssignSyntheticObject(frame.atomid, lhs, rhs);
	// current atom id
	auto atomid   = frame.atomid;
	// LHS cdef
	auto& cdeflhs = cdeftable.classdefFollowClassMember(CLID{atomid, lhs});
	// RHS cdef
	auto& cdefrhs = cdeftable.classdefFollowClassMember(CLID{atomid, rhs});
	// flag for implicitly converting objects (bool, i32...) into builtin (__bool, __i32...)
	bool implicitBuiltin = cdeflhs.isBuiltin() and (not cdefrhs.isBuiltinOrVoid());
	if (implicitBuiltin) {
		// checking if an implicit can be performed (if rhs is a 'builtin' type)
		auto* atomrhs = (cdeftable.findClassdefAtom(cdefrhs));
		if (cdeftable.atoms().core.object[(uint32_t) cdeflhs.kind] == atomrhs) {
			// read the first field, assuming that the first one if actually the same type
			if (canGenerateCode())
				ir::emit::fieldget(out, lhs, rhs, 0);
			return true;
		}
	}
	if (checktype) {
		auto similarity = TypeCheck::isSimilarTo(*this, cdeflhs, cdefrhs, false);
		if (unlikely(TypeCheck::Match::strictEqual != similarity))
			return complainInvalidType(nullptr, cdefrhs, cdeflhs);
	}
	// type propagation
	{
		if (cdefrhs.isVoid()) {
			// trying to assign ? this is acceptable between void values (or any)
			cdeftable.substitute(lhs).mutateToVoid();
			frame.lvids(lhs).autorelease = false;
			return true;
		}
		if (not cdefrhs.isBuiltin()) {
			auto* rhsAtom = cdeftable.findClassdefAtom(cdefrhs);
			if (unlikely(nullptr == rhsAtom))
				return complainInvalidAtomLeftSideAssign();
			cdeftable.substitute(lhs).mutateToAtom(rhsAtom);
		}
		else
			cdeftable.substitute(lhs).mutateToBuiltin(cdefrhs.kind);
	}
	bool lhsCanBeAcquired = canBeAcquired(*this, cdeflhs);
	// Determining the strategy for copying the two values
	// deep copy by default
	auto strategy = AssignStrategy::deepcopy;
	if (lhsCanBeAcquired) {
		if (not forceDeepcopy) {
			// NOTE: the qualifiers from `cdeflhs` are not valid and correspond to nothing
			auto& originalcdef = cdeftable.classdef(CLID{atomid, lhs});
			ir::emit::trace(out, canGenerateCode(), [&]() {
				return originalcdef.print(cdeftable) << originalcdef.clid;
			});
			if (originalcdef.qualifiers.ref)
				strategy = AssignStrategy::ref;
			else {
				auto& lvidinfo = frame.lvids(rhs);
				if (lvidinfo.origin.memalloc or lvidinfo.origin.returnedValue)
					strategy = AssignStrategy::ref;
			}
		}
	}
	else
		strategy = AssignStrategy::rawregister;
	if (lhsCanBeAcquired)
		frame.lvids(lhs).autorelease = true;
	auto& origin = frame.lvids(lhs).origin.varMember;
	bool isMemberVariable = (origin.atomid != 0);
	if (isMemberVariable and unlikely(origin.self == 0))
		return complainInvalidSelf(cdeflhs, cdefrhs);
	ir::emit::trace(out, canGenerateCode(), [&]() {
		return traceAssignStrategy(strategy, cdeflhs, cdeftable, lhs, rhs);
	});
	switch (strategy) {
		case AssignStrategy::rawregister: {
			if (canGenerateCode()) {
				ir::emit::copy(out, lhs, rhs);
				if (isMemberVariable)
					ir::emit::fieldset(out, lhs, origin.self, origin.field);
			}
			break;
		}
		case AssignStrategy::ref: {
			// preserve the origin of the value
			frame.lvids(lhs).origin.memalloc = frame.lvids(rhs).origin.memalloc;
			frame.lvids(lhs).origin.returnedValue = frame.lvids(rhs).origin.returnedValue;
			if (canGenerateCode()) {
				// acquire first the right value to make sure that all data are alive
				// example: a = a
				ir::emit::ref(out, rhs);
				// release the old left value
				if (canDisposeLHS) {
					tryUnrefObject(*this, lhs);
					if (isMemberVariable)
						tryUnrefObject(*this, lhs);
				}
				// copy the pointer
				ir::emit::copy(out, lhs, rhs);
				if (isMemberVariable) {
					ir::emit::ref(out, lhs); // re-acquire for the object
					ir::emit::fieldset(out, lhs, origin.self, origin.field);
				}
			}
			break;
		}
		case AssignStrategy::deepcopy: {
			auto* rhsAtom = cdeftable.findClassdefAtom(cdefrhs);
			if (unlikely(nullptr == rhsAtom))
				return complainInvalidAtomRightSideAssign();
			// 'clone' operator
			if (0 == rhsAtom->classinfo.clone.atomid) {
				if (unlikely(not instanciateAtomClassClone(*rhsAtom, lhs, rhs)))
					return false;
			}
			if (canGenerateCode()) {
				// acquire first the right value to make sure that all data are alive
				// example: a = a
				ir::emit::ref(out, rhs);
				// release the old left value
				if (canDisposeLHS) {
					tryUnrefObject(*this, lhs);
					if (isMemberVariable)
						tryUnrefObject(*this, lhs);
				}
				// note: do not keep a reference on 'out->at...', since the internal buffer might be reized
				uint32_t lvid = createLocalVariables(/*count*/ 2);
				uint32_t retcall = lvid + 1;
				uint32_t rsizof = ir::emit::alloc(out, lvid, CType::t_u64);
				ir::emit::type::objectSizeof(out, rsizof, rhsAtom->atomid);
				// re-allocate some memory
				ir::emit::memory::allocate(out, lhs, rsizof);
				ir::emit::ref(out, lhs);
				frame.lvids(lhs).origin.memalloc = true;
				ir::emit::alloc(out, retcall, CType::t_void);
				// call operator 'clone'
				ir::emit::push(out, lhs); // self
				ir::emit::push(out, rhs); // the object to copy
				ir::emit::call(out, retcall, rhsAtom->classinfo.clone.atomid, rhsAtom->classinfo.clone.instanceid);
				// release rhs - copy is done
				tryUnrefObject(*this, rhs);
				if (isMemberVariable) {
					ir::emit::ref(out, lhs);
					ir::emit::fieldset(out, lhs, origin.self, origin.field);
				}
			}
			break;
		}
	} // switch strategy
	return true;
}

bool Analyzer::instanciateAssignment(const ir::isa::Operand<ir::isa::Op::call>& operands) {
	assert(frame != nullptr);
	if (unlikely(pushedparams.func.indexed.size() != 1))
		return (ice() << "assignment: invalid number of pushed parameters");
	if (unlikely(not pushedparams.func.named.empty()))
		return (ice() << "assignment: named parameters are not accepted");
	if (unlikely(not pushedparams.gentypes.indexed.empty() or not pushedparams.gentypes.named.empty()))
		return (ice() << "assignment: invalid template parameters");
	// -- LHS
	// note: double indirection, since assignment is like method call
	//  %y = %x."="
	//  %z = resolve %y."^()"
	uint32_t lhs = frame->lvids(operands.ptr2func).referer;
	if (likely(lhs != 0)) {
		lhs  = frame->lvids(lhs).referer;
		uint32_t alias = frame->lvids(lhs).alias;
		if (alias != 0)
			lhs = alias;
	}
	// -- RHS
	uint32_t rhs = pushedparams.func.indexed[0].lvid;
	pushedparams.func.indexed.clear();
	bool assigned = instanciateAssignment(*frame, lhs, rhs);
	if (unlikely(not assigned)) {
		frame->invalidate(operands.lvid);
		return false;
	}
	auto& cdeflhs = cdeftable.classdef(CLID{frame->atomid, lhs});
	cdeftable.substitute(operands.lvid).import(cdeflhs);
	if (canBeAcquired(*this, cdeflhs)) {
		frame->lvids(operands.lvid).autorelease = true;
		if (canGenerateCode()) {
			ir::emit::copy(out, operands.lvid, lhs);
			ir::emit::ref(out, operands.lvid);
		}
	}
	return true;
}

} // ny::semantic
