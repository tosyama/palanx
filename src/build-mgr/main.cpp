/// Palan Build manager
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <algorithm>
#include <cstring>
#include <getopt.h>
#include <filesystem>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnBuildMgrMessage.h"
#include "../common/PlnProcess.h"

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

static string getPalanDirPath();

// Run a pipeline tool and map the outcome onto build-mgr's historical return
// convention: 0 on success, the child's exit status when it failed, -1 when
// it died from a signal (main returns that as 255, as before). A child that
// could not be started at all was previously indistinguishable from either --
// the shell reported "command not found" on its own stderr and exited 127,
// which build-mgr forwarded blindly. Now it is diagnosed and 127 is kept as
// the returned exit code for compatibility.
static int runTool(const vector<string>& argv)
{
	PlnProcResult r = runProcess(argv);
	switch (r.status) {	// LCOV_EXCL_BR_LINE -- the Signaled arm below is unreachable under test
		case PlnSpawnStatus::Exited:
			return r.exit_code;
		// LCOV_EXCL_START
		case PlnSpawnStatus::Signaled:
			// Only reachable if one of the pipeline tools itself (gen-ast/sa/
			// codegen/as/ld) dies by signal -- not reproducible by any Palan
			// source under test; the compiled *user* program crashing is a
			// separate, tested path (see the run-and-delete step below).
			return -1;
		// LCOV_EXCL_STOP
		default:
			cerr << PlnBuildMgrMessage::getMessage(
				E_FailedToExecute, argv[0], strerror(r.err_no)) << endl;
			return 127;
	}
}

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "version", no_argument,       NULL, 'v' },
		{ "clean",   no_argument,       NULL, 'c' },
		{ "output",  required_argument, NULL, 'o' },
		{ 0 }
	};

	int opt, option_index = 0;
	bool do_clean = false;
	string binary_name = "a.out";
	bool output_specified = false;

	while (0 < (opt = getopt_long(argc, argv, "hvco:", long_options, NULL))) {
		switch (opt) {
			case 'h':
				cout << PlnBuildMgrMessage::getMessage(M_Help) << endl;
				return 0;
			case 'v':
				cout << PlnBuildMgrMessage::getMessage(M_Version) << endl;
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
		cerr << PlnBuildMgrMessage::getMessage(E_WrongNumberOfArgs) << endl;
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
			cerr << PlnBuildMgrMessage::getMessage(E_CouldNotOpenFile, input_file.string()) << endl;
			return 1;
		}

		string input_file_work_path = palan_work_path + input_file.parent_path().string();
		fs::create_directories(input_file_work_path);
		string output_ast_path = palan_work_path + input_file.string() + ".ast.json";

		if (int ret = runTool({exec_path + "/palan-gen-ast", input_file.string(), "-o", output_ast_path}))
			return ret;
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
	// Union of libraries requested by cinclude `link` clauses across every
	// module. Declared here (not scoped to the aggregation block below) so it
	// is still alive when the ld command line is built.
	set<string> link_libs;

	for (int i=0; i<ast_files.size(); i++) {
		string ast_file = ast_files[i];

		// palan-sa
		if (int ret = runTool({exec_path + "/palan-sa", ast_file}))
			return ret;

		// Derive output paths (strip ".ast.json" suffix from ast_file)
		string base = ast_file.substr(0, ast_file.size() - 9);  // remove ".ast.json"
		string sa_file  = base + ".sa.json";
		string asm_file = base + ".s";
		string obj_file = base + ".o";

		// palan-codegen
		bool is_entry = (i == 0);
		vector<string> codegenArgv{exec_path + "/palan-codegen"};
		if (!is_entry) codegenArgv.push_back("--no-entry");
		codegenArgv.push_back(sa_file);
		if (int ret = runTool(codegenArgv))
			return ret;

		// as
		if (int ret = runTool({"as", asm_file, "-o", obj_file}))
			return ret;

		obj_files.push_back(obj_file);
	}

	// Single pass over every module's sa.json to aggregate whole-program
	// information: the union of `link`-clause libraries (into link_libs,
	// declared above so it survives to the ld step) and alloc-shapes
	// deduplicated by key (used below to synthesize shared allocators).
	{
		set<string> seen_shapes;
		vector<json> arr_shapes;
		vector<json> struct_shapes;
		vector<json> arr_struct_shapes;
		for (auto& ast_file : ast_files) {
			string base = ast_file.substr(0, ast_file.size() - 9);
			ifstream f(base + ".sa.json");
			json sa = json::parse(f);
			// SA always emits "libs" (possibly empty), so no contains() guard
			// is needed. Read before the alloc-shapes continue below: a
			// module can carry libs without carrying alloc-shapes.
			for (auto& l : sa["libs"]) link_libs.insert(l.get<string>());
			if (!sa.contains("alloc-shapes")) continue;
			for (auto& shape : sa["alloc-shapes"]) {
				string key;
				if (shape.value("shape-kind", "") == "struct")
					key = "struct:" + shape["shape-name"].get<string>();
				else
					key = shape["shape-key"].get<string>();
				if (!seen_shapes.insert(key).second) continue;
				if (shape.value("shape-kind", "") == "struct")
					struct_shapes.push_back(shape);
				else if (shape.value("shape-kind", "") == "arr-struct")
					arr_struct_shapes.push_back(shape);
				else
					arr_shapes.push_back(shape);
			}
		}

		// Derive one shared allocator per leaf primitive type used by "arr-ptr" fields
		vector<json> arr_prim_shapes;
		set<string> seenArrPrim;
		for (auto& shape : struct_shapes)
			for (auto& af : shape["owned-array-fields"])
				if (af["elem-kind"] == "prim") {
					string leaf = af["leaf-name"];
					if (seenArrPrim.insert(leaf).second)
						arr_prim_shapes.push_back({{"leaf-type", leaf}});
				}

		if (!arr_shapes.empty() || !struct_shapes.empty() || !arr_struct_shapes.empty()) {
			string workdir = fs::path(ast_files[0]).parent_path().string();
			string alloc_pa  = workdir + "/__allocators.pa";
			string alloc_ast = alloc_pa + ".ast.json";
			string alloc_sa  = alloc_pa + ".sa.json";
			string alloc_s   = alloc_pa + ".s";
			string alloc_o   = alloc_pa + ".o";

			{
				// This synthesized source only ever cinclude's stdlib.h with
				// no `link` clause, so its sa.json can never contribute to
				// link_libs; it does not need to join the aggregation loop
				// above.
				ofstream out(alloc_pa);
				out << "cinclude <stdlib.h>;\n";

				// Struct type declarations (leaf-first order from SA)
				for (auto& shape : struct_shapes) {
					string name = shape["shape-name"];
					out << "\ntype " << name << " {";
					for (auto& f : shape["fields"]) {
						string kind  = f["type-kind"];
						string tname = f["type-name"];
						string fname = f["name"];
						if (kind == "embed")
							out << " $" << tname << " " << fname << ";";
						else if (kind == "raw-ptr")
							out << " @" << tname << " " << fname << ";";
						else if (kind == "embed-arr")
							out << " [" << f["count"].get<int64_t>() << "]$" << tname << " " << fname << ";";
						else if (kind == "arr-ptr")
							out << " [" << f["count"].get<int64_t>() << "]" << tname << " " << fname << ";";
						else if (kind == "embed-ptr-arr")
							out << " [" << f["count"].get<int64_t>() << (f.value("mutable",false) ? "]@!" : "]@")
							    << tname << " " << fname << ";";
						else
							out << " " << tname << " " << fname << ";";
					}
					out << " };\n";
				}

				// arr-prim allocator/free functions (one shared pair per leaf primitive type)
				for (auto& shape : arr_prim_shapes) {
					string leaf = shape["leaf-type"];
					out << "\nexport func __pln_alloc_arr_prim_" << leaf
					    << "(int64 n) -> []" << leaf << " {\n"
					    << "    [n]" << leaf << " result;\n"
					    << "    return result;\n"
					    << "}\n"
					    << "export func __pln_free_arr_prim_" << leaf
					    << "([]" << leaf << " arr) {\n"
					    << "    free(arr);\n"
					    << "    return;\n"
					    << "}\n";
				}

				// Struct allocator/free functions
				for (auto& shape : struct_shapes) {
					string name = shape["shape-name"];
					auto& owned = shape["owned-fields"];
					auto& ownedArr = shape["owned-array-fields"];

					out << "\nexport func __pln_alloc_" << name
					    << "() -> " << name << " p {\n"
					    << "    " << name << " p;\n";
					for (auto& of : owned) {
						string oname = of["name"];
						string sname = of["struct-name"];
						out << "    __pln_alloc_" << sname << "() -> p." << oname << ";\n";
					}
					for (auto& af : ownedArr) {
						string fname = af["name"];
						int64_t count = af["count"].get<int64_t>();
						if (af["elem-kind"] == "prim") {
							string leaf = af["leaf-name"];
							out << "    __pln_alloc_arr_prim_" << leaf << "(" << count
							    << ") -> p." << fname << ";\n";
						} else { // struct leaf
							string sname = af["leaf-name"];
							out << "    __pln_alloc_arr_" << sname << "(" << count
							    << ") -> p." << fname << ";\n";
						}
					}
					out << "}\n";

					out << "export func __pln_free_" << name
					    << "(" << name << " p) {\n";
					for (auto& af : ownedArr) {
						string fname = af["name"];
						if (af["elem-kind"] == "prim") {
							string leaf = af["leaf-name"];
							out << "    __pln_free_arr_prim_" << leaf << "(p." << fname << ");\n";
						} else { // struct leaf
							string sname = af["leaf-name"];
							int64_t count = af["count"].get<int64_t>();
							out << "    __pln_free_arr_" << sname << "(p." << fname << ", " << count << ");\n";
						}
					}
					for (auto& of : owned) {
						string oname = of["name"];
						string sname = of["struct-name"];
						out << "    __pln_free_" << sname << "(p." << oname << ");\n";
					}
					out << "    free(p);\n"
					    << "    return;\n"
					    << "}\n";
				}

				// Owned struct array allocator/free functions
				for (auto& shape : arr_struct_shapes) {
					string struct_name = shape["struct-name"];
					string shape_key   = shape["shape-key"];

					bool has_owned = false;
					for (auto& ss : struct_shapes)
						if (ss.value("shape-name","") == struct_name && !ss["owned-fields"].empty())
							{ has_owned = true; break; }
					string elem_free = has_owned
						? "__pln_free_" + struct_name + "(pts[i]);"
						: "free(pts[i]);";

					out << "\nexport func __pln_alloc_" << shape_key
					    << "(int64 n) -> []@!" << struct_name << " {\n"
					    << "    [n]@!" << struct_name << " outer;\n"
					    << "    int64 i = 0;\n"
					    << "    while i < n {\n"
					    << "        " << struct_name << " p;\n"
					    << "        p ->> outer[i];\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "    return outer;\n"
					    << "}\n"
					    << "export func __pln_free_" << shape_key
					    << "([]@!" << struct_name << " pts, int64 n) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < n {\n"
					    << "        " << elem_free << "\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "    free(pts);\n"
					    << "    return;\n"
					    << "}\n";
				}

				// Array allocator/free functions
				for (auto& shape : arr_shapes) {
					string leaf = shape["leaf-type"];
					out << "\nexport func __pln_alloc_arr_arr_" << leaf
					    << "(int64 d0, int64 d1) -> [][]" << leaf << " {\n"
					    << "    [d0]@![]" << leaf << " outer;\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        [d1]" << leaf << " inner;\n"
					    << "        inner ->> outer[i];\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "    return outer;\n"
					    << "}\n"
					    << "export func __pln_free_arr_arr_" << leaf
					    << "([][]" << leaf << " outer, int64 d0) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        free(outer[i]);\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "    free(outer);\n"
					    << "    return;\n"
					    << "}\n";
				}
			}

			if (int ret = runTool({exec_path + "/palan-gen-ast", alloc_pa, "-o", alloc_ast}))
				return ret;
			if (int ret = runTool({exec_path + "/palan-sa", alloc_ast}))
				return ret;
			if (int ret = runTool({exec_path + "/palan-codegen", "--no-entry", alloc_sa}))
				return ret;
			if (int ret = runTool({"as", alloc_s, "-o", alloc_o}))
				return ret;

			obj_files.push_back(alloc_o);
		}
	}

	// ld — link all object files into the final binary. -lc is unconditional:
	// the allocators Palan synthesizes call libc malloc/free, a runtime
	// dependency independent of any header. link_libs then adds one -l per
	// cinclude `link` clause; names are validated at SA ingest, not here.
	// link_libs is a set, so declaration order from source is already lost --
	// harmless for shared libraries (order doesn't gate symbol resolution),
	// but this would need to change if static archives with inter-library
	// dependencies were ever supported.
	vector<string> ldArgv{"ld"};
	for (auto& obj : obj_files) ldArgv.push_back(obj);
	ldArgv.push_back("-lc");
	for (auto& lib : link_libs) ldArgv.push_back("-l" + lib);
	ldArgv.push_back("-dynamic-linker");
	ldArgv.push_back("/lib64/ld-linux-x86-64.so.2");
	ldArgv.push_back("-o");
	ldArgv.push_back(binary_name);
	if (int ret = runTool(ldArgv))
		return ret;

	// When no explicit output is specified, run the binary and remove it.
	// This enables script-style usage: palan script.pa
	if (!output_specified) {
		PlnProcResult r = runProcess({"./" + binary_name});
		fs::remove(binary_name);
		if (r.status == PlnSpawnStatus::Exited)
			return r.exit_code;
		if (r.status == PlnSpawnStatus::SpawnFailed) {	// LCOV_EXCL_START
			// ld just produced this binary; reaching SpawnFailed here needs an
			// OS-level condition (e.g. a noexec mount, disk full) rather than
			// anything reproducible from Palan source under test.
			cerr << PlnBuildMgrMessage::getMessage(
				E_FailedToExecute, "./" + binary_name, strerror(r.err_no)) << endl;
			return 127;
		}	// LCOV_EXCL_STOP
		return -1;
	}

	return 0;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with many local strings/paths; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

string getPalanDirPath()
{
	string home_path = getenv("HOME");
	string palan_path = home_path + "/.palan";
	
	if (fs::exists(palan_path)) {
		if (!fs::is_directory(palan_path)) {
			cerr << PlnBuildMgrMessage::getMessage(E_PalanDirNotDirectory, palan_path) << endl;
			return "";
		}
	}

	// create palan dirs
	fs::create_directories(palan_path + "/work");
	return palan_path;
}

