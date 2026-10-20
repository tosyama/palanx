/// c2ast error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

// --- CLI errors ---

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

// --- Preprocessor errors ---

TEST(c2ast_error, error_directive) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_001_error_directive.h");
	ASSERT_NE(out.find("error:"), string::npos);
}

// --- Remaining preprocessor errors ---

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

// --- Preprocessor warnings ---

TEST(c2ast_error, warning_extra_tokens_after_directive) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_007_extra_tokens.h");
	ASSERT_NE(out.find("warning:"), string::npos);
	ASSERT_EQ(out.find("error:"), string::npos);
}

// --- Additional preprocessor error messages ---

TEST(c2ast_error, invalid_defined_syntax) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_008_invalid_defined.h");
	ASSERT_NE(out.find("invalid syntax in 'defined'"), string::npos);
}

TEST(c2ast_error, elif_without_if) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_009_elif_without_if.h");
	ASSERT_NE(out.find("#elif without #if"), string::npos);
}

TEST(c2ast_error, else_after_else) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_010_else_after_else.h");
	ASSERT_NE(out.find("#else after #else"), string::npos);
}

TEST(c2ast_error, malformed_macro_args) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_011_malformed_macro_args.h");
	ASSERT_NE(out.find("malformed macro argument list"), string::npos);
}

TEST(c2ast_error, va_args_not_last) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_016_va_args_not_last.h");
	ASSERT_NE(out.find("malformed macro argument list"), string::npos);
}

TEST(c2ast_error, vector_size_field) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_017_vector_size_field.h");
	ASSERT_NE(out.find("error_017_vector_size_field.h:1:35: error: attribute 'vector_size' is not supported here"),
		string::npos);
}

TEST(c2ast_error, packed_enum) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_018_packed_enum.h");
	ASSERT_NE(out.find("error_018_packed_enum.h:1:19: error: attribute 'packed' is not supported here"),
		string::npos);
}

TEST(c2ast_error, malformed_attribute) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_019_malformed_attribute.h");
	ASSERT_NE(out.find("error_019_malformed_attribute.h:1:1: error: unexpected token"), string::npos);
}

TEST(c2ast_error, mode_param) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_020_mode_param.h");
	ASSERT_NE(out.find("error_020_mode_param.h:1:29: error: attribute 'mode' is not supported here"), string::npos);
}

TEST(c2ast_error, mode_global) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_021_mode_global.h");
	ASSERT_NE(out.find("error_021_mode_global.h:1:29: error: attribute 'mode' is not supported here"), string::npos);
}

TEST(c2ast_error, mode_struct) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_022_mode_struct.h");
	ASSERT_NE(out.find("error_022_mode_struct.h:1:36: error: attribute 'mode' is not supported here"), string::npos);
}

TEST(c2ast_error, if_missing_colon) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_012_if_missing_colon.h");
	ASSERT_NE(out.find("missing ':' in conditional expression"), string::npos);
}

TEST(c2ast_error, if_invalid_token) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_013_if_invalid_token.h");
	ASSERT_NE(out.find("expression is not valid"), string::npos);
}

TEST(c2ast_error, if_expr_unexpected_end) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_014_if_expr_end.h");
	ASSERT_NE(out.find("unexpected end of expression"), string::npos);
}

TEST(c2ast_error, enum_value_missing) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/error_015_enum_value_missing.h");
	ASSERT_NE(out.find("unexpected token"), string::npos);
}

// --- Input file errors ---

TEST(c2ast_error, input_system_header_not_found) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-c2ast -s nosuch_system_header.h");
	ASSERT_NE(out.find("No such file or directory"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);
}

TEST(c2ast_error, input_local_header_not_found) {
	cleanTestEnv();
	string out = execTestCommand(
		"bin/palan-c2ast ../test/testdata/c2ast/nosuch_local.h");
	ASSERT_NE(out.find("No such file or directory"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);
}
