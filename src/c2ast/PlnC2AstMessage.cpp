/// C2Ast Message management definitions.
///
/// Manage output message descriptions.
///
/// @file		PlnC2AstMessage.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include "PlnC2AstMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnC2AstMessage::getMessage(PlnC2AstMessageCode msg_code, string arg1)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-c2ast [options] <C header file>\n"
				"Translate C header to palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output <file>   Output AST to file.\n"
				" -i, --indent          Output AST json with indent.\n"
				" -p, --path <dir>      Add search path for C headers.\n"
				" -c, --curdir <dir>    Specify current directory for relative includes.\n"
				" -s, --sysheader       Treat input as a system header.\n"
				" -v, --version         Display version.\n"
				" -h, --help            Display help.\n";

		case M_Version:
			return "palan-c2ast " PALAN_VERSION;

		case E_NoInputFile:
			return "palan-c2ast: no input file";

		default:
			BOOST_ASSERT(false);
	}
}
