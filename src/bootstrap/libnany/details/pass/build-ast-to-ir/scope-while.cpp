#include "details/pass/build-ast-to-ir/scope.h"
#include "details/grammar/nany.h"

using namespace Yuni;





namespace Nany
{
namespace IR
{
namespace Producer
{


	bool Scope::visitASTExprWhile(Node& node)
	{
		assert(node.rule == rgWhile);
		assert(not node.children.empty());

		uint32_t childcount = static_cast<uint32_t>(node.children.size());
		if (unlikely(childcount != 2))
		{
			ICE(node) << "while: invalid number of children";
			return false;
		}

		auto& out = program();
		bool success = true;

		if (debugmode)
			out.emitComment("while");

		// new scope for the 'while' statement
		OpcodeScopeLocker opscopeWhile{out};

		// the label at the very begining, to loop back
		uint32_t labelWhile = nextvar();
		out.emitLabel(labelWhile);

		if (debugmode)
			out.emitComment("while-condition");

		// a temporary variable for the result of the condition evaluation
		uint32_t condlvid = nextvar();
		out.emitStackalloc(condlvid, nyt_bool);
		// generating the code for the condition itself
		{
			OpcodeScopeLocker opscopeCond{out};
			auto& condition = *(node.children[0]);
			uint32_t exprEval = 0;
			success &= visitASTExpr(condition, exprEval, false);
			out.emitAssign(condlvid, exprEval, false);
		}

		// jump at the end of the 'while' statement if false
		uint32_t jumpOffset = out.opcodeCount();
		// the label value will be set later, to preserve strict ordering
		out.emitJz(condlvid, 0, /*labelEnd*/ 0);

		// 'while' body
		{
			if (debugmode)
				out.emitComment("while-body");

			OpcodeScopeLocker opscopeBody{out};
			success &= visitASTStmt(*(node.children[1]));
		}

		// loop back to the condition evaluation
		out.emitJmp(labelWhile);

		// end of while, if the condition evaluation failed
		// (and update the label id in the original jump)
		uint32_t labelEnd = nextvar();
		out.emitLabel(labelEnd);
		out.at<IR::ISA::Op::jz>(jumpOffset).label = labelEnd;

		return success;
	}


	bool Scope::visitASTExprDoWhile(Node& node)
	{
		assert(node.rule == rgDoWhile);
		assert(not node.children.empty());

		uint32_t childcount = static_cast<uint32_t>(node.children.size());
		if (unlikely(childcount != 2))
		{
			ICE(node) << "do-while: invalid number of children";
			return false;
		}

		auto& out = program();
		bool success = true;

		if (debugmode)
			out.emitComment("do-while");

		// new scope for the 'while' statement
		OpcodeScopeLocker opscopeWhile{out};

		// the label at the very begining, to loop back
		uint32_t labelWhile = nextvar();
		out.emitLabel(labelWhile);

		// 'while' body
		{
			if (debugmode)
				out.emitComment("do-while-body");

			OpcodeScopeLocker opscopeBody{out};
			success &= visitASTStmt(*(node.children[0]));
		}

		if (debugmode)
			out.emitComment("do-while-condition");

		// a temporary variable for the result of the condition evaluation
		uint32_t condlvid = nextvar();
		out.emitStackalloc(condlvid, nyt_bool);
		// generating the code for the condition itself
		{
			OpcodeScopeLocker opscopeCond{out};
			auto& condition = *(node.children[1]);
			uint32_t exprEval = 0;
			success &= visitASTExpr(condition, exprEval, false);
			out.emitAssign(condlvid, exprEval, false);
		}

		// jump to the begining if true (non zero)
		out.emitJnz(condlvid, 0, labelWhile);

		return success;
	}





} // namespace Producer
} // namespace IR
} // namespace Nany