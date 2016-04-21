#include "stacktrace.h"
#include "details/context/build.h"

using namespace Yuni;




namespace Nany
{
namespace VM
{

	void Stacktrace<true>::grow()
	{
		auto offbase     = reinterpret_cast<std::uintptr_t>(baseframe);
		auto offtop      = reinterpret_cast<std::uintptr_t>(topframe);
		auto size        = (offtop - offbase) / sizeof(Frame);
		auto newcapacity = size + 64;

		baseframe  = (Frame*)::realloc(baseframe, sizeof(Frame) * newcapacity);
		if (YUNI_UNLIKELY(!baseframe))
			throw std::bad_alloc();
		topframe   = baseframe + size;
		upperLimit = baseframe + newcapacity;
	}


	void Stacktrace<true>::dump(Build& build, const AtomMap& map) const
	{
		// this routine does not allocate memory to handle extreme situations

		ShortString128 tmp;
		uint32_t i = 0;

		for (auto* pointer = topframe; (pointer > baseframe); --pointer, ++i)
		{
			auto& frame = *pointer;

			build.printStderr("    at #");
			tmp.clear();
			tmp << i << ": ";
			build.printStderr(tmp);
			const auto& caption = map.fetchSequenceCaption(frame.atomidInstance[0], frame.atomidInstance[1]);
			build.printStderr(caption);

			build.printStderr("\n       '");

			auto* atom = map.findAtom(frame.atomidInstance[0]);
			if (atom)
			{
				build.printStderr(atom->origin.filename);
				if (atom->origin.line != 0)
				{
					tmp.clear();
					tmp << ':' << atom->origin.line;
					build.printStderr(tmp);
				}
			}
			else
				build.printStderr("<invalid-atom>");

			build.printStderr("'\n");
		}
	}






} // namespace VM
} // namespace Nany
