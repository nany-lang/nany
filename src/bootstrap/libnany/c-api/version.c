#include "nany/nany.h"
#include "libnany-version.h"

// NOTE
//
// This file is intentionnaly a C file to abort the compilation if nany.h
// does not use ansi C




const char* nany_website_url()
{
	return LIBNANY_WEBSITE;
}


const char* nany_version()
{
	return LIBNANY_VERSION_STR;
}


const char* nany_version_metadata()
{
	return LIBNANY_VERSION_METADATA;
}


const char* nany_version_prerelease()
{
	return LIBNANY_VERSION_PRERELEASE;
}


uint32_t nany_get_version(uint32_t* major, uint32_t* minor, uint32_t* patch)
{
	if (major)
		*major = LIBNANY_VERSION_MAJOR;
	if (minor)
		*minor = LIBNANY_VERSION_MINOR;
	if (patch)
		*patch = LIBNANY_VERSION_PATCH;

	return LIBNANY_VERSION_MAJOR * 100000u + LIBNANY_VERSION_MINOR * 1000u + LIBNANY_VERSION_PATCH;
}


nybool_t nany_check_compatible_version(uint32_t major, uint32_t minor)
{
	/* currently, no real incompatibilities except for version comparison */
	return ((LIBNANY_VERSION_MAJOR >= major)
		|| (LIBNANY_VERSION_MAJOR == major && minor <= LIBNANY_VERSION_MINOR)
		) ? nytrue : nyfalse;
}
