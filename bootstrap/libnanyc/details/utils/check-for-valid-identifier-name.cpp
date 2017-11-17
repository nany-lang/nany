#include "check-for-valid-identifier-name.h"
#include "details/ast/ast.h"
#include "details/errors/errors.h"
#include "libnanyc-config.h"
#include <yuni/io/file.h>
#include <unordered_set>
#include <unordered_map>

using namespace Yuni;

namespace ny {

static const std::unordered_set<AnyString> reservedKeywords = {
	"self", "override", "new", "is",
	"func", "class", "typedef",
	"for", "while", "do", "in", "each", "if", "then", "else", "switch",
	"return", "raise",
	"const", "var", "ref", "cref",
	"and", "or", "mod", "xor", "not",
	// boolean types
	"true", "false",
	"i8", "i6", "i32", "i64", "i128", "i256", "i512",
	"u8", "u16", "u32", "u64", "u128", "u256", "u512",
	"f32", "f64",
	"bool", "string", "pointer",
	// null keyword
	"null", "void"
};

// TODO use a more efficient matching mecanism for small string sets
// (it seems that using a bunch a switch/case would be really faster)

static const std::unordered_set<AnyString> classOperators = {
	"new", "dispose", "clone",
	"+=", "-=", "*=", "/=", "^=",
	"()", "[]",
	"++self", "self++", "--self", "self--"
};

static const std::unordered_set<AnyString> globalOperators = {
	"and", "or", "mod", "xor", "not",
	"<<", ">>",
	"+", "-", "*", "/", "^",
	"!=", "==", "<", ">", "<=", ">="
};

static const std::unordered_map<AnyString, AnyString> opnormalize = {
	{ "and", "^and" },
	{ "or",  "^or"  },
	{ "mod", "^mod" },
	{ "xor", "^xor" },
	{ "not", "^not" },
	{ "+",   "^+"   }, { "++",  "^++"  },
	{ "-",   "^-"   }, { "--",  "^--"  },
	{ "*",   "^*"   }, { "/",   "^/"   },
	{ "^",   "^^"   },
	{ "<<",  "^<<"  }, { ">>",  "^>>"  },
	{ "<=",  "^<="  }, { ">=",  "^>="  },
	{ "<",   "^<"   }, { ">",   "^>"   },
	{ "==",  "^=="  }, { "!=",  "^!="  },
	{ "+=",  "^+="  }, { "-=",  "^-="  },
	{ "*=",  "^*="  }, { "/=",  "^/="  },
	{ "^=",  "^^="  },
	{ "()",  "^()"  }, { "[]",  "^[]"  },
	{ "self++", "^self++" }, { "++self", "^++self" },
	{ "self--", "^self--" }, { "--self", "^--self" },
};

bool complainEmptyNode(const AST::Node& node) {
	return (error(node) << "invalid empty name");
}

bool complainInvalidIdentifierNameWithUnderscore(const AST::Node& node, const AnyString& name) {
	auto err = error(node);
	err << "invalid identifier name '" << name << "': underscore as a prefix is not allowed";
	err.message.origins.location.pos.offsetEnd = err.message.origins.location.pos.offset + name.size();
	return false;
}

bool complainIdentifierTooLong(const AST::Node& node, const AnyString& name) {
	auto err = error(node) << "identifier name too long";
	auto& location = err.message.origins.location;
	location.pos.offsetEnd = location.pos.offset + name.size();
	return false;
}

bool complainNotValidOperator(const AST::Node& node, const AnyString& name, const char* what) {
	auto err = (error(node)
				<< "invalid identifier name: '" << name << "' is not a valid " << what << " operator");
	auto& location = err.message.origins.location;
	location.pos.offsetEnd = location.pos.offset + name.size();
	return false;
}

bool complainReservedKeyword(const AST::Node& node, const AnyString& name) {
	auto err = error(node) << "'" << name << "' is a reserved keyword";
	auto& location = err.message.origins.location;
	location.pos.offsetEnd = location.pos.offset + name.size();
	return false;
}

void warnNonAscii(const AST::Node& node, const AnyString& name) {
	auto wrn = warning(node)
			   << "invalid identifier name: the name contains non-ascii characters";
	auto& location = wrn.message.origins.location;
	location.pos.offsetEnd = location.pos.offset + name.size();
}

AnyString normalizeOperatorName(AnyString name) {
	// to deal with grammar's potential glitches when eating tokens
	// (it seems that it would considerably slow down the parsing to improve it)
	name.trimRight();
	assert(not name.empty());
	if (name == '=')
		return AnyString{"=", 1}; // iso, not an overloaded operator
	// TODO normalize operator name: find a mecanism to not rely on a map
	auto tit = opnormalize.find(name);
	if (YUNI_LIKELY(tit != opnormalize.end()))
		return tit->second;
	assert(false and "unknown identifier for normalization");
	return name;
}

// operator / type
bool checkForValidIdentifierName(const AST::Node& node, const AnyString& name, Flags<IdNameFlag> flags) {
	// checking the name size
	uint32_t size = name.size();
	if (unlikely(0 == size))
		return complainEmptyNode(node);
	if (unlikely(size > config::maxSymbolNameLength))
		return complainIdentifierTooLong(node, name);
	if (YUNI_LIKELY(not flags(IdNameFlag::isOperator))) {
		// names with '_' as prefix are for internal uses only
		char c = name[0];
		if (c != '^') {
			if (unlikely(c == '_'))
				return complainInvalidIdentifierNameWithUnderscore(node, name);
			// checking if the id name is not from one of reserved keywords
			if (unlikely(not flags(IdNameFlag::isType) and reservedKeywords.count(name) != 0))
				return complainReservedKeyword(node, name);
			// warning - ascii only chars
			if (false) {
				auto end = name.utf8end();
				for (auto i = name.utf8begin(); i != end; ++i) {
					if (not i->isAscii()) {
						warnNonAscii(node, name);
						break;
					}
				}
			}
		}
	}
	else {
		if (flags.has(IdNameFlag::isInClass)) {
			if (unlikely(classOperators.count(name) == 0))
				return complainNotValidOperator(node, name, "class");
		}
		else {
			if (unlikely(globalOperators.count(name) == 0))
				return complainNotValidOperator(node, name, "global");
		}
	}
	return true;
}

} // ny
