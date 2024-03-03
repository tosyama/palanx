/// Palan AST JSON Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <vector>
#include <fstream>
#include <getopt.h>

#include "PlnGenAstMessage.h"
#include "PlnParser.h"
#include "PlnLexer.h"

using namespace std;
using namespace palan;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "output", required_argument, NULL, 'o' },
		{ "indent", no_argument, NULL, 'i' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool do_indent = false;

	while (0 < (opt = getopt_long(argc, argv, "ho:i", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnGenAstMessage::getMessage(M_Help) << endl;
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
		ifstream f(input_file);
		if (!f) {
			cerr << PlnGenAstMessage::getMessage(E_CouldNotOpenFile, input_file) << endl;
			return 1;
		}

		PlnLexer lexer;
		json ast;
		PlnParser parser(lexer, ast);
		lexer.switch_streams(&f);

		int ret;
		ret = parser.parse();
		if (ret == 0) {
			string out_str = do_indent ? ast.dump(2) : ast.dump();
			if (output_file) {
				ofstream of(output_file);
				if (!of) {
					cerr << "err2";
					return 1;
				}
				of << out_str;
			} else {
				cout << out_str;
			}
		} else { // 1: parse err, 2: memory err
			return 1;
		}
	}

	return 0;
}

