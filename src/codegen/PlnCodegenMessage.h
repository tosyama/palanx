/// Codegen Message Management.
///
/// Manage output message descriptions class.
///
/// @file		PlnCodegenMessage.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnCodegenMessageCode {
	M_Help,
	M_Version,
	E_UnknownType,		// arg1: type name
	E_UnknownExprType,	// arg1: expr-type string
	E_UnknownStmtType,	// arg1: stmt-type string
};

class PlnCodegenMessage
{
public:
	static string getMessage(PlnCodegenMessageCode msg_code, string arg1="\x01");
};
