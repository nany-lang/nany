#include "runtime.h"
#include "details/intrinsic/catalog.h"
#include "details/vm/context.h"
#include <yuni/core/hash/checksum/md5.h>
#define XXH_PRIVATE_API 1
#define XXH_ACCEPT_NULL_INPUT_POINTER 1
#include <xxhash/xxhash.h>

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


static uint32_t nanyc_digest_xxhash32(nyvm_t*, const void* ptr, uint32_t length) {
	uint32_t seed = 0;
	return XXH32(ptr, length, seed);
}


static uint64_t nanyc_digest_xxhash64(nyvm_t*, const void* ptr, uint64_t length) {
	uint32_t seed = 0;
	return XXH64(ptr, length, seed);
}


namespace ny {
namespace nsl {
namespace import {


void digest(ny::intrinsic::Catalog& intrinsics) {
	intrinsics.emplace("__nanyc_digest_md5",      nanyc_digest_md5);
	intrinsics.emplace("__nanyc_digest_xxhash32", nanyc_digest_xxhash32);
	intrinsics.emplace("__nanyc_digest_xxhash64", nanyc_digest_xxhash64);
}


} // namespace import
} // namespace nsl
} // namespace ny
