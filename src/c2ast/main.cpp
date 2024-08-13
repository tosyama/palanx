/// C to Palan AST Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <getopt.h>

using namespace std;
#include "cfileinfo.h"
#include "ctoken.h"
#include "clexer.h"
#include "cpreprocessor.h"

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "output", no_argument, NULL, 'o' },
		{ "indent", no_argument, NULL, 'i' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool do_indent = false;

	while (0 < (opt = getopt_long(argc, argv, "ho:i", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << "Help (not Inplemented.)" << endl;
				return 0;
			case 'o':
				output_file = optarg;
				break;
			case 'i':
				do_indent = true;
				break;
			default:
				break;
		}
	} 

	if ((argc-optind) != 1) {
		cerr << "err1" << endl;
		return 1;
	}

	string input_file = argv[optind];
	{
		vector<CMacro*> macros;
		vector<string> include_paths = {
			"/usr/include",
			"/usr/include/x86_64-linux-gnu",
			"/usr/lib/gcc/x86_64-linux-gnu/7/include",
			"/usr/lib/gcc/x86_64-linux-gnu/8/include",
			"/usr/lib/gcc/x86_64-linux-gnu/9/include",
			"/usr/lib/gcc/x86_64-linux-gnu/10/include",
		};
		CPreprocessor cpp(macros, include_paths);
	}

	return 0;
}
