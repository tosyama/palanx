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
#include <functional>
#include <memory>
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
		vector<json> struct_shapes;
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
				string kind = shape.value("shape-kind", "");
				// A bytes shape is a struct the module only declares.
				bool isStruct = kind == "struct" || kind == "bytes";
				string key;
				if (isStruct)
					key = "struct:" + shape["shape-name"].get<string>();
				else
					key = shape["shape-key"].get<string>();
				if (!seen_shapes.insert(key).second) continue;
				if (isStruct)
					struct_shapes.push_back(shape);
				else
					elem_arr_shapes.push_back(shape);
			}
		}

		if (!struct_shapes.empty() || !elem_arr_shapes.empty()) {
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
				    << "void abort(void);\n";
				ofstream out(alloc_pa);
				out << "cinclude \"__allocators.h\";\n";

				map<string, const json*> struct_by_name;
				for (auto& shape : struct_shapes)
					struct_by_name[shape["shape-name"].get<string>()] = &shape;
				// A struct owning nothing comes only as its layout, a bytes shape.
				auto hasOwned = [&](const string& name) {
					return struct_by_name.at(name)->at("shape-kind") == "struct";
				};
				// Copies one struct's storage into another's: a flat struct is a
				// byte copy, one owning sub-objects goes through its copy function.
				auto structCopy = [&](const string& name, const string& dst, const string& src) {
					if (hasOwned(name))
						return "__pln_copy_" + name + "(" + dst + ", " + src + ");";
					return "memcpy(" + dst + ", " + src + ", "
						+ to_string(struct_by_name.at(name)->at("total-size").get<int64_t>()) + ");";
				};

				// Slots are filled with raw C allocations: a borrow slot never takes
				// ownership, and the array's free function releases the elements.
				auto structElemBuild = [&](const string& name, const string& slot, const string& indent) {
					string s = indent + "malloc("
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
				// copied in bulk.
				struct ElemOps {
					string kind;       // the shape entry's element kind
					string inner;      // an inner `[+]T` element's shape key
					string key;        // the element's type symbol
					string type;       // one element's type name in the source
					int64_t size;      // the bytes a slot takes, but a row laid out in place
					int nlens;         // the lengths its helpers take: its levels', then its elements'
					int depth;         // the levels of a row laid out in place
					bool owned;
					bool embedded;     // laid out in the slot itself (`[+]$T`, `[+]$[n]T`)
					string buildElem;  // a slot as the build helper fills it
					string declElem;   // a slot as a local array declares it
					string ownElem;    // a slot as the drop and copy helpers read it
					string slotRun;    // a row block's slots, as `[]` of this; a row laid out in place stands for its elements'
					string fill;       // a row block's slot as its build helper fills it
					string value;      // what push takes
					string popped;     // what pop returns
					string run;        // a copy's source run of elements
					string data;       // the header's data pointer
					shared_ptr<const ElemOps> leaf;   // a row's element
				};
				map<string, const json*> varrElemDescs;
				for (auto& shape : elem_arr_shapes)
					if (shape["shape-kind"] == "varr") varrElemDescs[shape["shape-key"]] = &shape["elem"];
				function<ElemOps(const json&)> opsOf = [&](const json& desc) {
					string kind = desc["kind"];
					ElemOps e;
					e.kind = kind;
					e.inner = desc.value("shape-key", "");
					e.key = desc["key"];
					e.size = desc.value("size", (int64_t)8);
					e.depth = desc.value("depth", 0);
					if (kind == "row" || kind == "embed-row") {
						e.leaf = make_shared<const ElemOps>(opsOf(desc["elem"]));
						const ElemOps& l = *e.leaf;
						e.embedded = kind == "embed-row";
						e.value = "[]" + l.slotRun;
						if (!e.embedded) {
							// A row is a block of its own, which the slot points at.
							e.nlens = 1 + l.nlens;
							e.owned = true;
							e.buildElem = "@!" + l.fill;
							// A local array of `@!@!T` slots must be spelled `[n]@![]@!T`.
							e.declElem = "@![]" + (l.owned ? l.buildElem : l.slotRun);
							e.ownElem = "[]" + l.slotRun;
							e.slotRun = e.ownElem;
							e.fill = e.buildElem;
							e.popped = e.buildElem;
							e.run = "[]" + e.ownElem;
							e.data = "@!" + e.buildElem;
						} else {
							// A row laid out in place is its elements' slots, depth levels of them.
							// pop moves them into a block of their own for the caller.
							e.nlens = e.depth + l.nlens;
							e.owned = l.owned;
							e.type = l.type;
							e.buildElem = l.buildElem;
							e.ownElem = l.ownElem;
							e.slotRun = l.slotRun;
							e.fill = l.fill;
							e.popped = "@!" + l.fill;
							e.run = l.run;
							e.data = l.data;
						}
						return e;
					}
					e.type = kind == "prim" ? desc["type"] : desc["name"];
					e.nlens = kind == "varr" ? opsOf(*varrElemDescs.at(e.inner)).nlens : 0;
					e.owned = kind == "struct" || kind == "varr";
					e.embedded = kind == "embed-struct";
					e.buildElem = "@!" + e.type;
					e.declElem = e.buildElem;
					e.ownElem = e.type;
					e.slotRun = (e.embedded ? "$" : "") + e.type;
					e.fill = e.owned ? e.buildElem : e.type;
					e.value = e.type;
					// The caller takes a struct's storage, which a `@!T` return carries the same way.
					e.popped = e.owned ? "@!" + e.type : e.type;
					e.run = e.owned ? "[]" + e.ownElem : "@" + e.type;
					e.data = e.owned ? "@!@!" + e.type : "@!" + e.type;
					return e;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// The helpers of an element take the lengths it needs as m0, m1, ...
				// and pass the rest on to its element's helpers.
				auto lenNames = [](int n, int from = 0) {
					vector<string> names;
					for (int i = from; i < from + n; i++) names.push_back("m" + to_string(i));
					return names;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto lenParams = [&](int n) {
					string s;
					for (auto& name : lenNames(n)) s += ", int64 " + name;
					return s;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto lenArgs = [](const vector<string>& names, size_t from = 0) {
					string s;
					for (size_t i = from; i < names.size(); i++) s += ", " + names[i];
					return s;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto rest = [](const vector<string>& names, size_t from) {
					return vector<string>(names.begin() + from, names.end());
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// The slots a row laid out in place holds, and the bytes any slot takes.
				auto countOf = [](const ElemOps& e, const vector<string>& names) {
					string s = names[0];
					for (int i = 1; i < e.depth; i++) s += " * " + names[i];
					return s;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				auto sizeOf = [&](const ElemOps& e, const vector<string>& names) {
					if (e.kind != "embed-row") return to_string(e.size);
					return countOf(e, names) + " * " + to_string(e.leaf->size);
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// The address of slot idx of a data pointer, for the byte copies of
				// unowned elements. An embedded struct's `arr[i]` is its address already.
				auto slotAddr = [&](const ElemOps& e, const string& arr, const string& idx, const vector<string>& names) {
					bool isRow = e.kind == "embed-row";
					string at = arr + "[" + idx + (isRow ? " * " + countOf(e, names) : "") + "]";
					bool isAddr = isRow ? e.leaf->embedded : e.embedded;
					return (isAddr ? "" : "@!") + at;
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// An inner `[+]T` element is its header, starting empty. A row's
				// data starts undefined, as an array variable's does; its owned
				// elements are built at once, so no slot is left unset.
				auto elemBuild = [&](const ElemOps& e, const string& slot, const string& indent, const vector<string>& names) {
					if (e.kind == "varr") return indent + "calloc(1, 24) -> " + slot + ";\n";
					if (e.kind == "row") {
						const ElemOps& l = *e.leaf;
						string s = indent + "malloc(" + names[0] + " * " + sizeOf(l, rest(names, 1)) + ") -> " + slot + ";\n";
						if (l.owned) s += indent + "__pln_build_" + l.key + "(" + slot + ", 0, " + names[0] + lenArgs(names, 1) + ");\n";
						return s;
					}
					return structElemBuild(e.type, slot, indent);
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// A row moved out by `->>` leaves NULL in its slot.
				auto elemDrop = [&](const ElemOps& e, const string& slot, const string& indent, const vector<string>& names) {
					if (e.kind == "varr") return "__pln_free_" + e.inner + "(" + slot + lenArgs(names) + ");";
					if (e.kind == "row") {
						const ElemOps& l = *e.leaf;
						if (!l.owned) return "free(" + slot + ");";
						return "if " + slot + " != NULL { __pln_drop_" + l.key + "(" + slot
							+ ", 0, " + names[0] + lenArgs(names, 1) + "); }\n" + indent + "free(" + slot + ");";
					}
					return hasOwned(e.type) ? "__pln_free_" + e.type + "(" + slot + ");" : "free(" + slot + ");";
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				// dst is the slot as its build fills it, a writable reference.
				auto elemSet = [&](const ElemOps& e, const string& dst, const string& src, const vector<string>& names) {
					if (e.kind == "varr")
						return "__pln_copy_" + e.inner + "(" + dst + ", " + src + ".data, " + src + ".len" + lenArgs(names) + ");";
					if (e.kind == "row") {
						const ElemOps& l = *e.leaf;
						if (l.owned) return "__pln_copyrun_" + l.key + "(" + dst + ", " + src + ", " + names[0] + lenArgs(names, 1) + ");";
						return "memcpy(" + dst + ", " + src + ", " + names[0] + " * " + sizeOf(l, rest(names, 1)) + ");";
					}
					return structCopy(e.type, dst, src);
				}; // LCOV_EXCL_LINE -- only exception cleanup is attributed here
				set<string> elemHelpersDone;
				function<void(const ElemOps&)> emitElemHelpers = [&](const ElemOps& e) {
					if (!e.owned || !elemHelpersDone.insert(e.key).second) return;
					if (e.leaf) emitElemHelpers(*e.leaf);
					vector<string> names = lenNames(e.nlens);
					string mp = lenParams(e.nlens), ma = lenArgs(names);
					string head = "\nfunc __pln_build_" + e.key + "([]" + e.buildElem + " slots, int64 from, int64 to" + mp + ") {\n";
					string dropHead = "func __pln_drop_" + e.key + "([]" + e.ownElem + " slots, int64 from, int64 to" + mp + ") {\n";
					string setHead = "func __pln_set_" + e.key + "([]" + e.ownElem + " slots, int64 i, " + e.value + " x" + mp + ") {\n";
					string runHead = "func __pln_copyrun_" + e.key + "([]" + e.ownElem + " dst, []" + e.ownElem + " src, int64 n" + mp + ") {\n";
					if (e.kind == "embed-row") {
						// Rows laid out in place are runs of their elements' slots.
						string l = e.leaf->key, n = countOf(e, names), la = lenArgs(names, e.depth);
						out << head
						    << "    __pln_build_" << l << "(slots, from * " << n << ", to * " << n << la << ");\n"
						    << "}\n"
						    << dropHead
						    << "    __pln_drop_" << l << "(slots, from * " << n << ", to * " << n << la << ");\n"
						    << "}\n"
						    << setHead
						    << "    int64 j = 0;\n"
						    << "    while j < " << n << " {\n"
						    << "        __pln_set_" << l << "(slots, i * " << n << " + j, x[j]" << la << ");\n"
						    << "        j + 1 -> j;\n"
						    << "    }\n"
						    << "}\n"
						    << runHead
						    << "    __pln_copyrun_" << l << "(dst, src, n * " << n << la << ");\n"
						    << "}\n";
						return;
					}
					out << head
					    << "    int64 i = from;\n"
					    << "    while i < to {\n"
					    << elemBuild(e, "slots[i]", "        ", names)
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << dropHead
					    << "    int64 i = from;\n"
					    << "    while i < to {\n"
					    << "        " << elemDrop(e, "slots[i]", "        ", names) << "\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n"
					    << setHead
					    << "    " << elemSet(e, "@!slots[i]", "x", names) << "\n"
					    << "}\n"
					    << runHead
					    << "    int64 i = 0;\n"
					    << "    while i < n {\n"
					    << "        __pln_set_" << e.key << "(dst, i, src[i]" << ma << ");\n"
					    << "        i + 1 -> i;\n"
					    << "    }\n"
					    << "}\n";
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
					// An owned field is declared as the slot its build fills, so
					// this module neither copies nor frees what it is given.
					vector<pair<int64_t, string>> owned_decls;
					for (auto& of : shape.value("owned-fields", json::array()))
						owned_decls.push_back({of["offset"].get<int64_t>(),
							opsOf(of["elem"]).buildElem + " " + of["name"].get<string>()});
					sort(owned_decls.begin(), owned_decls.end());

					out << "\nexport type " << name << " {";
					int64_t pos = 0;
					int pad_no = 0;
					auto pad_to = [&](int64_t end) {
						if (end <= pos) return;
						string pad = "__pln_pad" + to_string(pad_no++);
						out << " $[" << (end - pos) << "]uint8 " << pad << ";";
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

				// Struct allocator/free functions, for structs owning fields
				for (auto& shape : struct_shapes) {
					if (shape["shape-kind"] == "bytes") continue;
					string name = shape["shape-name"];
					struct OwnedField { string slot; ElemOps e; vector<string> lens; };
					vector<OwnedField> owned;
					for (auto& of : shape["owned-fields"]) {
						OwnedField f{of["name"], opsOf(of["elem"]), {}};
						for (auto& len : of["lens"]) f.lens.push_back(to_string(len.get<int64_t>()));
						if (f.e.leaf) emitElemHelpers(*f.e.leaf);
						owned.push_back(move(f));
					}

					// The block is returned as `@!T`: a `T` return would be allocated
					// by this very function.
					out << "\nexport func __pln_alloc_" << name
					    << "() -> @!" << name << " p {\n"
					    << "    malloc(" << shape["total-size"].get<int64_t>() << ") -> p;\n"
					    << "    __pln_init_" << name << "(p);\n"
					    << "}\n";

					// A struct in a stack frame releases only what it owns (fini);
					// a heap one also frees its own block.
					out << "export func __pln_init_" << name << "(@!" << name << " p) {\n";
					for (auto& f : owned) out << elemBuild(f.e, "p." + f.slot, "    ", f.lens);
					out << "}\n"
					    << "export func __pln_fini_" << name << "(" << name << " p) {\n";
					for (auto& f : owned) out << "    " << elemDrop(f.e, "p." + f.slot, "    ", f.lens) << "\n";
					out << "}\n"
					    << "export func __pln_free_" << name
					    << "(" << name << " p) {\n"
					    << "    if (p == NULL) { return; }\n"
					    << "    __pln_fini_" << name << "(p);\n"
					    << "    free(p);\n"
					    << "    return;\n"
					    << "}\n";

					out << "export func __pln_copy_" << name
					    << "(" << name << " dst, " << name << " src) {\n";
					for (auto& [pad, size] : struct_pads[name])
						out << "    memcpy(dst." << pad << ", src." << pad << ", " << size << ");\n";
					for (auto& f : owned)
						out << "    " << elemSet(f.e, "dst." + f.slot, "src." + f.slot, f.lens) << "\n";
					out << "}\n";
				}

				for (auto& shape : elem_arr_shapes) {
					ElemOps e = opsOf(shape["elem"]);
					string key = shape["shape-key"];
					emitElemHelpers(e);
					vector<string> names = lenNames(e.nlens);
					string mp = lenParams(e.nlens), ma = lenArgs(names);
					if (shape["shape-kind"] == "arr") {
						// A fixed array of owned elements, all of them live.
						string buildRun = "[]" + e.buildElem, ownRun = "[]" + e.ownElem;
						out << "\nexport func __pln_init_" << key << "(" << buildRun << " outer, int64 n" << mp << ") {\n"
						    << "    __pln_build_" << e.key << "(outer, 0, n" << ma << ");\n"
						    << "}\n"
						    << "export func __pln_alloc_" << key << "(int64 n" << mp << ") -> []" << e.declElem << " {\n"
						    << "    [n]" << e.declElem << " outer;\n"
						    << "    __pln_init_" << key << "(outer, n" << ma << ");\n"
						    << "    return outer;\n"
						    << "}\n"
						    << "export func __pln_fini_" << key << "(" << ownRun << " pts, int64 n" << mp << ") {\n"
						    << "    __pln_drop_" << e.key << "(pts, 0, n" << ma << ");\n"
						    << "}\n"
						    << "export func __pln_free_" << key << "(" << ownRun << " pts, int64 n" << mp << ") {\n"
						    << "    if (pts == NULL) { return; }\n"
						    << "    __pln_fini_" << key << "(pts, n" << ma << ");\n"
						    << "    free(pts);\n"
						    << "    return;\n"
						    << "}\n"
						    << "export func __pln_copy_" << key << "(" << ownRun << " dst, " << ownRun << " src, int64 n" << mp << ") {\n"
						    << "    __pln_copyrun_" << e.key << "(dst, src, n" << ma << ");\n"
						    << "}\n";
						continue;
					}
					// A variable-length array's header {len, cap, data}. Only the
					// elements below len are live: an owned element is built when the
					// length grows over its slot and dropped when it shrinks below it.
					string hdr = "__pln_" + key;
					string size = sizeOf(e, names);
					out << "\nexport type " << hdr << " { int64 len; int64 cap; " << e.data << " data; };\n";
					out << "export func __pln_init_" << key
					    << "(@!" << hdr << " h, int64 len, int64 cap" << mp << ") {\n"
					    << "    len -> h.len;\n"
					    << "    cap -> h.cap;\n"
					    << "    if cap > 0 { malloc(cap * " << size << ") -> h.data; }\n"
					    << "    else { NULL -> h.data; }\n";
					if (e.owned) out << "    __pln_build_" << e.key << "(h.data, 0, len" << ma << ");\n";
					out << "}\n"
					    << "export func __pln_alloc_" << key
					    << "(int64 len, int64 cap" << mp << ") -> " << hdr << " h {\n"
					    << "    __pln_init_" << key << "(@!h, len, cap" << ma << ");\n"
					    << "}\n"
					    << "export func __pln_fini_" << key << "(" << hdr << " h" << mp << ") {\n";
					if (e.owned) out << "    __pln_drop_" << e.key << "(h.data, 0, h.len" << ma << ");\n";
					out << "    free(h.data);\n"
					    << "}\n"
					    << "export func __pln_free_" << key << "(" << hdr << " h" << mp << ") {\n"
					    << "    if (h == NULL) { return; }\n"
					    << "    __pln_fini_" << key << "(h" << ma << ");\n"
					    << "    free(h);\n"
					    << "    return;\n"
					    << "}\n"
					// Growing doubles the capacity so that adding one element at a
					// time, by push or by resize, reallocates only now and then.
					    << "func __pln_reserve_" << key << "(@!" << hdr << " h, int64 n" << mp << ") {\n"
					    << "    int64 c = h.cap * 2;\n"
					    << "    if c < n { n -> c; }\n"
					    << "    if c < 4 { 4 -> c; }\n"
					    << "    realloc(h.data, c * " << size << ") -> h.data;\n"
					    << "    c -> h.cap;\n"
					    << "}\n"
					    << "export func __pln_push_" << key << "(@!" << hdr << " h, " << e.value << " x" << mp << ") {\n"
					    << "    if h.len == h.cap { __pln_reserve_" << key << "(h, h.len + 1" << ma << "); }\n";
					// The caller hands an inner `[+]T` over, so its header becomes the slot.
					if (e.kind == "varr")
						out << "    @!x -> h.data[h.len];\n";
					else if (e.owned)
						out << "    __pln_build_" << e.key << "(h.data, h.len, h.len + 1" << ma << ");\n"
						    << "    __pln_set_" << e.key << "(h.data, h.len, x" << ma << ");\n";
					else if (e.embedded)
						out << "    memcpy(" << slotAddr(e, "h.data", "h.len", names) << ", x, " << size << ");\n";
					else
						out << "    x -> h.data[h.len];\n";
					out << "    h.len + 1 -> h.len;\n"
					    << "}\n"
					    << "export func __pln_resize_" << key << "(@!" << hdr << " h, int64 n" << mp << ") {\n"
					    << "    if n < 0 { abort(); }\n"
					    << "    if n > h.cap { __pln_reserve_" << key << "(h, n" << ma << "); }\n";
					if (e.owned)
						out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n" << ma << "); }\n"
						    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len" << ma << "); }\n";
					out << "    n -> h.len;\n"
					    << "}\n"
					// The source is any run of elements, which may be h's own
					// (`v -> v`, or a borrow of v copied into v); then n is h's
					// length, so no element is built, freed or moved.
					    << "export func __pln_copy_" << key << "(@!" << hdr << " h, " << e.run << " src, int64 n" << mp << ") {\n"
					    << "    if n > h.cap { __pln_reserve_" << key << "(h, n" << ma << "); }\n";
					if (e.owned)
						out << "    if n > h.len { __pln_build_" << e.key << "(h.data, h.len, n" << ma << "); }\n"
						    << "    else { __pln_drop_" << e.key << "(h.data, n, h.len" << ma << "); }\n"
						    << "    __pln_copyrun_" << e.key << "(h.data, src, n" << ma << ");\n";
					else
						out << "    memmove(h.data, src, n * " << size << ");\n";
					out << "    n -> h.len;\n"
					    << "}\n";
					// An owned element leaves its slot for the caller.
					out << "export func __pln_pop_" << key << "(@!" << hdr << " h" << mp << ") -> " << e.popped << " x {\n"
					    << "    if h.len == 0 { abort(); }\n"
					    << "    h.len - 1 -> h.len;\n";
					if (e.kind == "embed-row")
						out << "    malloc(" << size << ") -> x;\n";
					// Owned elements' slots are pointers, which '@!' cannot address.
					if (e.kind == "embed-row" && e.owned) {
						string n = countOf(e, names);
						out << "    int64 j = 0;\n"
						    << "    while j < " << n << " {\n"
						    << "        h.data[h.len * " << n << " + j] -> x[j];\n"
						    << "        j + 1 -> j;\n"
						    << "    }\n";
					} else if (e.embedded) {
						out << "    memcpy(x, " << slotAddr(e, "h.data", "h.len", names) << ", " << size << ");\n";
					} else {
						out << "    h.data[h.len] -> x;\n";
					}
					out << "}\n";
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

