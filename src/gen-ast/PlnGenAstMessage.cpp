/// Message management definisions.
///
/// Manage output message descripitons.
///
/// @file		PlnGenAstMessage.h
/// @copyright	2024 YAMAGUCHI Toshinobu

#include "PlnGenAstMessage.h"
#include <boost/assert.hpp>

string PlnGenAstMessage::getMessage(PlnMessageCode msg_code)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: gen-ats [options] <path-to-source>...\n"
				"Generate palan AST Json.\n"
				"\n"
				"Options\n"
				" -o, --output=<output-path>   Output AST to file.\n"
				" -i, --indent                 Output AST json with indent.\n"
				" -h, --help                   Display help.\n";

		default:
			BOOST_ASSERT(false);
	}
}
