/// Build Manager Message management definitions.
///
/// Manage output message descriptions.
///
/// @file		PlnBuildMgrMessage.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include "PlnBuildMgrMessage.h"
#include "../common/PlnDefs.h"
#include <boost/assert.hpp>

string PlnBuildMgrMessage::getMessage(PlnBuildMgrMessageCode msg_code, string arg1)
{
	switch (msg_code) {
		case M_Help:
			return
				"Usage: palan [options] <source.pa>\n"
				"Compile and run a Palan source file.\n"
				"\n"
				"Options\n"
				" -o, --output=<file>   Write binary to file (do not auto-run).\n"
				" -c, --clean           Remove all cached work files.\n"
				" -v, --version         Display version.\n"
				" -h, --help            Display help.\n";

		case M_Version:
			return "palan " PALAN_VERSION;

		case E_WrongNumberOfArgs:
			return "palan: wrong number of arguments";

		case E_CouldNotOpenFile:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: could not open file '" + arg1 + "'";

		case E_PalanDirNotDirectory:
			BOOST_ASSERT(arg1 != "\x01");
			return "palan: '" + arg1 + "' exists but is not a directory";

		default:
			BOOST_ASSERT(false);
	}
}
