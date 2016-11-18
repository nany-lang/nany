#pragma once
#include <yuni/yuni.h>
#include <yuni/core/string.h>
#include <vector>
#include "details/utils/clid.h"



namespace ny
{

	class ClassdefFollow final
	{
	public:
		//! Default constructor
		ClassdefFollow() = default;


		bool empty() const;

		void print(Yuni::String& out, bool clearBefore = true) const;


	public:
		//! Follow some indexed parameter (atomid/parameter index type)
		std::vector<CLID> extends;
		//! Follow some indexed parameter (atomid/parameter index type)
		std::vector<std::pair<CLID, uint>> pushedIndexedParams;
		//! Follow some named parameters (atomid / name)
		std::vector<std::pair<CLID, AnyString>> pushedNamedParams;

	}; // class ClassdefFollow






} // namespace ny

#include "classdef-follow.hxx"
