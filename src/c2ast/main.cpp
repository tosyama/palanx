/// C to Palan AST Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <vector>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <getopt.h>

using namespace std;
namespace fs = std::filesystem;

#include "cfileinfo.h"
#include "ctoken.h"
#include "clexer.h"
#include "cpreprocessor.h"


int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help", no_argument, NULL, 'h' },
		{ "indent", no_argument, NULL, 'i' },
		{ "path", required_argument, NULL, 'p' },
		{ "curdir", required_argument, NULL, 'c' },
		{ "output", required_argument, NULL, 'o' },
		{ "sysheader", no_argument, NULL, 's' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool do_indent = false;
	bool is_sys_header = false;
	vector<string> search_paths;
	string current_directory = "";

	while (0 < (opt = getopt_long(argc, argv, "ho:ip:c:s", long_options, NULL))) {
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
			case 'p': // search path
				search_paths.push_back(optarg);
				break;			
			case 'c': // current directory
				current_directory = optarg;
				break;
			case 's':
				is_sys_header = true;
				break;

			default:
				break;
		}
	} 

	if ((argc-optind) != 1) {
		cerr << "errx " << argc << ":" << optind << endl;
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
			"/usr/lib/gcc/x86_64-linux-gnu/11/include",
		};

		fs::path exec_file_path = fs::canonical("/proc/self/exe");
		string exec_path = exec_file_path.parent_path().string();
		CPreprocessor cpp(macros, include_paths);
		string predefined_path = exec_path + "/c2ast/predefined.h";
		cpp.loadPredefined(predefined_path);

		if (is_sys_header) {
			input_file = CFileInfo::searchFilePath(input_file, include_paths);
		}

		cpp.preprocess(input_file);

		// cout << "c2ast pre_path: " << predefined_path << endl;
		// cout << "c2ast inputfile: " << input_file << endl;

		cpp.setOutput(output_file);
	}

	return 0;
}
