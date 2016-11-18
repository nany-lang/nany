#include "nany/nany.h"
#include <yuni/io/file.h>
#include <yuni/core/math.h>
#include <yuni/core/process/program.h>
#include "libnanyc-config.h"
#include "common-debuginfo.hxx"
#include <string.h>
#include <iostream>
#include <memory>
#ifdef YUNI_OS_LINUX
#include <sys/utsname.h>
#endif

using namespace Yuni;



namespace // anonymous
{

	void printnyVersion(String& out)
	{
		out << "> nanyc {c++/bootstrap} v" << nylib_version();
		if (debugmode)
			out << " {debug}";
		out << '\n';
	}


	void printCompiler(String& out)
	{
		out << "> compiled with ";
		nany_details_export_compiler_version(out);
		out << '\n';
	}


	void printBuildFlags(String& out)
	{
		out << "> config: ";
		out << "params:" << ny::Config::maxFuncDeclParameterCount;
		out << ", pushedparams:" << ny::Config::maxPushedParameters;
		out << ", nmspc depth:" << ny::Config::maxNamespaceDepth;
		out << ", symbol:" << ny::Config::maxSymbolNameLength;
		out << ", nsl:" << ny::Config::importNSL;
		out << '\n';
	}


	void printOS(String& out)
	{
		out << "> os:  ";
		bool osDetected = false;
		#ifdef YUNI_OS_LINUX
		{
			String distribName;
			if (IO::errNone == IO::File::LoadFromFile(distribName, "/etc/issue.net"))
			{
				distribName.replace('\n', ' ');
				distribName.trim();
				if (not distribName.empty())
					out << distribName << ", ";
			}

			osDetected = true;
			struct utsname un;
			if (uname(&un) == 0)
				out << un.sysname << ' ' << un.release << " (" << un.machine << ')';
			else
				out << "(unknown linux)";
		}
		#endif

		if (not osDetected)
			nany_details_export_system(out);
		out << '\n';
	}


	void printCPU(String& out)
	{
		ShortString64 cpustr;
		cpustr << System::CPU::Count() << " cpu(s)/core(s)";

		bool cpuAdded = false;
		if (System::linux)
		{
			auto cpus = Process::System("sh -c \"grep 'model name' /proc/cpuinfo | cut -d':' -f 2 |  sort -u\"");
			cpus.words("\n", [&](AnyString line) -> bool
			{
				line.trim();
				if (not line.empty())
				{
					out << "> cpu: " << line;
					if (not cpuAdded)
					{
						out << " (" << cpustr << ')';
						cpuAdded = true;
					}
					out << '\n';
				}
				return true;
			});
		}

		if (not cpuAdded)
			out << "> cpu: " << cpustr << '\n';
	}


	void printMemory(String& out)
	{
		out << "> ";
		nany_details_export_memory_usage(out);
		out << '\n';
	}


	void buildBugReport(String& out)
	{
		out.reserve(512);
		printnyVersion(out);
		printCompiler(out);
		printBuildFlags(out);
		printOS(out);
		printCPU(out);
		printMemory(out);
	}


} // anonymous namespace




extern "C" void nylib_print_info_for_bugreport()
{
	try
	{
		String out;
		buildBugReport(out);
		std::cout << out;
	}
	catch (const std::exception& e)
	{
		std::cout << "nany: exception: " << e.what() << '\n';
	}
}


extern "C" char* nylib_get_info_for_bugreport(uint32_t* length)
{
	try
	{
		String string;
		buildBugReport(string);

		uint32_t size = string.size();
		auto result = std::make_unique<char[]>(size + 1);
		memcpy(result.get(), string.data(), size);
		result[size] = '\0';
		if (size)
			*length = size;
		return result.release();
	}
	catch (...) {}
	if (length)
		*length = 0;
	return nullptr;
}
