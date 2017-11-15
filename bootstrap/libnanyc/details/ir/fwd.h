#pragma once
#include <yuni/yuni.h>
#include <cstdint>

namespace ny {
namespace ir {

//! Constant for declaring classdef with several overloads
static constexpr yuint64 kClassdefHasOverloads = (yuint64) - 1;

//! Magic dust for blueprints
static constexpr yuint64 blueprintMagicDust = 0x123456789ABCDEF;

} // namespace ir
} // namespace ny

namespace ny {
namespace ir {
namespace Producer {

struct Scope;
struct Context;

} // namespace Producer
} // namespace ir
} // namespace ny
