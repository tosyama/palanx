/// palan build manager tests: modules and calls
///
/// @file moduleTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

TEST(build_mgr, import_basic) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/023_import_basic.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, import_mutual) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/024_import_mutual.pa");
	ASSERT_EQ(output, "10\n13\n");
}

TEST(build_mgr, block_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/045_block_import.pa");
	ASSERT_EQ(output, "4\n");
}

TEST(build_mgr, selective_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/046_selective_import.pa");
	ASSERT_EQ(output, "16\n");
}

TEST(build_mgr, alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/047_alias_import.pa");
	ASSERT_EQ(output, "9\n8\n");
}

TEST(build_mgr, selective_alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/048_selective_alias_import.pa");
	ASSERT_EQ(output, "25\n");
}

TEST(build_mgr, link_import) {
	// This file has no `link` clause -- imported lib_sqrt.pa is the one
	// requesting libm, proving the "libs" union is whole-program, not per-file.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/172_link_import.pa");
	ASSERT_EQ(output, "11\n");
}

TEST(build_mgr, output_name_injection) {
	// The -o argument used to be concatenated unquoted into ld's shell command
	// line. The marker file must NOT exist (injection closed); a file with the
	// exact literal name must exist (the argument reached ld verbatim).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string output = execTestCommand(
		"bin/palan -o 'out/b_$(touch PWNED)' ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");
	ASSERT_FALSE(fs::exists("PWNED"));
	ASSERT_TRUE(fs::exists("out/b_$(touch PWNED)"));
}

TEST(build_mgr, source_path_injection) {
	// The input .pa path used to flow unquoted through fs::weakly_canonical
	// into every pipeline stage's command line. Metacharacter filenames aren't
	// checked into git, so the hostile name is created at runtime.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand(
		"cp ../test/testdata/build-mgr/174_shell_metachar_source.pa 'out/inj_$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan 'out/inj_$(touch PWNED).pa'");
	ASSERT_EQ(output, "Hello World!\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, import_path_injection) {
	// An import path read back out of ast.json used to be concatenated
	// unquoted into the next palan-gen-ast shell command line. A nonexistent
	// metacharacter path is rejected earlier by fs::exists(), so the imported
	// file must actually exist under the hostile name to reach that code path.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand("cp ../test/testdata/build-mgr/175_import_path_injection.pa out/");
	execTestCommand(
		"printf 'export func add(int32 a, int32 b) -> int32 { return a + b; }\\n' "
		"> 'out/$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan out/175_import_path_injection.pa");
	ASSERT_EQ(output, "7\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, import_syscall) {
	// The library module exports no code at all, only a prototype, so this also
	// covers assembling and linking an object with no callable symbol.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/181_import_syscall.pa");
	ASSERT_EQ(output, "import syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_syscall_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/182_import_syscall_alias.pa");
	ASSERT_EQ(output, "alias syscall\nn==14\n");
}

TEST(build_mgr, import_syscall_macro) {
	// The exported syscall's macro-derived number is already folded in the
	// library module's own ast.json (ast.export gets the same walk as the
	// library's own statements), so the importing module needs no cinclude
	// of its own to resolve it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/184_import_syscall_macro.pa");
	ASSERT_EQ(output, "import macro syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_float)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/201_import_float.pa");
	ASSERT_EQ(output, "25.000000 1.500000\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}

TEST(build_mgr, method_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/262_method_call.pa");
	ASSERT_EQ(output, "121 5\n7\n1 1\n");
}

TEST(build_mgr, import_type) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/263_import_type.pa");
	ASSERT_EQ(output, "25.0\n3.0 7 5\n1.0 0 5\n");
}

TEST(build_mgr, import_type_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/264_import_type_field.pa");
	ASSERT_EQ(output, "1.0 2.0 7 3.0 5 4.0\n");
}

TEST(build_mgr, import_mutual_type) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/266_import_mutual_type.pa");
	ASSERT_EQ(output, "4.5 4.0 2 9\n");
}

TEST(build_mgr, chain_call_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_call_bin "
		"../test/testdata/build-mgr/281_chain_call_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_call_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_call_bin");
	ASSERT_EQ(output, "0.6 0.0 0.8\n"
	                  "1.4\n"
	                  "9.0\n"
	                  "105\n"
	                  "2\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 7) << "expected 7 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, chain_lit_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_lit_bin "
		"../test/testdata/build-mgr/282_chain_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_lit_bin");
	ASSERT_EQ(output, "2.0 4.0 6.0\n"
	                  "18.0\n"
	                  "7\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 5) << "expected 5 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, block_type_scope_mtrace) {
	// Same-named block-local structs of different layouts each get their own
	// allocator.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_block_type_scope_bin "
		"../test/testdata/build-mgr/285_block_type_scope_mtrace.pa"), "");

	string traceFile = "/tmp/palan_block_type_scope_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_block_type_scope_bin");
	ASSERT_EQ(output, "5 2\n7\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_GT(allocs, 0);
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, import_private_type) {
	cleanTestEnv();
	// Each module's P gets its own allocators: copying lib's [2]P with main's
	// 4-byte copy would print "5 3 0".
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/303_import_private_type.pa");
	ASSERT_EQ(output, "5 3 4\n");
}

TEST(build_mgr, import_private_dep_type) {
	cleanTestEnv();
	// Q brings lib's unexported P along; main's own P does not clash with it.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/304_import_private_dep_type.pa");
	ASSERT_EQ(output, "6 7\n");
}

TEST(build_mgr, import_private_alias_type) {
	cleanTestEnv();
	// PP names lib's P wherever it is written, not main's P of the same name.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/305_import_private_alias_type.pa");
	ASSERT_EQ(output, "8 9\n");
}

TEST(build_mgr, qualified_type_entries) {
	cleanTestEnv();
	// V.P reaches lib's L through an alias chain, a parameter, a tapple, a
	// cast and sizeof alike, never main's L.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/306_qualified_type_entries.pa");
	ASSERT_EQ(output, "5 6 2 8 16\n");
}

TEST(build_mgr, import_hidden_type_name_reuse) {
	cleanTestEnv();
	// main's R coexists with lib's R, written only as V.R or reached through S.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/307_import_hidden_type_name_reuse.pa");
	ASSERT_EQ(output, "1 2 30000000000\n7 5\n");
}

TEST(build_mgr, overload_symbols) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/320_overload_symbols.pa");
	ASSERT_EQ(output, "int 3\nfloat 2.5\n8 3.0\n1 3 4 5 1\n");
}

TEST(build_mgr, overload_modules) {
	cleanTestEnv();
	// A function exported under a libc name does not take the place of the
	// libc one, and two modules can export the same function.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/321_overload_modules.pa");
	ASSERT_EQ(output, "8 3.0 12\n1 2\n5 100\n");
}

TEST(build_mgr, nested_func_symbols) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/322_nested_func_symbols.pa");
	ASSERT_EQ(output, "5000\n60000\n1 2 430\n");
}

TEST(build_mgr, chain_method_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/323_chain_method_call.pa");
	// The chained value is evaluated before the receiver's index.
	ASSERT_EQ(output, "324\n1 0 10\n8 7\n11\n8\n10 10 10\n");
}

TEST(build_mgr, chain_method_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_method_bin "
		"../test/testdata/build-mgr/324_chain_method_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_method_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_method_bin");
	ASSERT_EQ(output, "30\n34\n40 40 2\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 10) << "expected 10 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

// The entry is chosen at link time: an imported module's top level runs only
// when that module is the entry, and its object is the same either way.
TEST(build_mgr, entry_at_link) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	string libObj = string(getenv("HOME")) + "/.palan/work" + dir + "/lib_entry_demo.pa.o";

	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/358_entry_at_link.pa"), "15\n");
	fs::copy_file(libObj, "out/lib_entry_demo_imported.o");
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_entry_demo.pa"), "demo 12\n");
	ASSERT_EQ(execTestCommand("cmp out/lib_entry_demo_imported.o " + libObj), "");
}

TEST(build_mgr, implicit_args) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/args_bin ../test/testdata/build-mgr/359_args.pa"), "");
	ASSERT_EQ(execTestCommand("out/args_bin"), "1\nrebound\n");
	ASSERT_EQ(execTestCommand("out/args_bin x 'y z'"), "3\n1:x\n2:y z\nrebound\n");

	// An imported module's top level reads `args` only when it is the entry.
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/360_args_import.pa"), "hello main\n");
	ASSERT_EQ(execTestCommand("bin/palan -o out/lib_args_bin ../test/testdata/build-mgr/lib_args_demo.pa"), "");
	ASSERT_EQ(execTestCommand("out/lib_args_bin a b"), "hello a\nhello b\n");
}

// Without -o, everything after the source goes to the program, options too,
// and the executable is kept in the working directory, not the current one.
TEST(build_mgr, run_args) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	string bin = string(getenv("HOME")) + "/.palan/work" + dir + "/359_args.pa.out";
	fs::remove(bin);
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/359_args.pa x -o 'y z'"),
		"4\n1:x\n2:-o\n3:y z\nrebound\n");
	EXPECT_TRUE(fs::exists(bin));
	EXPECT_FALSE(fs::exists("a.out"));
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_args_demo.pa a"), "hello a\n");
}

// The header of `args` is in the frame: only the copy is allocated and freed.
TEST(build_mgr, implicit_args_mtrace) {
	expectMtraceRun("361_args_mtrace", "4 a end\n", 2, " a b");
}

// A callback C calls with fixed arguments (qsort's comparator) reaches what it
// needs through globals.
TEST(build_mgr, global_var) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/370_global.pa"),
		"1 3 5\n5 3 1\n1 13 4\n7\n");
}

// What a global owns lives as long as the process: only the value it was
// moved away from is freed.
TEST(build_mgr, global_var_mtrace) {
	cleanTestEnv();
	string bin = "/tmp/palan_global_mtrace_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/371_global_mtrace.pa"), "");
	string traceFile = "/tmp/palan_global_mtrace.log";
	ASSERT_EQ(execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin), "6\n15\n");
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 2);
	EXPECT_EQ(frees, 1);
}

TEST(build_mgr, import_global_var) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/372_import_global.pa");
	ASSERT_NE(out.find("372_import_global.pa:1:1: error: cannot import a module whose functions use its global variable 'x'"),
		string::npos) << out;
}
