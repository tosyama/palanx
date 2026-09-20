/// gen-ast error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include "../test-base/testBase.h"

using namespace std;
namespace fs = std::filesystem;

TEST(gen_ast_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast --help");
	ASSERT_NE(out.find("Usage: palan-gen-ast"), string::npos);
}

TEST(gen_ast_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast --version");
	ASSERT_NE(out.find("palan-gen-ast"), string::npos);
}

TEST(gen_ast_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast");
	ASSERT_NE(out.find("no input file"), string::npos);
}

TEST(gen_ast_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast no_such_file.pa");
	ASSERT_NE(out.find("Could not open file"), string::npos);
}

TEST(gen_ast_error, could_not_open_output_file) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa -o /nonexistent/dir/out.json");
	ASSERT_NE(out.find("Could not open output file"), string::npos);
}

TEST(gen_ast_error, syntax_error_with_loc) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_001_syntax_error.pa");
	ASSERT_NE(out.find(":1:"), string::npos);   // loc format
	ASSERT_NE(out.find("error:"), string::npos);
}

TEST(gen_ast_error, c2ast_failed) {
	// IT-2026-09-16-3102: cinclude of a header palan-c2ast cannot read
	// must be a fatal, diagnosed error rather than a silent no-op.
	// execute_c2ast() throws on failure (same runtime_error + main.cpp
	// catch path as E_CouldNotOpenFile), so there is no source location
	// prefix -- just the message, same style as could_not_open_file above.
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_002_c2ast_failed.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
}

TEST(gen_ast_error, link_keyword_typo) {
	// IT-2026-09-16-3106: the token after import_as, when present, must
	// spell "link" -- a misspelling is a diagnosed error (E_ExpectedLinkKeyword),
	// not silently mis-parsed as something else. Thrown from a parser action
	// (same runtime_error + main.cpp catch path as c2ast_failed above), so no
	// source location prefix.
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_003_link_keyword_typo.pa");
	ASSERT_NE(out.find("Expected 'link' keyword"), string::npos);
}

TEST(gen_ast_error, cinclude_sys_path_injection) {
	// IT-2026-09-18-gen-ast-argv-spawn: execute_c2ast used to build a shell
	// command string for a <...> cinclude path. Pre-fix, the shell ran the
	// touch and palan-c2ast -s stdio.h (the substituted remainder) succeeded;
	// post-fix the literal string is not a header palan-c2ast can find.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_004_cinclude_sys_injection.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(gen_ast_error, syscall_as_identifier) {
	// IT-2026-09-19-3201: unlike "link" (context-dependent, IT-3106), "syscall"
	// is a reserved word everywhere, so using it as an identifier must be a
	// diagnosed syntax error. Goes through bison's error path, so it has the
	// usual ":line:" location prefix (fixture's offending line is line 3,
	// after two leading comment lines).
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_006_syscall_as_identifier.pa");
	ASSERT_NE(out.find(":3:"), string::npos);
	ASSERT_NE(out.find("error:"), string::npos);
}

TEST(gen_ast_error, cinclude_local_path_injection) {
	// IT-2026-09-18-gen-ast-argv-spawn: same as cinclude_sys_path_injection,
	// for the "..." local-path branch (fs::path(base_dir) / resolved).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string out = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/error_005_cinclude_local_injection.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_FALSE(fs::exists("PWNED"));
}
