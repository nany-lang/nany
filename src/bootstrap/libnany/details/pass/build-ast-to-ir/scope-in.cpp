#include <yuni/yuni.h>
#include "details/pass/build-ast-to-ir/scope.h"
#include "details/utils/check-for-valid-identifier-name.h"

using namespace Yuni;




namespace Nany
{
namespace IR
{
namespace Producer
{


	bool Scope::visitASTExprIn(const AST::Node& node, LVID& localvar, ShortString128& elementname)
	{
		// lvid representing the input container
		AST::Node* container = nullptr;
		AST::Node* predicate = nullptr;

		for (auto& childptr: node.children)
		{
			auto& child = *childptr;
			switch (child.rule)
			{
				case AST::rgInVars:
				{
					if (unlikely(child.children.size() != 1))
						return error(child) << "only one cursor name is allowed in views";
					auto& identifier = child.firstChild();
					if (unlikely(identifier.rule != AST::rgIdentifier))
						return ice(identifier) << "identifier expected";

					if (unlikely(not checkForValidIdentifierName(child, identifier.text, false)))
						return false;

					elementname = identifier.text;
					break;
				}

				case AST::rgInContainer:
				{
					if (unlikely(child.children.size() != 1))
						return ice(child) << "invalid expression";

					container = &child.firstChild();
					break;
				}

				case AST::rgInPredicate:
				{
					if (unlikely(child.children.size() != 1))
						return ice(child) << "invalid predicate expr";
					auto& expr = child.firstChild();
					if (unlikely(expr.rule != AST::rgExpr))
						return ice(expr) << "invalid node type for predicate";

					predicate = &expr;
					break;
				}
				default:
					return unexpectedNode(child, "[expr/in]");
			}
		}

		if (unlikely(!container))
			return ice(node) << "invalid view container expr";

		if (!context.reuse.inset.node)
			context.prepareReuseForIn();

		context.reuse.inset.container->children.clear();
		context.reuse.inset.container->children.push_back(container);
		context.reuse.inset.viewname->text = "^view^default";

		if (elementname.empty())
			elementname << "%vr" << nextvar();
		context.reuse.inset.elementname->text = elementname;

		if (!predicate)
			predicate = AST::Node::Ptr::WeakPointer(context.reuse.inset.premadeAlwaysTrue);
		context.reuse.inset.predicate->children.clear();
		context.reuse.inset.predicate->children.push_back(predicate);

		emitDebugpos(node);
		return visitASTExpr(*context.reuse.inset.node, localvar);
	}


	bool Scope::visitASTExprIn(const AST::Node& node, LVID& localvar)
	{
		// Name of the target ref for each element in the container (ignored here)
		ShortString128 elementname;
		return visitASTExprIn(node, localvar, elementname);
	}




} // namespace Producer
} // namespace IR
} // namespace Nany
