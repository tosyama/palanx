/// Palan Semantec Analyzer
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <getopt.h>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using namespace std;
using json = nlohmann::json;

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
				cout << "help" << endl;
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
		// TODO: Error Message
		cerr << "err" << endl;
		return 1;
	}

	string ast_path = argv[optind];

	ifstream astfile(ast_path);
	json ast = json::parse(astfile);

	return 0;
}


