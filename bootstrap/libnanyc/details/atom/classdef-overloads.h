#pragma once
#include "libnanyc.h"
#include <vector>

namespace ny {

struct ClassdefTableView;
struct Atom;

struct ClassdefOverloads final {
	bool empty() const;
	uint size() const;
	void print(Yuni::String& out, const ClassdefTableView& table, bool clearBefore = true) const;
	void clear();

	std::vector<std::reference_wrapper<Atom>>&  getList();
	const std::vector<std::reference_wrapper<Atom>>&  getList() const;

private:
	//! Follow some indexed parameter (atomid/parameter index type)
	std::vector<std::reference_wrapper<Atom>> m_overloads;

}; // class ClassdefFollow

} // namespace ny

#include "classdef-overloads.hxx"
