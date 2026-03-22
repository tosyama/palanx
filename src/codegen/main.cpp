/// Palan Code Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <filesystem>
#include <getopt.h>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnDeserialize.h"
#include "PlnVCodeGen.h"
#include "PlnX86CodeGen.h"
#include "PlnCodegenMessage.h"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "version",  no_argument,       NULL, 'v' },
		{ "output",   required_argument, NULL, 'o' },
		{ "no-entry", no_argument,       NULL, 'n' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool no_entry = false;

	while (0 < (opt = getopt_long(argc, argv, "hvo:n", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnCodegenMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnCodegenMessage::getMessage(M_Version) << endl;
				return 0;
			case 'o':
				output_file = optarg;
				break;
			case 'n':
				no_entry = true;
				break;
			default:
				break;
		}
	}

	if ((argc - optind) != 1) {
		cerr << "Usage: palan-codegen [options] <SA file>" << endl;
		return 1;
	}

	fs::path sa_path = argv[optind];

	ifstream safile(sa_path.string());
	if (!safile) {
		cerr << "palan-codegen: Could not open file '" << sa_path.string() << "'." << endl;
		return 1;
	}
	json sa = json::parse(safile);

	// Determine output path
	string sa_filename = sa_path.filename().string();
	string suffix = ".sa.json";
	if (sa_filename.size() >= suffix.size() &&
		sa_filename.compare(sa_filename.size() - suffix.size(), suffix.size(), suffix) == 0) {
		sa_filename.erase(sa_filename.size() - suffix.size());
	}

	string asm_path = output_file
		? output_file
		: sa_path.parent_path().string() + "/" + sa_filename + ".s";

	ofstream asmfile(asm_path);
	if (!asmfile) {
		cerr << "palan-codegen: Could not open output file '" << asm_path << "'." << endl;
		return 1;
	}

	// Deserialize -> VProg -> assembly
	Module module = deserialize(sa);

	PlnVCodeGen vgen;
	VProg vprog = vgen.generate(module, no_entry);

	PlnX86CodeGen codegen(asmfile);
	codegen.emit(vprog);

	return 0;
}
