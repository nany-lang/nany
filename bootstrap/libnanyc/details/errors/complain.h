#pragma once
#include <yuni/core/string.h>
#include <exception>


namespace ny { namespace ir { struct Instruction; }}
namespace ny { namespace ir { struct Sequence; }}


namespace ny {
namespace complain {


struct Error: std::exception {
	const char* what() const noexcept override { return msg.c_str(); }
	yuni::String msg;
};


struct ICE: public Error {
	using Error::Error;
};


struct Opcode: Error {
	Opcode(const ny::ir::Sequence&, const ir::Instruction&, const AnyString&);
};


struct SilentFall final: std::exception {
	const char* what() const noexcept override { return "error silently ignored"; }
};



bool exception();
bool exception(const std::exception&);

bool invalidAtom(const char* what);
bool invalidAtomForFuncReturn(const AnyString& symbol);

bool invalidAtomMapping(const AnyString& atom);
bool invalidRecursiveAtom(const AnyString& atom);

bool inconsistentGenericTypeParameterIndex();


} // namespace complain
} // namespace ny
