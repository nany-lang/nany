#pragma once
#include <yuni/yuni.h>
#include <yuni/string.h>
#include "details/atom/classdef-table-view.h"
#include "details/ir/isa/data.h"
#include "details/ir/sequence.h"
#include "details/errors/errors.h"
#include "stack-frame.h"




namespace Nany
{
namespace Pass
{
namespace Instanciate
{
namespace complain
{


	//! Invalid classdef (ICE)
	bool classdef(const Classdef&, const char* usertxt);

	//! Unknown intrinsic
	bool unknownIntrinsic(const AnyString& name);

	//! Class not instanciated (not ready for use)
	bool classNotInstanciated(const Atom&);

	//! A class is required
	bool classRequired();

	//! Failed to allocate class object (null atom, due to previous error)
	bool canNotAllocateClassNullAtom(const Classdef&, uint32_t lvid);

	//! Invalid Self (not a class)
	bool invalidClassSelf(const AnyString& identifier);

	//! No property found
	bool noproperty(const AnyString& identifier);

	//! Ambigous property call
	bool ambigousPropertyCall(const AnyString& identifier);

	//! Invalid typedef
	bool invalidTypedef(const Classdef&);

	//! Unknown identifier
	bool notDeclaredInThisScope(const Atom* self, const Atom& atom, const AnyString& name);

	//! Multiple definitions
	bool multipleDefinitions(const Atom&, const AnyString& deffor);




	//! Return type mismatch
	bool returnTypeMismatch(const Classdef& expected, const Classdef& usertype);

	//! Return with implicit conversion
	bool returnTypeImplicitConversion(const Classdef& expected, const Classdef& usertype, uint32_t line = 0, uint32_t offset = 0);

	//! Return type is missing
	bool returnTypeMissing(const Classdef* expected, const Classdef* usertype);

	//! Multiple types for return
	bool returnMultipleTypes(const Classdef& expected, const Classdef& usertype, uint32_t line, uint32_t offset);




	//! Circular reference for typedef
	bool typedefCircularReference(const Atom& original, const Atom& responsible);

	//! Unknown typedef
	bool typedefNotResolved(const Atom& original);

	//! Reference to typedef declaration after the current one
	bool typedefRefDeclaredAfter(const Atom& original, const Atom& responsible);



} // namespace complain
} // namespace Instanciate
} // namespace Pass
} // namespace Nany
