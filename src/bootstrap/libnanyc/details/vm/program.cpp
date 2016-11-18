#include "program.h"
#include "details/atom/atom.h"
#include "details/intrinsic/intrinsic-table.h"
#include "types.h"
#include "thread-context.h"

using namespace Yuni;


namespace ny
{
namespace vm
{

	namespace {

	void flushAll(nyconsole_t& console)
	{
		if (console.flush)
		{
			console.flush(console.internal, nycerr);
			console.flush(console.internal, nycout);
		}
	}

	} // anonymous namespace




	void Program::destroy()
	{
		this->~Program();
		auto& allocator = const_cast<nyallocator_t&>(cf.allocator);
		allocator.deallocate(&allocator, this, sizeof(ny::vm::Program));
	}


	int Program::execute(uint32_t argc, const char** argv)
	{
		if (YUNI_UNLIKELY(cf.entrypoint.size == 0))
			return 0;

		if (cf.on_execute)
		{
			if (nyfalse == cf.on_execute(self()))
				return 1;
		}

		// TODO Take input arguments into consideration
		// (requires std.Array<:T:>)
		(void) argc;
		(void) argv;

		retvalue = 1; // EXIT_FAILURE
		uint32_t atomid = ny::ref(build).main.atomid;
		uint32_t instanceid = ny::ref(build).main.instanceid;

		auto& sequence = map.sequence(atomid, instanceid);
		ThreadContext thrctx{*this, AnyString{cf.entrypoint.c_str, cf.entrypoint.size}};

		bool success = thrctx.initializeFirstTContext();
		if (YUNI_UNLIKELY(not success))
			return 666;

		//
		// Execute the program
		//
		uint64_t exitstatus;
		if (thrctx.invoke(exitstatus, sequence, atomid, instanceid))
		{
			retvalue = static_cast<int>(exitstatus);

			if (cf.on_terminate)
				cf.on_terminate(self(), nytrue, retvalue);

			// always flush to make sure that the listener will update the output
			// (especially useful when embedded into a C/C++ application)
			flushAll(cf.console);
			return retvalue;
		}

		if (cf.on_terminate)
			cf.on_terminate(self(), nyfalse, retvalue);

		// Always flush the output
		flushAll(cf.console);
		return (retvalue != 0) ? retvalue : 1; // avoid 0 if failure
	}


} // namespace vm
} // namespace ny
