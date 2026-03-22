/// Message management definisions.
///
/// Manage output message descripitons.
///
/// @file		PlnGenAstMessage.h
/// @copyright	2024 YAMAGUCHI Toshinobu

#include "PlnGenAstMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnGenAstMessage::getMessage(PlnMessageCode msg_code, string arg1)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan-gen-ast [options] <path-to-source>...\n"
				"Generate palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output=<output-path>   Output AST to file.\n"
				" -i, --indent                 Output AST json with indent.\n"
				" -v, --version                Display version.\n"
				" -h, --help                   Display help.\n";

		case M_Version:
			return "palan-gen-ast " PALAN_VERSION;

		case E_CouldNotOpenFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open file '" + arg1 + "'.";

		case E_CouldNotOpenOutputFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "Could not open output file '" + arg1 + "'.";

		case E_NoInputFile:
			return "palan-gen-ast: no input file";

		default:
			BOOST_ASSERT(false);
	}
}
