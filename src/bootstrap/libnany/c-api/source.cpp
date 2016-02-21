#include <yuni/yuni.h>
#include "nany/nany.h"
#include "details/context/context.h"

using namespace Yuni;




extern "C" nybool_t nany_source_add_n(nycontext_t* ctx, const char* const text, size_t len)
{
	if (YUNI_LIKELY(len and text and ctx and ctx->internal))
	{
		auto& context = *reinterpret_cast<Nany::Context*>(ctx->internal);
		try
		{
			context.defaultTarget().addSource("unknown", AnyString{text, (uint32_t)len});
			return nytrue;
		}
		catch (...) {}
	}
	return nyfalse;
}


extern "C" nybool_t nany_source_add_from_file_n(nycontext_t* ctx, const char* const filename, size_t len)
{
	if (YUNI_LIKELY(len and filename and ctx and ctx->internal))
	{
		auto& context = *reinterpret_cast<Nany::Context*>(ctx->internal);
		try
		{
			context.defaultTarget().addSourceFromFile(AnyString{filename, (uint32_t)len});
			return nytrue;
		}
		catch (...) {}
	}
	return nyfalse;
}
