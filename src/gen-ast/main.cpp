/// Palan AST JSON Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2023 YAMAGUCHI Toshinobu

#include <iostream>
#include <getopt.h>

using namespace std;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "output", required_argument, NULL, 'o' },
		{ "indent", no_argument, NULL, 'i' },
		{ 0 }
	};

	int option_index = 0;
	int opt;

	while (0 < (opt = getopt_long(argc, argv, "ho:i", long_options, NULL))) {

		switch (opt) {
			case 'h':
				cout << "help:" << endl;
				break;
			case 'o':
				cout << optarg << endl;
				break;
			case 'i':
				cout << "indent" << endl;
				break;
			default:
				break;
		}

	} 

	if (optind >= argc) {
		cerr << "err" << endl;
		return 1;
	}
	
	for (int i=optind; i<argc; i++) {
		cout << argv[i] << endl;
	}

	cout << "OK" << endl;

	return 0;
}

