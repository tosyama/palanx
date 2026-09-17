/// gen-ast error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

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
