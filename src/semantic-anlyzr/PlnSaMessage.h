/// SA Message Management.
///
/// Manage output message descriptions class.
///
/// @file		PlnSaMessage.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnSaMessageCode {
	M_Help,
	M_Version,
	E_DuplicateVarDecl,		// arg1: variable name
	E_DuplicateFuncDef,		// arg1: function name
	E_IncompatibleTypeCast,	// arg1: from type, arg2: to type
	E_UndefinedFunction,	// arg1: function name
	E_InvalidNarrowingInit,
	E_ExportInBlock,		// arg1: function name
	E_ExportInFunction,		// arg1: function name
	E_UndefinedVariable,	// arg1: variable name
	E_ReturnOutsideFunction,
	E_MultiRetBareReturn,
	E_SingleRetOneExpr,
	E_VoidBareReturn,
	E_VoidCallUsedAsValue,
	E_TupleUndefinedFunction,	// arg1: function name
	E_TupleNeedsMultiRet,		// arg1: function name
	E_TupleVarCountMismatch,	// arg1: function name
	E_ImportFileNotFound,		// arg1: file path
	E_NoInputFile,
	E_InternalError,			// arg1: error code
	E_BreakOutsideLoop,
	E_ContinueOutsideLoop,
	E_FloatModulo,
};

class PlnSaMessage
{
public:
	static string getMessage(PlnSaMessageCode msg_code,
	                         string arg1="\x01", string arg2="\x01");
};
