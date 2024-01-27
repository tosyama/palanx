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
};

class PlnGenAstMessage
{
public:
	static string getMessage(PlnMessageCode msg_code);
};


