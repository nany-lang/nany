#include <yuni/yuni.h>
#include <yuni/string.h>
#include <yuni/core/system/main.h>
#include <yuni/core/system/environment.h>
#include <yuni/io/file.h>
#include <nany/nany.h>
#include <memory>
#include <iostream>
#include <cassert>
#include <limits.h>
#include <memory>

using namespace Yuni;

static const char* argv0 = "";

#define likely(X)    YUNI_LIKELY(X)
#define unlikely(X)  YUNI_UNLIKELY(X)




namespace std
{
	template<> struct default_delete<nyproject_t>
	{
		void operator () (nyproject_t* ptr)
		{
			nany_project_unref(ptr);
		}
	};

	template<> struct default_delete<nybuild_t>
	{
		void operator () (nybuild_t* ptr)
		{
			nany_build_unref(ptr);
		}
	};

	template<> struct default_delete<nyprogram_t>
	{
		void operator () (nyprogram_t* ptr)
		{
			nany_program_unref(ptr);
		}
	};

} // namespace std


struct Options
{
	//! Verbose mode
	bool verbose = false;
	//! Maximum number of jobs
	uint32_t jobs = 1;
	//! Memory limit (zero means unlimited)
	size_t memoryLimit = 0;

	//! The new argv0
	String argv0;
};


static int printUsage(const char* const argv0)
{
	std::cout
		<< "Usage: " << argv0 << " [options] file...\n"
		<< "Options:\n"
		<< "  --bugreport       Display some useful information to report a bug\n"
		<< "                    (https://github.com/nany-lang/nany/issues/new)\n"
		<< "  --help, -h        Display this information\n"
		<< "  --version, -v     Print the version\n\n";
	return EXIT_SUCCESS;
}


static int printBugReportInfo()
{
	auto* text = nany_get_info_for_bugreport();
	if (text)
	{
		std::cout << text;
		::free(text);
		return 0;
	}
	return EXIT_FAILURE;
}


static int printVersion()
{
	std::cout << nany_version() << '\n';
	return EXIT_SUCCESS;
}


static int unknownOption(const AnyString& name)
{
	std::cerr << argv0 << ": unknown option '" << name << "'\n";
	return EXIT_FAILURE;
}


static inline std::unique_ptr<nyproject_t> createProject(const Options& options)
{
	nyproject_cf_t cf;
	nany_project_cf_init(&cf);

	size_t limit = options.memoryLimit;
	if (limit == 0)
		limit = static_cast<size_t>(System::Environment::ReadAsUInt64("NANY_MEMORY_LIMIT"));

	if (limit != 0)
		nany_memalloc_set_with_limit(&cf.allocator, limit);

	return std::unique_ptr<nyproject_t>{nany_project_create(&cf)};
}


static inline std::unique_ptr<nyprogram_t> compile(const AnyString& argv0, Options& options)
{
	// PROJECT
	auto project = createProject(options);
	if (!project)
		return nullptr;

	// SOURCE
	auto& file = options.argv0;
	IO::Canonicalize(file, argv0);
	auto r = nany_project_add_source_from_file_n(project.get(), file.c_str(), file.size());
	if (unlikely(r == nyfalse))
		return nullptr;

	// BUILD
	nybuild_cf_t cf;
	nany_build_cf_init(&cf, project.get());
	auto build = std::unique_ptr<nybuild_t>{nany_build_prepare(project.get(), &cf)};
	if (!build)
		return nullptr;

	do
	{
		auto bStatus = nany_build(build.get());
		if (unlikely(bStatus == nyfalse))
			break;

		if (unlikely(options.verbose))
			nany_build_print_report_to_console(build.get());

		nyprogram_cf_t pcf;
		nany_program_cf_init(&pcf, &cf);
		return std::unique_ptr<nyprogram_t>(nany_program_prepare(build.get(), &pcf));
	}
	while (false);

	// an error has occured
	nany_build_print_report_to_console(build.get());
	return nullptr;
}


static inline int execute(int argc, char** argv, Options& options)
{
	assert(argc >= 1);

	auto program = compile(argv[0], options);
	int exitstatus = 66;

	if (!!program)
	{
		if (argc == 1)
		{
			const char* nargv[] = { options.argv0.c_str(), nullptr };
			exitstatus = nany_main(program.get(), 1, nargv);
		}
		else
		{
			auto** nargv = (const char**)::malloc(((size_t) argc + 1) * sizeof(const char**));
			if (nargv)
			{
				nargv[0] = options.argv0.c_str();
				for (int i = 1; i < argc; ++i)
					nargv[i] = argv[i];
				nargv[argc] = nullptr;

				exitstatus = nany_main(program.get(), argc, nargv);
				free(nargv);
			}
		}
	}

	return exitstatus;
	/*
	// building
	bool buildstatus;
	if (init)
	{
		nyreport_t* report = nullptr;
		buildstatus = (nany_build(&ctx, &report) == nytrue);

		// report printing
		// if an error has occured or if in verbose mode
		if (YUNI_UNLIKELY((not buildstatus) or options.verbose))
			(buildstatus ? nany_report_print_stdout : nany_report_print_stderr)(report);

		nany_report_unref(&report);
	}
	else
		buildstatus = false;

	// executing the program
	if (buildstatus)
	{
		if (argc == 1)
		{
			const char* nargv[] = { scriptfile.c_str(), nullptr };
			exitstatus = nany_run_main(&ctx, 1, nargv);
		}
		else
		{
			auto** nargv = (const char**)::malloc(((size_t) argc + 1) * sizeof(const char**));
			if (nargv)
			{
				for (int i = 1; i < argc; ++i)
					nargv[i] = argv[i];
				nargv[0]    = scriptfile.c_str();
				nargv[argc] = nullptr;

				exitstatus = nany_run_main(&ctx, argc, nargv);
				free(nargv);
			}
		}
	}

	nany_uninitialize(&ctx);*/
	return exitstatus;
}






int main(int argc, char** argv)
{
	argv0 = argv[0];
	if (YUNI_UNLIKELY(argc <= 1))
	{
		std::cerr << argv0 << ": no input script file\n";
		return EXIT_FAILURE;
	}

	try
	{
		Options options;
		int firstarg = -1;
		for (int i = 1; i < argc; ++i)
		{
			const char* const carg = argv[i];
			if (carg[0] == '-')
			{
				if (carg[1] == '-')
				{
					if (carg[2] != '\0') // to handle '--' option
					{
						AnyString arg{carg};

						if (arg == "--help")
							return printUsage(argv[0]);
						if (arg == "--version")
							return printVersion();
						if (arg == "--bugreport")
							return printBugReportInfo();
						if (arg == "--verbose")
						{
							options.verbose = true;
							continue;
						}
						return unknownOption(arg);
					}
					else
					{
						firstarg = i + 1;
						break; // nothing must interpreted after '--'
					}
				}
				else
				{
					AnyString arg{carg};
					if (arg == "-h")
						return printUsage(argv[0]);
					if (arg == "-v")
						return printVersion();
					return unknownOption(arg);
				}
			}

			firstarg = i;
			break;
		}

		//
		// -- execute the script
		//
		return execute(argc - firstarg, argv + firstarg, options);
	}
	catch (const std::bad_alloc&)
	{
		std::cerr << '\n' << argv0 << ": error: failed to allocate memory\n";
	}
	catch (...)
	{
		std::cerr << '\n' << argv0 << ": error: unhandled exception\n";
	}
	return EXIT_FAILURE;
}
