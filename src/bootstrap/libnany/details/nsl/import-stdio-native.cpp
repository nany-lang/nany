#include "import-stdcore.h"
#include "details/intrinsic/intrinsic-table.h"
#include <yuni/yuni.h>
#include <yuni/io/file.h>

using namespace Yuni;




namespace Nany
{
namespace Builtin
{


	static bool yn_io_exists(nyvm_t*, void* flnmptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		return (filename) ? IO::Exists(*filename) : false;
	}

	static bool yn_io_file_exists(nyvm_t*, void* flnmptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		return (filename) ? IO::File::Exists(*filename) : false;
	}

	static uint64_t yn_io_file_size(nyvm_t*, void* flnmptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		return (filename) ? IO::File::Size(*filename) : 0u;
	}

	static void* yn_io_file_load(nyvm_t* vm, void* flnmptr)
	{
		auto* string   = vm_allocate<String>(vm);
		auto* filename = reinterpret_cast<String*>(flnmptr);
		if (filename)
			IO::File::LoadFromFile(*string, *filename, (uint64_t) -1);
		return string;
	}

	static bool yn_io_file_save(nyvm_t*, void* flnmptr, void* contentptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		auto* content  = reinterpret_cast<String*>(contentptr);
		return (filename and content)
			? IO::File::SetContent(*filename, *content)
			: false;
	}

	static bool yn_io_file_append(nyvm_t*, void* flnmptr, void* contentptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		auto* content  = reinterpret_cast<String*>(contentptr);
		return (filename and content)
			? IO::File::AppendContent(*filename, *content)
			: false;
	}

	static bool yn_io_file_erase(nyvm_t*, void* flnmptr)
	{
		auto* filename = reinterpret_cast<String*>(flnmptr);
		return (filename) ? (IO::errNone == IO::File::Delete(*filename)) : false;
	}








} // namespace Builtin
} // namespace Nany



namespace Nany
{

	void importNSLIONative(IntrinsicTable& intrinsics)
	{
		intrinsics.add("yuni.io.exists", Builtin::yn_io_exists);

		intrinsics.add("yuni.io.file.exists", Builtin::yn_io_file_exists);
		intrinsics.add("yuni.io.file.size",   Builtin::yn_io_file_size);
		intrinsics.add("yuni.io.file.load",   Builtin::yn_io_file_load);
		intrinsics.add("yuni.io.file.save",   Builtin::yn_io_file_save);
		intrinsics.add("yuni.io.file.append", Builtin::yn_io_file_append);
		intrinsics.add("yuni.io.file.erase",  Builtin::yn_io_file_erase);
	}


} // namespace Nany
