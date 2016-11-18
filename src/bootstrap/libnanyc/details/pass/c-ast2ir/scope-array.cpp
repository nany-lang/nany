#include <yuni/yuni.h>
#include "scope.h"
#include "details/ir/scope-locker.h"

using namespace Yuni;



namespace ny
{
namespace IR
{
namespace Producer
{


	bool Scope::visitASTArray(AST::Node& node, LVID& localvar)
	{
		assert(node.rule == AST::rgArray);
		if (unlikely(!context.reuse.shorthandArray.node))
			context.prepareReuseForShorthandArray();

		auto& typeofcall = *context.reuse.shorthandArray.typeofcall;
		for (auto& child: node.children)
		{
			switch (child.rule)
			{
				case AST::rgCallParameter:
				{
					typeofcall.children.push_back(&child);
					break;
				}
				default:
					return unexpectedNode(child, "[array-shorthand]");
			}
		}
		if (unlikely(typeofcall.children.empty()))
			return (error(node) << "at least one type is required for shorthand array declarations");

		emitDebugpos(node);
		bool success = visitASTExprNew(*context.reuse.shorthandArray.node, localvar);
		typeofcall.children.clear();

		if (success)
		{
			auto& out = sequence();
			IR::OpcodeScopeLocker opscope{out};
			emitDebugpos(node);
			uint32_t lvidappend = out.emitStackalloc(nextvar(), nyt_any);
			out.emitIdentify(lvidappend, "append", localvar);
			uint32_t func = out.emitStackalloc(nextvar(), nyt_any);
			out.emitIdentify(func, "^()", lvidappend);

			for (auto& child: node.children)
			{
				if (child.rule == AST::rgCallParameter)
				{
					uint32_t lvid = 0;
					success &= visitASTExpr(child.firstChild(), lvid);
					if (success)
					{
						auto callret = out.emitStackalloc(nextvar(), nyt_any);
						out.emitPush(lvid);
						out.emitCall(callret, func);
					}
				}
			}
		}
		return success;
	}




} // namespace Producer
} // namespace IR
} // namespace ny
