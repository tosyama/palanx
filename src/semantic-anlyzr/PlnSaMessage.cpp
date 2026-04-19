/// SA Message management definitions.
///
/// Manage output message descriptions.
///
/// @file		PlnSaMessage.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include "PlnSaMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnSaMessage::getMessage(PlnSaMessageCode msg_code, string arg1, string arg2)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-sa [options] <ast.json>\n"
				"Semantic analyze palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output=<output-path>   Output SA result to file.\n"
				" -i, --indent                 Output SA json with indent.\n"
				" -v, --version                Display version.\n"
				" -h, --help                   Display help.\n";

		case M_Version:
			return "palan-sa " PALAN_VERSION;

		case E_DuplicateVarDecl:
			BOOST_ASSERT(arg1 != "\x01");
			return "Variable '" + arg1 + "' is already declared.";

		case E_DuplicateFuncDef:
			BOOST_ASSERT(arg1 != "\x01");
			return "Function '" + arg1 + "' is already defined.";

		case E_IncompatibleTypeCast:
			BOOST_ASSERT(arg1 != "\x01");
			BOOST_ASSERT(arg2 != "\x01");
			return "Cannot cast type '" + arg1 + "' to '" + arg2 + "'.";

		case E_UndefinedFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined function '" + arg1 + "'.";

		case E_InvalidNarrowingInit:
			return "Narrowing initialization requires a numeric literal.";

		case E_ExportInBlock:
			BOOST_ASSERT(arg1 != "\x01");
			return "Export is not allowed inside a block scope.";

		case E_ExportInFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Export is not allowed inside a function scope.";

		case E_UndefinedVariable:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined variable '" + arg1 + "'.";

		case E_ReturnOutsideFunction:
			return "Return statement outside of function.";

		case E_MultiRetBareReturn:
			return "Multi-return function requires bare 'return;' statement.";

		case E_SingleRetOneExpr:
			return "Single-return function requires exactly one return expression.";

		case E_VoidBareReturn:
			return "Void function requires bare 'return;' statement.";

		case E_VoidCallUsedAsValue:
			return "Void function call cannot be used as a value.";

		case E_TupleUndefinedFunction:
			BOOST_ASSERT(arg1 != "\x01");
			return "Undefined function '" + arg1 + "'.";

		case E_TupleNeedsMultiRet:
			BOOST_ASSERT(arg1 != "\x01");
			return "Function '" + arg1 + "' does not have multiple return values.";

		case E_TupleVarCountMismatch:
			BOOST_ASSERT(arg1 != "\x01");
			return "Variable count does not match return count of '" + arg1 + "'.";

		case E_ImportFileNotFound:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open import file '" + arg1 + "'.";

		case E_NoInputFile:
			return "palan-sa: no input file";

		case E_InternalError:
			BOOST_ASSERT(arg1 != "\x01");
			return "Internal error: " + arg1;

		case E_BreakOutsideLoop:
			return "Break statement outside of loop.";

		case E_ContinueOutsideLoop:
			return "Continue statement outside of loop.";

		case E_FloatModulo:
			return "'%' operator is not supported for float types.";

		case E_ArraySizeNotInteger:
			return "Array size expression must be an integer type.";

		case E_ArrayIndexNotInteger:
			return "Array index expression must be an integer type.";

		case E_NotArrayType:
			return "Expression is not an array (pointer) type.";

		default:
			BOOST_ASSERT(false);
	}
}
