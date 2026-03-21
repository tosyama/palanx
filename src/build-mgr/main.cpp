/// Palan Build manager
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <getopt.h>
#include <filesystem>
#include <boost/assert.hpp>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

static string getPalanDirPath();

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",   no_argument,       NULL, 'h' },
		{ "clean",  no_argument,       NULL, 'c' },
		{ "output", required_argument, NULL, 'o' },
		{ 0 }
	};

	int opt, option_index = 0;
	bool do_clean = false;
	string binary_name = "a.out";
	bool output_specified = false;

	while (0 < (opt = getopt_long(argc, argv, "hco:", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << "help" << endl;
				return 0;
			case 'c':
				do_clean = true;
				break;
			case 'o':
				binary_name = optarg;
				output_specified = true;
				break;
			default:
				break;
		}
	}

	string palan_dir_path = getPalanDirPath();
	string palan_work_path = palan_dir_path + "/work";

	if (do_clean) {
		fs::remove_all(palan_work_path);
		return 0;
	}

	if ((argc-optind) != 1) {
		// TODO: Error Message
		cerr << "build-mgr err1" << endl;
		return 1;
	}

	fs::path exec_file_path = fs::canonical("/proc/self/exe");
	string exec_path = exec_file_path.parent_path().string();

	vector<string> target_files;
	vector<string> ast_files;
	target_files.push_back(fs::weakly_canonical(argv[optind]).string());

	// Compile target files.
	for (int i=0; i<target_files.size(); i++) {
		fs::path input_file = target_files[i];
		if (!fs::exists(input_file)) {
			// TODO: Error Message
			cerr << "build-mgr err2" << endl;
			cerr << input_file << endl;
			return 1;
		}

		string input_file_work_path = palan_work_path + input_file.parent_path().string();
		fs::create_directories(input_file_work_path);
		string output_ast_path = palan_work_path + input_file.string() + ".ast.json";
		string astcmd = exec_path + "/palan-gen-ast " + input_file.string() + " -o " + output_ast_path;

		int ret = system(astcmd.c_str());
		if (WIFEXITED(ret)) {
			ret = WEXITSTATUS(ret);
			if (ret)
				return ret;
		} else {
			return -1;
		}
		ast_files.emplace_back(output_ast_path);

		// check import files and add targets
		ifstream astfile(output_ast_path);
		json ast = json::parse(astfile);
		if (!ast["import"].is_null()) {
			for (auto imp: ast["import"]) {
				if (imp["path-type"] == "src") {
					string import_path = imp["path"];

					if (import_path.ends_with(".pa")) {
						if (import_path[0] != '/') {
							import_path = input_file.parent_path().string() + "/" + import_path;
						}
						import_path = fs::weakly_canonical(import_path);
						auto it = find(target_files.begin(), target_files.end(), import_path);
						if (it == target_files.end()) {
							target_files.emplace_back(import_path);
						}
					}
				}
			}
		}
	}

	vector<string> obj_files;

	for (int i=0; i<ast_files.size(); i++) {
		string ast_file = ast_files[i];

		// palan-sa
		string sacmd = exec_path + "/palan-sa " + ast_file;
		int ret = system(sacmd.c_str());
		if (WIFEXITED(ret)) {
			ret = WEXITSTATUS(ret);
			if (ret) return ret;
		} else {
			return -1;
		}

		// Derive output paths (strip ".ast.json" suffix from ast_file)
		string base = ast_file.substr(0, ast_file.size() - 9);  // remove ".ast.json"
		string sa_file  = base + ".sa.json";
		string asm_file = base + ".s";
		string obj_file = base + ".o";

		// palan-codegen
		bool is_entry = (i == 0);
		string codegencmd = exec_path + "/palan-codegen";
		if (!is_entry) codegencmd += " --no-entry";
		codegencmd += " " + sa_file;
		ret = system(codegencmd.c_str());
		if (WIFEXITED(ret)) {
			ret = WEXITSTATUS(ret);
			if (ret) return ret;
		} else {
			return -1;
		}

		// as
		string ascmd = "as " + asm_file + " -o " + obj_file;
		ret = system(ascmd.c_str());
		if (WIFEXITED(ret)) {
			ret = WEXITSTATUS(ret);
			if (ret) return ret;
		} else {
			return -1;
		}

		obj_files.push_back(obj_file);
	}

	// ld — link all object files into the final binary
	// TODO: extract required libraries from AST cinclude link clauses
	string ldcmd = "ld";
	for (auto& obj : obj_files) ldcmd += " " + obj;
	ldcmd += " -lc -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o " + binary_name;
	int ret = system(ldcmd.c_str());
	if (WIFEXITED(ret)) {
		ret = WEXITSTATUS(ret);
		if (ret) return ret;
	} else {
		return -1;
	}

	// When no explicit output is specified, run the binary and remove it.
	// This enables script-style usage: palan script.pa
	if (!output_specified) {
		int run_ret = system(("./" + binary_name).c_str());
		fs::remove(binary_name);
		if (WIFEXITED(run_ret)) return WEXITSTATUS(run_ret);
		return -1;
	}

	return 0;
}

string getPalanDirPath()
{
	string home_path = getenv("HOME");
	string palan_path = home_path + "/.palan";
	
	if (fs::exists(palan_path)) {
		if (!fs::is_directory(palan_path)) {
			// TODO: Error Message
			cerr << "err" << endl;
			return "";
		}
	}

	// create palan dirs
	fs::create_directories(palan_path + "/work");
	return palan_path;
}

