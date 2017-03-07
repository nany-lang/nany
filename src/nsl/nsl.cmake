#
# Standard Library (NSL) for Nany C++/Bootstrap
#
set(nsl_files
	# std.core
	"${nsl_root}/std.core/bool.ny"
	"${nsl_root}/std.core/string.ny"
	"${nsl_root}/std.core/f64.ny"
	"${nsl_root}/std.core/f32.ny"
	"${nsl_root}/std.core/i64.ny"
	"${nsl_root}/std.core/u64.ny"
	"${nsl_root}/std.core/i32.ny"
	"${nsl_root}/std.core/u32.ny"
	"${nsl_root}/std.core/i16.ny"
	"${nsl_root}/std.core/u16.ny"
	"${nsl_root}/std.core/u8.ny"
	"${nsl_root}/std.core/i8.ny"
	"${nsl_root}/std.core/utils.ny"
	"${nsl_root}/std.core/pointer.ny"
	"${nsl_root}/std.core/ascii.ny"
	"${nsl_root}/std.core/hash.ny"
	"${nsl_root}/std.core/containers/array.ny"
	"${nsl_root}/std.core/details/string.ny"

	# C types
	"${nsl_root}/std.c/ctypes.ny"

	# std.env
	"${nsl_root}/std.env/env.ny"

	# std.io
	"${nsl_root}/std.io/path.ny"
	"${nsl_root}/std.io/file.ny"
	"${nsl_root}/std.io/file-object.ny"
	"${nsl_root}/std.io/folder.ny"
	"${nsl_root}/std.io/folder-object.ny"
	"${nsl_root}/std.io/io.ny"

	# std.math
	"${nsl_root}/std.math/math.ny"

	# std.memory
	"${nsl_root}/std.memory/utils.ny"

	# std.os
	"${nsl_root}/std.os/process.ny"
	"${nsl_root}/std.os/os.ny"

	# Console
	"${nsl_root}/std.console/console.ny"
	"${nsl_root}/std.console/global.ny"

	# Digest
	"${nsl_root}/std.digest/digest.ny"

	CACHE INTERNAL "Nany Standard Library - File list")




set(nsl_files_unittest
	"${nsl_root}/std.core/unittests/string.ny"
	"${nsl_root}/std.core/unittests/hash.ny"
	"${nsl_root}/std.core/unittests/closure.ny"
	"${nsl_root}/std.core/unittests/view.ny"
	"${nsl_root}/std.core/unittests/view-multiple-loops.ny"
	"${nsl_root}/std.core/unittests/class-anonymous-with-capture.ny"
	"${nsl_root}/std.core/unittests/on-scope.ny"
	"${nsl_root}/std.digest/unittest-digest.ny"
	"${nsl_root}/std.io/unittests/path.ny"

	CACHE INTERNAL "Nany Standard Library - File list unittest")
