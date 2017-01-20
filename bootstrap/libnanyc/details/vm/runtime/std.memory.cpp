#include "std.core.h"
#include "details/intrinsic/catalog.h"
#include <limits>


template<class T>
static T nanyc_strlen(nyvm_t*, void* string) {
	size_t len = string ? strlen(reinterpret_cast<const char*>(string)) : 0u;
	return (sizeof(size_t) == sizeof(T) or len < std::numeric_limits<T>::max())
		   ? static_cast<T>(len)
		   : static_cast<T>(-1);
}


namespace ny {
namespace nsl {
namespace import {


void memory(ny::intrinsic::Catalog& intrinsics) {
	intrinsics.add("strlen32",  nanyc_strlen<uint32_t>);
	intrinsics.add("strlen64",  nanyc_strlen<uint64_t>);
}


} // namespace import
} // namespace nsl
} // namespace ny
