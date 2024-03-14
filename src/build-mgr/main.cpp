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
		{ "help", no_argument, NULL, 'h' },
		{ "clean", no_argument, NULL, 'c' },
		{ 0 }
	};

	int opt, option_index = 0;
	bool do_clean = false;

	while (0 < (opt = getopt_long(argc, argv, "h:", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << "help" << endl;
				return 0;
			case 'c':
				do_clean = true;
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
		cerr << "err" << endl;
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
			cerr << "err" << endl;
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

					if (import_path.size()) {
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

	for (int i=0; i<ast_files.size(); i++) {
		fs::path input_file = ast_files[i];
		string sacmd = exec_path + "/palan-sa " + input_file.string();
		int ret = system(sacmd.c_str());
		if (WIFEXITED(ret)) {
			ret = WEXITSTATUS(ret);
			if (ret)
				return ret;
		} else {
			return -1;
		}
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

