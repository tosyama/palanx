/// c2ast error case tests
///
/// @file	errorTests.cpp
/// @copyright	2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

// --- CLI errors (IT-0801) ---

TEST(c2ast_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-c2ast");
	ASSERT_NE(out.find("no input file"), string::npos);
}

TEST(c2ast_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-c2ast --help");
	ASSERT_NE(out.find("Usage: palan-c2ast"), string::npos);
}

TEST(c2ast_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-c2ast --version");
	ASSERT_NE(out.find("palan-c2ast"), string::npos);
}

// --- Preprocessor errors (IT-0802) ---

TEST(c2ast_error, error_directive) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_001_error_directive.h");
	ASSERT_NE(out.find("error:"), string::npos);
}

// --- Remaining preprocessor errors (IT-0807) ---

TEST(c2ast_error, include_not_found) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_002_include_not_found.h");
	ASSERT_NE(out.find("No such file or directory"), string::npos);
}

TEST(c2ast_error, endif_without_if) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_003_endif_without_if.h");
	ASSERT_NE(out.find("#endif without #if"), string::npos);
}

TEST(c2ast_error, else_without_if) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_004_else_without_if.h");
	ASSERT_NE(out.find("#else without #if"), string::npos);
}

TEST(c2ast_error, define_no_name) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_005_define_no_name.h");
	ASSERT_NE(out.find("macro name must be an identifier"), string::npos);
}

TEST(c2ast_error, if_expr_extra_tokens) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_006_if_expr_error.h");
	ASSERT_NE(out.find("error:"), string::npos);
}

// --- Preprocessor warnings (IT-0803) ---

TEST(c2ast_error, warning_extra_tokens_after_directive) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_007_extra_tokens.h");
	ASSERT_NE(out.find("warning:"), string::npos);
	ASSERT_EQ(out.find("error:"), string::npos);
}
