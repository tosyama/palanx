/// Palan Semantec Analyzer
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <filesystem>
#include <getopt.h>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'v' },
		{ "output",  required_argument, NULL, 'o' },
		{ "indent",  no_argument,       NULL, 'i' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool do_indent = false;

	while (0 < (opt = getopt_long(argc, argv, "hvo:i", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnSaMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnSaMessage::getMessage(M_Version) << endl;
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
		cerr << PlnSaMessage::getMessage(E_NoInputFile) << endl;
		return 1;
	}

	fs::path exec_file_path = fs::canonical("/proc/self/exe");
	string exec_path = exec_file_path.parent_path().string();
	string c2ast_path = exec_path + "/palan-c2ast";

	fs::path ast_path = argv[optind];

	ifstream astfile(ast_path.string());
	json ast = json::parse(astfile);

	string ast_filename = ast_path.filename();
	string suffix = ".ast.json";

	// Delete suffix from filename
	if (ast_filename.size() >= suffix.size() && 
		ast_filename.compare(ast_filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
		ast_filename.erase(ast_filename.size() - suffix.size());
	}

	PlnSemanticAnalyzer analyzer(ast_path.parent_path().string(), ast_filename, c2ast_path);
	analyzer.analysis(ast);

	string sa_path = output_file
		? output_file
		: ast_path.parent_path().string() + "/" + ast_filename + ".sa.json";

	ofstream safile(sa_path);
	safile << (do_indent ? analyzer.result().dump(2) : analyzer.result().dump()) << endl;

	return 0;
}


