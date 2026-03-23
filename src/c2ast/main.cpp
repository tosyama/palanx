/// C to Palan AST Generator
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <getopt.h>

using namespace std;
namespace fs = std::filesystem;

#include "CFileInfo.h"
#include "CToken.h"
#include "CLexer.h"
#include "CPreprocessor.h"
#include "CParser.h"
#include "PlnC2AstMessage.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

static vector<string> split(const string &str, const string &delim)
{
	vector<string> v;
	string::size_type start = 0;
	string::size_type end = str.find(delim);

	while (end != string::npos) {
		v.push_back(str.substr(start, end - start));
		start = end + delim.size();
		end = str.find(delim, start);
	}

	v.push_back(str.substr(start, end - start));
	return v;
}

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",               no_argument,       NULL, 'h' },
		{ "version",            no_argument,       NULL, 'v' },
		{ "indent",             no_argument,       NULL, 'i' },
		{ "path",               required_argument, NULL, 'p' },
		{ "curdir",             required_argument, NULL, 'c' },
		{ "output",             required_argument, NULL, 'o' },
		{ "sysheader",          no_argument,       NULL, 's' },
		{ "dump-preprocessed",  no_argument,       NULL, 'd' },
		{ 0 }
	};

	int opt, option_index = 0;
	char *output_file = NULL;
	bool do_indent = false;
	bool is_sys_header = false;
	bool do_dump_preprocessed = false;
	vector<string> search_paths;
	string current_directory = "";
	string target = "x86_64-linux-gnu";

	while (0 < (opt = getopt_long(argc, argv, "hvo:ip:c:sd", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnC2AstMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnC2AstMessage::getMessage(M_Version) << endl;
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
			case 'd':
				do_dump_preprocessed = true;
				break;

			default:
				break;
		}
	}

	if ((argc-optind) != 1) {
		cerr << PlnC2AstMessage::getMessage(E_NoInputFile) << endl;
		return 1;
	}

	// Add search latest gcc include path from /usr/lib/gcc/{target}/{version}/include
	if (search_paths.size() == 0) {
		int gcc_major = -1;
		int gcc_minor = -1;
		int gcc_build = -1;

		fs::path gcc_path = "/usr/lib/gcc/" + target;
		if (fs::exists(gcc_path)) {
			for (auto& p: fs::directory_iterator(gcc_path)) {
				fs::path include_path = p.path();
				include_path.append("include");
				if (fs::is_directory(include_path)) {
					string version = p.path().filename();
					vector<string> v = split(version, ".");
					if (v.size() >= 1 && v.size() <= 3) {
						int major = stoi(v[0]);
						int minor = (v.size() >= 2) ? stoi(v[1]) : -1;
						int build = (v.size() >= 3) ? stoi(v[2]) : -1;

						if (major > gcc_major) {
							gcc_major = major;
							gcc_minor = minor;
							gcc_build = build;
						} else if (major == gcc_major) {
							if (minor > gcc_minor) {
								gcc_minor = minor;
								gcc_build = build;
							} else if (minor == gcc_minor) {
								if (build > gcc_build) {
									gcc_build = build;
								}
							}
						}
					}
				}
			}
		}

		if (gcc_major >= 0) {
			string gcc_latest = gcc_path.string() + "/" + to_string(gcc_major);
			if (gcc_minor >= 0) {
				gcc_latest += "." + to_string(gcc_minor);
				if (gcc_build >= 0) {
					gcc_latest += "." + to_string(gcc_build);
				}
			}

			search_paths.push_back(gcc_latest + "/include");
		}
	}

	try {
		string input_file = argv[optind];
		{
			vector<CMacro*> macros;
			vector<string> include_paths = {
				"/usr/include",
				"/usr/local/include",
			};

			include_paths.push_back("/usr/include/" + target);

			for (auto& p: search_paths) {
				include_paths.push_back(p);
			}

			fs::path exec_file_path = fs::canonical("/proc/self/exe");
			string exec_path = exec_file_path.parent_path().string();
			CPreprocessor cpp(macros, include_paths);
			string predefined_path = exec_path + "/c2ast/predefined.h";
			cpp.loadPredefined(predefined_path);

			if (is_sys_header) {
				input_file = CFileInfo::searchFilePath(input_file, include_paths);
			}

			cpp.preprocess(input_file);

			if (do_dump_preprocessed) {
				cpp.dumpPreprocessed(cout);

			} else {
				CParser cparser(cpp.top_tokens, cpp.lexers);
				json ast;
				ast["original"] = input_file;
				int ret = cparser.parse(ast);
				if (ret) return ret;

				ostream* out = &cout;
				ofstream outfile;
				if (output_file) {
					outfile.open(output_file);
					out = &outfile;
				}
				if (do_indent) {
					*out << ast.dump(2) << endl;
				} else {
					*out << ast.dump() << endl;
				}
			}
		}
	} catch (runtime_error& e) {
		if (e.what()[0]) cerr << e.what() << endl;
		return 1;
	}

	return 0;
}
