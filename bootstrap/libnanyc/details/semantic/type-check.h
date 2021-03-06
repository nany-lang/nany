#pragma once
#include "details/atom/classdef-table-view.h"

namespace ny::semantic { struct Analyzer; }

namespace ny::semantic::TypeCheck {

enum class Match {
	//! The two types do not match
	none = 0,
	//! The two types are strictly equal (exactly the same type, or any)
	strictEqual,
	//! The two types have the same public interface
	equal,
};

/*!
** \brief Try to tell if 2 types are similar
**
** \param A The first type
** \param[in,out] B The other type
** \return 'none' if not equal, othe
** \note B: Only a well-known classdef, with no interface and no follow-ups (and a valid atom)
*/
Match isSimilarTo(Analyzer&, const Classdef& A, const Classdef& B, bool allowImplicit = false);

} // ny::semantic::TypeCheck
