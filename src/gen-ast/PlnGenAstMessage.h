/// Message Management.
///
/// Manage output message descripitons class.
///
/// @file		PlnGenAstMessage.h
/// @copyright	2024 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnMessageCode {
	M_Help,
	M_Version,
	E_CouldNotOpenFile,		// filename
	E_CouldNotOpenOutputFile,	// filename
	E_NoInputFile,
	E_C2AstFailed,			// header path
	E_ExpectedLinkKeyword,		// identifier
};

class PlnGenAstMessage
{
public:
	static string getMessage(PlnMessageCode msg_code, string arg1="\x01");
};


