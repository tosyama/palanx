#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(sa_error, block_shadow_var) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/018_block_shadow_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: variable shadowing is forbidden
}

TEST(sa_error, block_shadow_pln_func) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/019_block_shadow_pln_func.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: Palan function shadowing is forbidden
}

TEST(sa_error, func_inner_scope) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/021_func_inner_scope.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: inner is scoped to outer, not visible in caller
}

TEST(sa_error, undefined_function_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/022_undefined_function.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:1: error:"), string::npos);  // loc format: file:line:col: error:
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, undefined_variable_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/023_undefined_variable.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, return_outside_function_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/024_return_outside_function.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Return statement outside of function"), string::npos);
}

TEST(sa_error, incompatible_type_cast) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/025_incompatible_type_cast.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Cannot cast type"), string::npos);
}

TEST(sa_error, invalid_narrowing_init) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/026_invalid_narrowing_init.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Narrowing initialization"), string::npos);
}

TEST(sa_error, export_in_block) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/027_export_in_block.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("not allowed inside a block"), string::npos);
}

TEST(sa_error, export_in_function) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/028_export_in_function.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("not allowed inside a function"), string::npos);
}

TEST(sa_error, multi_ret_bare_return) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/029_multi_ret_bare_return.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Multi-return function requires bare"), string::npos);
}

TEST(sa_error, single_ret_one_expr) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/030_single_ret_one_expr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("one return expression"), string::npos);
}

TEST(sa_error, void_bare_return) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/031_void_bare_return.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Void function requires bare"), string::npos);
}

TEST(sa_error, tuple_undefined_function) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/032_tuple_undefined_function.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, tuple_needs_multi_ret) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/033_tuple_needs_multi_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("multiple return values"), string::npos);
}

TEST(sa_error, tuple_var_count_mismatch) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/034_tuple_var_count_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("does not match return count"), string::npos);
}

TEST(sa_error, import_file_not_found) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/035_import_file_not_found.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Could not open import file"), string::npos);
}
