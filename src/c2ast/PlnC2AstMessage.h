/// C2Ast Message Management.
///
/// Manage output message descriptions class.
///
/// @file		PlnC2AstMessage.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnC2AstMessageCode {
	M_Help,
	M_Version,
	E_NoInputFile,
	E_ErrorDirective,		// arg1: message text from #error directive
	E_InvalidDefinedSyntax,
	E_MalformedMacroArgList,
};

class PlnC2AstMessage
{
public:
	static string getMessage(PlnC2AstMessageCode msg_code, string arg1="\x01");
};
