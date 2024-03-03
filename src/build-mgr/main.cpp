/// Palan Build manager
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <vector>
#include <getopt.h>
#include <sys/stat.h>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

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

	if (do_clean) {
		fs::remove_all(palan_dir_path + "/work");
		return 0;
	}

	if ((argc-optind) != 1) {
		// TODO: Error Message
		cerr << "err" << endl;
		return 1;
	}

	// Compile and execute input file.
	{
		fs::path input_file = argv[optind];
		if (!fs::exists(input_file)) {
			cerr << "err" << endl;
			return 1;
		}
		fs::path exec_file_path = fs::canonical("/proc/self/exe");
		string input_file_work_path = palan_dir_path + "/work" + fs::canonical(input_file).string();
		fs::create_directories(input_file_work_path);
		string astcmd = exec_file_path.parent_path().string() + "/palan-gen-ast " + input_file.string() + " -o " + input_file_work_path + "/ast.json";

		int ret = system(astcmd.c_str());
		if (WIFEXITED(ret)) {
			return WEXITSTATUS(ret);
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
	
	// struct stat st;
	if (fs::exists(palan_path)) {
		if (!fs::is_directory(palan_path)) {
			// TODO: Error Message
			cerr << "err" << endl;
			return "";
		}
	}

	// create palan dirs
	fs::create_directory(palan_path);
	fs::create_directory(palan_path + "/work");
	return palan_path;
}

