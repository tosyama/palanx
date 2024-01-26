/// Palan AST Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2023 YAMAGUCHI Toshinobu

#include <iostream>
#include <getopt.h>

using std::cout;
using std::endl;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ 0 }
	};

	int option_index = 0;
	int opt;

	if ((opt = getopt_long(argc, argv, "h", long_options, NULL))) {

		switch (opt) {
			case 'h':
				cout << "help:" << endl;
				break;
			default:
				break;
		}

	} else {
	}

	return 0;
}

