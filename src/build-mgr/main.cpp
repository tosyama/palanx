/// Palan Build manager
/// Command line tool main.
///
/// @file main.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <vector>
#include <set>
#include <map>
#include <algorithm>
#include <cstring>
#include <getopt.h>
#include <filesystem>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnBuildMgrMessage.h"
#include "../common/PlnDefs.h"
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
	set<string> link_objs;

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
		vector<json> arr_arr_struct_shapes;
		// Arrays of any element ("arr", "varr"), in the order SA met them:
		// an element's own array helpers come before the arrays holding it.
		vector<json> elem_arr_shapes;
		for (auto& ast_file : ast_files) {
			string base = ast_file.substr(0, ast_file.size() - 9);
			ifstream f(base + ".sa.json");
			json sa = json::parse(f);
			// SA always emits "libs" (possibly empty), so no contains() guard
			// is needed. Read before the alloc-shapes continue below: a
			// module can carry libs without carrying alloc-shapes.
			for (auto& l : sa["libs"]) link_libs.insert(l.get<string>());
			for (auto& o : sa["objs"]) link_objs.insert(o.get<string>());
			if (!sa.contains("alloc-shapes")) continue;
			for (auto& shape : sa["alloc-shapes"]) {
				string key;
				if (shape.value("shape-kind", "") == "struct")
					key = "struct:" + shape["shape-name"].get<string>();
				else
					key = shape["shape-key"].get<string>();
				if (!seen_shapes.insert(key).second) continue;
				string kind = shape.value("shape-kind", "");
				if (kind == "struct")
					struct_shapes.push_back(shape);
				else if (kind == "arr-arr-struct")
					arr_arr_struct_shapes.push_back(shape);
				else if (kind == "arr" || kind == "varr")
					elem_arr_shapes.push_back(shape);
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

		if (!arr_shapes.empty() || !struct_shapes.empty() || !elem_arr_shapes.empty()) {
			string workdir = fs::path(ast_files[0]).parent_path().string();
			string alloc_pa  = workdir + "/" PALAN_ALLOCATORS_MODULE ".pa";
			string alloc_ast = alloc_pa + ".ast.json";
			string alloc_sa  = alloc_pa + ".sa.json";
			string alloc_s   = alloc_pa + ".s";
			string alloc_o   = alloc_pa + ".o";

			{
				// The struct types below are named after the program's own,
				// and a system header could bring a C type of any of those
				// names into this module, so it declares only what it calls.
				// With no `link` clause, its sa.json never contributes to
				// link_libs and need not join the aggregation loop above.
				ofstream hdr(workdir + "/__allocators.h");
				hdr << "#define NULL ((void *)0)\n"
				    << "void *malloc(unsigned long size);\n"
				    << "void *calloc(unsigned long nmemb, unsigned long size);\n"
				    << "void free(void *ptr);\n"
				    << "void *memcpy(void *dest, const void *src, unsigned long n);\n"
				    << "void *memmove(void *dest, const void *src, unsigned long n);\n"
				    << "void *realloc(void *ptr, unsigned long size);\n"
				    << "void *memset(void *s, int c, unsigned long n);\n"
				    << "void abort(void);\n";
				ofstream out(alloc_pa);
				out << "cinclude \"__allocators.h\";\n";

				map<string, const json*> struct_by_name;
				for (auto& shape : struct_shapes)
					struct_by_name[shape["shape-name"].get<string>()] = &shape;
				auto hasOwned = [&](const string& name) {
					const json& ss = *struct_by_name.at(name);
					return !ss["owned-fields"].empty() || !ss["owned-array-fields"].empty();
				};
				// Copies one struct's storage into another's: a flat struct is a
				// byte copy, one owning sub-objects goes through its copy function.
				auto structCopy = [&](const string& name, const string& dst, const string& src) {
					if (hasOwned(name))
						return "__pln_copy_" + name + "(" + dst + ", " + src + ");";
					return "memcpy(" + dst + ", " + src + ", "
						+ to_string(struct_by_name.at(name)->at("total-size").get<int64_t>()) + ");";
				};

				// Struct type declarations (leaf-first order from SA).
				// Only the owned pointer fields are declared, at their original
				// offsets; every other byte range becomes opaque padding. The
				// allocators touch nothing else, and the full field list cannot
				// be re-expressed as Palan source in general (C unions, synthesized
				// anonymous tags, embedded types not declared in this module).
				// Exported, because SA keeps only an exported type's name, and
				// these must keep the program-wide names the shapes carry.
				// Byte ranges of each struct outside its owned pointer fields,
				// declared as __pln_padN fields and copied as they are.
				map<string, vector<pair<string, int64_t>>> struct_pads;
				for (auto& shape : struct_shapes) {
					string name = shape["shape-name"];
					vector<pair<int64_t, string>> owned_decls;
					for (auto& of : shape["owned-fields"])
						owned_decls.push_back({of["offset"].get<int64_t>(),
							of["struct-name"].get<string>() + " " + of["name"].get<string>()});
					for (auto& af : shape["owned-array-fields"])
						owned_decls.push_back({af["offset"].get<int64_t>(),
							"[" + to_string(af["count"].get<int64_t>()) + "]"
							+ af["leaf-name"].get<string>() + " " + af["name"].get<string>()});
					sort(owned_decls.begin(), owned_decls.end());

					out << "\nexport type " << name << " {";
					int64_t pos = 0;
					int pad_no = 0;
					auto pad_to = [&](int64_t end) {
						if (end <= pos) return;
						string pad = "__pln_pad" + to_string(pad_no++);
						out << " [" << (end - pos) << "]$uint8 " << pad << ";";
						struct_pads[name].push_back({pad, end - pos});
					};
					for (auto& [offset, decl] : owned_decls) {
						pad_to(offset);
						out << " " << decl << ";";
						pos = offset + 8;
					}
					pad_to(shape["total-size"].get<int64_t>());
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
					    << "() -> " << name << " p {\n";
					if (hasOwned(name))
						out << "    __pln_init_" << name << "(@!p);\n";
					out << "}\n";

					if (hasOwned(name))
						out << "export func __pln_init_" << name
						    << "(@!" << name << " p) {\n";
					for (auto& of : owned) {
						string oname = of["name"];
						string sname = of["struct-name"];
						out << "    __pln_alloc_" << sname << "() ->> p." << oname << ";\n";
					}
					for (auto& af : ownedArr) {
						string fname = af["name"];
						int64_t count = af["count"].get<int64_t>();
						if (af["elem-kind"] == "prim") {
							string leaf = af["leaf-name"];
							out << "    __pln_alloc_arr_prim_" << leaf << "(" << count
							    << ") ->> p." << fname << ";\n";
						} else { // struct leaf
							out << "    __pln_alloc_" << af["shape-key"].get<string>() << "(" << count
							    << ") ->> p." << fname << ";\n";
						}
					}
					if (hasOwned(name))
						out << "}\n";

					// A struct in a stack frame releases only what it owns (fini);
					// a heap one also frees its own block.
					if (hasOwned(name))
						out << "export func __pln_fini_" << name
						    << "(" << name << " p) {\n";
					for (auto& af : ownedArr) {
						string fname = af["name"];
						if (af["elem-kind"] == "prim") {
							string leaf = af["leaf-name"];
							out << "    __pln_free_arr_prim_" << leaf << "(p." << fname << ");\n";
						} else { // struct leaf
							int64_t count = af["count"].get<int64_t>();
							out << "    __pln_free_" << af["shape-key"].get<string>() << "(p." << fname << ", " << count << ");\n";
						}
					}
					for (auto& of : owned) {
						string oname = of["name"];
						string sname = of["struct-name"];
						out << "    __pln_free_" << sname << "(p." << oname << ");\n";
					}
					if (hasOwned(name))
						out << "}\n";
					out << "export func __pln_free_" << name
					    << "(" << name << " p) {\n"
					    << "    if (p == NULL) { return; }\n";
					if (hasOwned(name))
						out << "    __pln_fini_" << name << "(p);\n";
					out << "    free(p);\n"
					    << "    return;\n"
					    << "}\n";

					out << "export func __pln_copy_" << name
					    << "(" << name << " dst, " << name << " src) {\n";
					for (auto& [pad, size] : struct_pads[name])
						out << "    memcpy(dst." << pad << ", src." << pad << ", " << size << ");\n";
					for (auto& of : owned) {
						string oname = of["name"];
						out << "    " << structCopy(of["struct-name"].get<string>(), "dst." + oname, "src." + oname) << "\n";
					}
					for (auto& af : ownedArr) {
						string fname = af["name"];
						int64_t count = af["count"].get<int64_t>();
						if (af["elem-kind"] == "prim")
							out << "    memcpy(dst." << fname << ", src." << fname << ", "
							    << count * af["elem-size"].get<int64_t>() << ");\n";
						else
							out << "    __pln_copy_" << af["shape-key"].get<string>()
							    << "(dst." << fname << ", src." << fname << ", " << count << ");\n";
					}
					out << "}\n";
				}

				// Slots are filled with raw C allocations: a borrow slot never takes
				// ownership, and the array's free function releases the elements.
				auto structElemBuild = [&](const string& name, const string& slot, const string& indent) {
					string s = indent + "calloc(1, "
						+ to_string(struct_by_name.at(name)->at("total-size").get<int64_t>())
						+ ") -> " + slot + ";\n";
					if (hasOwned(name))
						s += indent + "__pln_init_" + name + "(" + slot + ");\n";
					return s;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here

				// How the generated code handles one element of an array. An owned
				// element is built when its slot comes into use and dropped when it
				// leaves, through helpers named after the element's type that take
				// the slots as `[]@!T` to build and as `[]T` to free and copy, the
				// views of a struct array's own. Any other element is plain bytes,
				// zeroed and copied in bulk.
				struct ElemOps {
					string key;        // the element's type symbol
					string type;       // one element's type name in the source
					int64_t size;      // the bytes a slot takes
					bool owned;
					bool embedded;     // laid out in the slot itself (`[+]$T`)
					string buildElem;  // a slot as the build helper fills it
					string ownElem;    // a slot as the drop and copy helpers read it
					string value;      // what push takes
					string popped;     // what pop returns
					string run;        // a copy's source run of elements
					string data;       // the header's data pointer
				};
				auto opsOf = [&](const json& desc) {
					string kind = desc["kind"];
					ElemOps e;
					e.key = desc["key"];
					e.type = kind == "prim" ? desc["type"] : desc["name"];
					e.size = desc["size"].get<int64_t>();
					e.owned = kind == "struct";
					e.embedded = kind == "embed-struct";
					e.buildElem = "@!" + e.type;
					e.ownElem = e.type;
					e.value = e.type;
					// The caller takes a struct's storage, which a `@!T` return carries the same way.
					e.popped = e.owned ? "@!" + e.type : e.type;
					e.run = e.owned ? "[]" + e.ownElem : "@" + e.type;
					e.data = e.owned ? "@!@!" + e.type : "@!" + e.type;
					return e;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto elemBuild = [&](const ElemOps& e, const string& slot, const string& indent) {
					return structElemBuild(e.type, slot, indent);
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto elemDrop = [&](const ElemOps& e, const string& slot) {
					return hasOwned(e.type) ? "__pln_free_" + e.type + "(" + slot + ");" : "free(" + slot + ");";
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto elemSet = [&](const ElemOps& e, const string& dst, const string& src) {
					return structCopy(e.type, dst, src);
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				set<string> elemHelpersDone;
				auto emitElemHelpers = [&](const ElemOps& e) {
					if (!e.owned || !elemHelpersDone.insert(e.key).second) return;
					out << "\nfunc __pln_build_" << e.key << "([]" << e.buildElem << " slots, int64 from, int64 to) {\n"
					    << "    int64 i = from;\n"
					    << "    while i < to {\n"
					    << elemBuild(e, "slots[i]", "        ")
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "func __pln_drop_" << e.key << "([]" << e.ownElem << " slots, int64 from, int64 to) {\n"
					    << "    int64 i = from;\n"
					    << "    while i < to {\n"
					    << "        " << elemDrop(e, "slots[i]") << "\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "func __pln_set_" << e.key << "([]" << e.ownElem << " slots, int64 i, " << e.value << " x) {\n"
					    << "    " << elemSet(e, "slots[i]", "x") << "\n"
					    << "}\n"
					    << "func __pln_copyrun_" << e.key << "([]" << e.ownElem << " dst, []" << e.ownElem << " src, int64 n) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < n {\n"
					    << "        __pln_set_" << e.key << "(dst, i, src[i]);\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n";
				};

				for (auto& shape : elem_arr_shapes) {
					ElemOps e = opsOf(shape["elem"]);
					string key = shape["shape-key"];
					emitElemHelpers(e);
					if (shape["shape-kind"] == "arr") {
						// A fixed array of owned elements, all of them live.
						string buildRun = "[]" + e.buildElem, ownRun = "[]" + e.ownElem;
						out << "\nexport func __pln_init_" << key << "(" << buildRun << " outer, int64 n) {\n"
						    << "    __pln_build_" << e.key << "(outer, 0, n);\n"
						    << "}\n"
						    << "export func __pln_alloc_" << key << "(int64 n) -> " << buildRun << " {\n"
						    << "    [n]" << e.buildElem << " outer;\n"
						    << "    __pln_init_" << key << "(outer, n);\n"
						    << "    return outer;\n"
						    << "}\n"
						    << "export func __pln_fini_" << key << "(" << ownRun << " pts, int64 n) {\n"
						    << "    __pln_drop_" << e.key << "(pts, 0, n);\n"
						    << "}\n"
						    << "export func __pln_free_" << key << "(" << ownRun << " pts, int64 n) {\n"
						    << "    if (pts == NULL) { return; }\n"
						    << "    __pln_fini_" << key << "(pts, n);\n"
						    << "    free(pts);\n"
						    << "    return;\n"
						    << "}\n"
						    << "export func __pln_copy_" << key << "(" << ownRun << " dst, " << ownRun << " src, int64 n) {\n"
						    << "    __pln_copyrun_" << e.key << "(dst, src, n);\n"
						    << "}\n";
						continue;
					}
					// A variable-length array's header {len, cap, data}. The header
					// starts zeroed (calloc or the frame's memset), so data stays
					// NULL while the capacity is 0. Only the elements below len are
					// live: an owned element is built when the length grows over its
					// slot and dropped when it shrinks below it.
					string hdr = "__pln_" + key;
					int64_t size = e.size;
					out << "\nexport type " << hdr << " { int64 len; int64 cap; " << e.data << " data; };\n";
					out << "export func __pln_init_" << key
					    << "(@!" << hdr << " h, int64 len, int64 cap) {\n"
					    << "    len -> h.len;\n"
					    << "    cap -> h.cap;\n"
					    << "    if cap > 0 { calloc(cap, " << size << ") -> h.data; }\n";
					if (e.owned) out << "    __pln_build_" << e.key << "(h.data, 0, len);\n";
					out << "}\n"
					    << "export func __pln_alloc_" << key
					    << "(int64 len, int64 cap) -> " << hdr << " h {\n"
					    << "    __pln_init_" << key << "(@!h, len, cap);\n"
					    << "}\n"
					    << "export func __pln_fini_" << key << "(" << hdr << " h) {\n";
					if (e.owned) out << "    __pln_drop_" << e.key << "(h.data, 0, h.len);\n";
					out << "    free(h.data);\n"
					    << "}\n"
					    << "export func __pln_free_" << key << "(" << hdr << " h) {\n"
					    << "    if (h == NULL) { return; }\n"
					    << "    __pln_fini_" << key << "(h);\n"
					    << "    free(h);\n"
					    << "    return;\n"
					    << "}\n"
					// Growing doubles the capacity so that adding one element at a
					// time, by push or by resize, reallocates only now and then.
					    << "func __pln_reserve_" << key << "(@!" << hdr << " h, int64 n) {\n"
					    << "    int64 c = h.cap * 2;\n"
					    << "    if c < n { n -> c; }\n"
					    << "    if c < 4 { 4 -> c; }\n"
					    << "    realloc(h.data, c * " << size << ") -> h.data;\n"
					    << "    c -> h.cap;\n"
					    << "}\n"
					    << "export func __pln_push_" << key << "(@!" << hdr << " h, " << e.value << " x) {\n"
					    << "    if h.len == h.cap { __pln_reserve_" << key << "(h, h.len + 1); }\n";
					if (e.owned)
						out << "    __pln_build_" << e.key << "(h.data, h.len, h.len + 1);\n"
						    << "    __pln_set_" << e.key << "(h.data, h.len, x);\n";
					else if (e.embedded)
						out << "    memcpy(h.data[h.len], x, " << size << ");\n";
					else
						out << "    x -> h.data[h.len];\n";
					out << "    h.len + 1 -> h.len;\n"
					    << "}\n"
					    << "export func __pln_resize_" << key << "(@!" << hdr << " h, int64 n) {\n"
					    << "    if n < 0 { abort(); }\n"
					    << "    if n > h.cap { __pln_reserve_" << key << "(h, n); }\n";
					if (e.owned)
						out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n); }\n"
						    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len); }\n";
					else
					// Shrinking keeps the old elements past the length, so growing
					// zeroes what it brings back. `h.data[i]` of an embedded element
					// is its address already.
						out << "    if n > h.len { memset(" << (e.embedded ? "" : "@!") << "h.data[h.len], 0, (n - h.len) * " << size << "); }\n";
					out << "    n -> h.len;\n"
					    << "}\n"
					// The source is any run of elements, which may be h's own
					// (`v -> v`, or a borrow of v copied into v); then n is h's
					// length, so no element is built, freed or moved.
					    << "export func __pln_copy_" << key << "(@!" << hdr << " h, " << e.run << " src, int64 n) {\n"
					    << "    if n > h.cap { __pln_reserve_" << key << "(h, n); }\n";
					if (e.owned)
						out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n); }\n"
						    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len); }\n"
						    << "    __pln_copyrun_" << e.key << "(h.data, src, n);\n";
					else
						out << "    memmove(h.data, src, n * " << size << ");\n";
					out << "    n -> h.len;\n"
					    << "}\n";
					// An owned element leaves its slot for the caller.
					out << "export func __pln_pop_" << key << "(@!" << hdr << " h) -> " << e.popped << " x {\n"
					    << "    if h.len == 0 { abort(); }\n"
					    << "    h.len - 1 -> h.len;\n";
					if (e.embedded)
						out << "    memcpy(x, h.data[h.len], " << size << ");\n";
					else
						out << "    h.data[h.len] -> x;\n";
					out << "}\n";
				}

				// Owned 2D struct array: each row is an owned struct array from above
				for (auto& shape : arr_arr_struct_shapes) {
					string struct_name = shape["struct-name"];
					string shape_key   = shape["shape-key"];
					string row_key     = shape["row-key"];
					out << "\nexport func __pln_init_" << shape_key
					    << "([]@![]@!" << struct_name << " outer, int64 d0, int64 d1) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        calloc(d1, 8) -> outer[i];\n"
					    << "        int64 j = 0;\n"
					    << "        while j < d1 {\n"
					    << structElemBuild(struct_name, "outer[i][j]", "            ")
					    << "            j + 1 -> j;\n"
					    << "        }\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "export func __pln_alloc_" << shape_key
					    << "(int64 d0, int64 d1) -> []@![]@!" << struct_name << " {\n"
					    << "    [d0]@![]@!" << struct_name << " outer;\n"
					    << "    __pln_init_" << shape_key << "(outer, d0, d1);\n"
					    << "    return outer;\n"
					    << "}\n"
					    << "export func __pln_fini_" << shape_key
					    << "([][]" << struct_name << " outer, int64 d0, int64 d1) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        __pln_free_" << row_key << "(outer[i], d1);\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "export func __pln_free_" << shape_key
					    << "([][]" << struct_name << " outer, int64 d0, int64 d1) {\n"
					    << "    if (outer == NULL) { return; }\n"
					    << "    __pln_fini_" << shape_key << "(outer, d0, d1);\n"
					    << "    free(outer);\n"
					    << "    return;\n"
					    << "}\n"
					    << "export func __pln_copy_" << shape_key
					    << "([][]" << struct_name << " dst, [][]" << struct_name
					    << " src, int64 d0, int64 d1) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        __pln_copy_" << row_key << "(dst[i], src[i], d1);\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n";
				}

				// Array allocator/free functions
				for (auto& shape : arr_shapes) {
					string leaf = shape["leaf-type"];
					out << "\nexport func __pln_init_arr_arr_" << leaf
					    << "([]@![]" << leaf << " outer, int64 d0, int64 d1) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        malloc(d1 * " << shape["leaf-size"].get<int64_t>() << ") -> outer[i];\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "export func __pln_alloc_arr_arr_" << leaf
					    << "(int64 d0, int64 d1) -> []@![]" << leaf << " {\n"
					    << "    [d0]@![]" << leaf << " outer;\n"
					    << "    __pln_init_arr_arr_" << leaf << "(outer, d0, d1);\n"
					    << "    return outer;\n"
					    << "}\n"
					    << "export func __pln_fini_arr_arr_" << leaf
					    << "([][]" << leaf << " outer, int64 d0) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        free(outer[i]);\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << "export func __pln_free_arr_arr_" << leaf
					    << "([][]" << leaf << " outer, int64 d0) {\n"
					    << "    if (outer == NULL) { return; }\n"
					    << "    __pln_fini_arr_arr_" << leaf << "(outer, d0);\n"
					    << "    free(outer);\n"
					    << "    return;\n"
					    << "}\n"
					    << "export func __pln_copy_arr_arr_" << leaf
					    << "([][]" << leaf << " dst, [][]" << leaf << " src, int64 d0, int64 d1) {\n"
					    << "    int64 i = 0;\n"
					    << "    while i < d0 {\n"
					    << "        memcpy(dst[i], src[i], d1 * " << shape["leaf-size"].get<int64_t>() << ");\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
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
	for (auto& obj : link_objs) ldArgv.push_back(obj);
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

