/// Build Manager Message Management.
///
/// Manage output message descriptions class.
///
/// @file		PlnBuildMgrMessage.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#include <string>
using std::string;

enum PlnBuildMgrMessageCode {
	M_Help,
	M_Version,
	E_WrongNumberOfArgs,
	E_CouldNotOpenFile,		// arg1: file path
	E_PalanDirNotDirectory,	// arg1: path string
};

class PlnBuildMgrMessage
{
public:
	static string getMessage(PlnBuildMgrMessageCode msg_code, string arg1="\x01");
};
