#include "runtime.h"
#include "details/intrinsic/catalog.h"
#include "details/vm/context.h"
#include <yuni/core/hash/checksum/md5.h>
#include <iostream>

using namespace Yuni;


static void* nanyc_digest_md5(nyvm_t* vm, const char* string, uint64_t length) {
	auto& context = *reinterpret_cast<ny::vm::Context*>(vm->tctx);
	Hash::Checksum::MD5 md5;
	md5.fromRawData(string, length);
	if (not md5.value().empty()) {
		uint32_t size = md5.value().size();
		uint32_t capacity = size + ny::config::extraObjectSize;
		char* cstr = (char*) vm->allocator->allocate(vm->allocator, capacity);
		if (cstr) {
			const char* src = md5.value().c_str();
			for (uint32_t i = 0; i != size; ++i)
				cstr[i] = src[i];
			context.returnValue.size     = md5.value().size();
			context.returnValue.capacity = md5.value().size();
			context.returnValue.data     = cstr;
			return &context.returnValue;
		}
	}
	return nullptr;
}


namespace ny {
namespace nsl {
namespace import {


void digest(ny::intrinsic::Catalog& intrinsics) {
	intrinsics.add("__nanyc_digest_md5",   nanyc_digest_md5);
}


} // namespace import
} // namespace nsl
} // namespace ny
