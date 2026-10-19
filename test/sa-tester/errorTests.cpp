/// semantic analyzer error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(sa_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-sa --help");
	ASSERT_NE(out.find("Usage: palan-sa"), string::npos);
}

TEST(sa_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-sa --version");
	ASSERT_NE(out.find("palan-sa"), string::npos);
}

TEST(sa_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-sa");
	ASSERT_NE(out.find("no input file"), string::npos);
}

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
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int8'"), string::npos);
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
	ASSERT_NE(sa.find("Function with named return values requires bare"), string::npos);
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

TEST(sa_error, embed_arr_unsized_inner) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_050_embed_arr_unsized_inner.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("inner dimension"), string::npos);
}

TEST(sa_error, embed_arr_inner_size_mismatch) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_051_embed_arr_inner_size_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("mismatch"), string::npos);
}

TEST(sa_error, void_call_as_expr) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_040_void_call_as_expr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, break_outside_loop) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_041_break_outside_loop.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Break statement outside of loop"), string::npos);
}

TEST(sa_error, continue_outside_loop) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_042_continue_outside_loop.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Continue statement outside of loop"), string::npos);
}

TEST(sa_error, float_modulo) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_043_float_modulo.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("'%' operator is not supported for float types"), string::npos);
}

TEST(sa_error, array_size_not_integer) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_044_array_size_float.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("integer"), string::npos);
}

TEST(sa_error, arr_index_float_idx) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_045_arr_index_float_idx.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Array index expression must be an integer type"), string::npos);
}

TEST(sa_error, arr_not_array_type) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_046_arr_not_array_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Expression is not an array (pointer) type"), string::npos);
}

TEST(sa_error, arr_assign_void) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_047_arr_assign_void.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, unsized_arr_var_decl) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_048_unsized_arr_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Unsized array type cannot be used in variable declaration"), string::npos);
}

TEST(sa_error, float_logical_op) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_049_float_logical_op.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Logical operator operand must be an integer type"), string::npos);
}

TEST(sa_error, float_bitand) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_121_float_bitand.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Bitwise operator operand must be an integer type"), string::npos);
}

TEST(sa_error, float_bitnot) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_122_float_bitnot.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Bitwise operator operand must be an integer type"), string::npos);
}

TEST(sa_error, embed_arr_variable_inner_arg) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_052_embed_arr_variable_inner_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("mismatch"), string::npos);
	ASSERT_NE(sa.find("variable"), string::npos);
}

TEST(sa_error, embed_arr_float_size) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_053_embed_arr_float_size.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Array size expression must be an integer type"), string::npos);
}

TEST(sa_error, unqualified_alias_call)
{
	cleanTestEnv();
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("bin/palan-gen-ast ../test/testdata/sa/error_054_unqualified_alias_call.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("requires a module alias qualifier"), string::npos);
}

TEST(sa_error, ambiguous_call)
{
	cleanTestEnv();
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import2.pa -o out/lib_sa_import2.pa.ast.json");
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("bin/palan-gen-ast ../test/testdata/sa/error_055_ambiguous_call.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Ambiguous function call"), string::npos);
}

TEST(sa_error, unknown_alias)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("bin/palan-gen-ast ../test/testdata/sa/error_056_unknown_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined variable or module alias 'X'"), string::npos);
}

TEST(sa_error, import_block_scope_out)
{
	cleanTestEnv();
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("bin/palan-gen-ast ../test/testdata/sa/error_057_import_block_scope_out.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, cinclude_unqualified_alias)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("bin/palan-gen-ast ../test/testdata/sa/error_058_cinclude_unqualified_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("requires a module alias qualifier"), string::npos);
}

TEST(sa_error, float_logical_not)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_059_float_logical_not.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Logical operator operand must be an integer type"), string::npos);
}

TEST(sa_error, unknown_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_060_unknown_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, unknown_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_061_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, non_prim_struct_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_062_non_prim_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unsupported struct field type"), string::npos);
}

TEST(sa_error, recursive_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_069_recursive_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct 'A' recursively contains itself"), string::npos);
}

TEST(sa_error, field_assign_undef_var)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_063_field_assign_undef_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, field_assign_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_064_field_assign_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_unknown_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_065_field_assign_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, field_assign_void_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_066_field_assign_void_value.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, field_access_undef_var)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_067_field_access_undef_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, field_access_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_068_field_access_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, inline_as_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_070_inline_as_value.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":5:1: error: cannot convert 'Point' to 'int64'."), string::npos);
}

TEST(sa_error, field_on_prim)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_071_field_on_prim.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, write_readonly_ptr)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_072_write_readonly_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through"), string::npos);
}

TEST(sa_error, struct_field_unknown_type)
{
	// type Foo { BadType x; } — prim field with unknown type name
	// Covers: buildStructDef E_UnknownType for prim field (sz < 0 branch)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_073_struct_field_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, embed_unknown_struct)
{
	// type Foo { $Unknown a; } — embed field with undefined struct name
	// Covers: buildStructDef E_UnknownType for embed field
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_074_embed_unknown_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, alias_unknown_method)
{
	// lib.nonexistent() — alias "lib" exists but method not found
	// Covers: findImportFuncByAlias returning nullptr
	cleanTestEnv();
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_075_alias_unknown_method.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, embed_arr_owned_sub_struct)
{
	// [n]$Outer where Outer has an owned struct-ptr field — should error
	// Covers: E_EmbedArrOwnedSubStruct in sa_embed_arr_var_decl
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_076_embed_arr_owned_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("it owns fields"), string::npos) << sa;
}

TEST(sa_error, write_readonly_arr_elem)
{
	// [4]@Point rpts; 42 -> rpts[0].x; — write through read-only pointer array element
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, mutable:false branch (E_WriteToReadOnlyArrElem)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_077_write_readonly_arr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, field_access_on_arr_index_non_struct)
{
	// [4]int64 arr; printf("%ld\n", arr[0].x); — arr[i] is not a struct pointer
	// Covers: resolveObjectChain arr-index base case, non-struct value-type branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_078_field_access_on_arr_index_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_on_arr_index_non_struct)
{
	// [4]int64 arr; 10 -> arr[0].x; — arr[i] is not a struct pointer
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, non-struct value-type branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_079_field_assign_on_arr_index_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_access_on_tuple_base)
{
	// type Point{...}; type Line{$Point a; $Point b;}; int64 v = (1,2).a.x; --
	// a multi-expression tuple grouping normalizes to "not-impl" (see term's
	// '(' tapple_inner ')' rule), reached as the base of a field-access chain.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_116_field_access_on_tuple_base.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("this expression is not supported in this version."), string::npos);
}

TEST(sa_error, arr_field_size_not_constant)
{
	// type Buf { [int64(2) + 1]$int64 data; }; -- a cast is not folded as a constant expression
	// Covers: buildStructDef "arr" branch, E_ArrFieldSizeNotConstant
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_080_arr_field_size_not_constant.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: struct field array size must be a compile-time constant"), string::npos);
}

TEST(sa_error, embed_arr_field_owned_substruct)
{
	// type Rect { Point tl; Point br; }; type Grid { [2]$Rect cells; };
	// -- Rect has owned sub-struct fields, so [n]$Rect is not supported.
	// Covers: buildStructDef "arr" branch, struct-leaf case, E_EmbedArrOwnedSubStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_081_embed_arr_field_owned_substruct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot embed 'Rect' with '$': it owns fields"), string::npos) << sa;
}

TEST(sa_error, recursive_arr_field)
{
	// type A { [2]$A a; }; -- self-referential embedded array field
	// Covers: buildStructDef "arr" branch, struct-leaf case, E_RecursiveStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_082_recursive_arr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct 'A' recursively contains itself"), string::npos);
}

TEST(sa_error, write_readonly_arr_field_elem)
{
	// type Watch { [3]@Point observed; }; p -> w.observed[0]; 42 -> w.observed[0].x;
	// -- write-through to a non-mutable embed-ptr-arr struct field element
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, mutable:false
	// branch (E_WriteToReadOnlyArrElem), exercised via a struct field (embed-ptr-arr)
	// rather than a plain variable array (already covered by write_readonly_arr_elem).
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_083_write_readonly_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, field_access_on_prim_arr_field_elem)
{
	// type Buf { [4]$int64 data; }; printf("%ld\n", buf.data[0].sub);
	// -- data[0] is a primitive embed-arr leaf, not a struct pointer
	// Covers: sa_expr_arr_index's primitive leaf branch feeding
	// into resolveObjectChain's arr-index base case, non-struct value-type branch
	// (E_FieldAccessOnNonStruct), exercised via an embedded struct field array
	// rather than a plain variable array (already covered by
	// field_access_on_arr_index_non_struct).
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_084_field_access_on_prim_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_on_prim_arr_field_elem)
{
	// type Buf { [4]$int64 data; }; 10 -> buf.data[0].sub;
	// -- same as field_access_on_prim_arr_field_elem but through the write side
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, non-struct
	// value-type branch (E_FieldAccessOnNonStruct), via embedded struct field array.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_085_field_assign_on_prim_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, struct_arr_ptr_field_unknown_prim_type)
{
	// type T { [4]NoSuchType field; }; -- [n]T owned pointer array, prim leaf unknown
	// Covers: buildStructDef "arr" branch, non-embedded arr-ptr prim-leaf case,
	// E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_086_arr_ptr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, struct_embed_arr_field_unknown_prim_type)
{
	// type T { [4]$NoSuchType field; }; -- [n]$T embed-arr, prim leaf unknown
	// Covers: buildStructDef "arr" branch, embed-arr prim-leaf case,
	// E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_087_embed_arr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, struct_nested_embed_arr_field_unsupported)
{
	// type T { [4]$[2]int64 field; }; -- [n]$[m]T nested embed array, not supported
	// Covers: buildStructDef "arr" branch, embed-arr leaf-kind chain final else
	// (base_kind=="arr"), E_UnsupportedStructFieldType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_088_nested_embed_arr_field_unsupported.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unsupported struct field type"), string::npos);
}

TEST(sa_error, cinclude_typedef_conflict)
{
	// Two headers typedef the same name to different types.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_089_cinclude_typedef_conflict.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: Typedef 'myint' conflicts"), string::npos);
}

TEST(sa_error, const_not_literal)
{
	// const NOTCONST = x; where x is a runtime variable, not a compile-time literal
	// Covers: sa_const_decl E_ConstNotCompileTimeValue branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_090_const_not_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("must be initialized with a compile-time constant"), string::npos);
}

TEST(sa_error, addr_of_on_param)
{
	// `@n;` where n is a function parameter, not a local variable
	// Covers: sa_expression addr-of E_AddrOfNotLocalVar branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_091_addr_of_on_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("address-of requires a local variable"), string::npos);
}

TEST(sa_error, addr_of_struct_ptr_var)
{
	// `@v;` where v is a `@!Point` local -- a struct pointer is passed by name,
	// and '@' on it would build a `struct T **`.
	// Covers: sa_expr_addr_of E_AddrOfBorrowed branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_092_addr_of_struct_ptr_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("'v' is already a borrow"), string::npos);
}

TEST(sa_error, addr_of_undefined)
{
	// `@undefined_name;` -- reuses the existing E_UndefinedVariable diagnostic
	// Covers: sa_expression addr-of not-found branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_093_addr_of_undefined.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, addr_of_field_write_through_readonly)
{
	// `@Point p = s; @!int64 q = @!p.x;` -- p is a read-only struct pointer
	// (@T); taking a mutable address through one of its fields must not
	// launder read-only into mutable.
	// Covers: sa_expr_addr_of field-access branch -> resolveObjectChain(forWrite=true)
	// "id" case -> E_WriteThroughReadOnlyPtr
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_105_addr_of_field_write_through_readonly.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer '@T'"), string::npos);
}

TEST(sa_error, addr_of_field_immutable_ptr_hop)
{
	// `@!int64 p = @!n.next.val;` where `next` is `@Node` (read-only raw-ptr
	// field) -- the intermediate hop, not just the final field, must be
	// write-permission checked.
	// Covers: resolveObjectChain(forWrite=true) field-hop branch -> E_WriteToImmutablePtrField
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_106_addr_of_field_immutable_ptr_hop.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer field '@T'"), string::npos);
}

TEST(sa_error, addr_of_ptr_field)
{
	// `@s.in;` where `in` is a `@Inner` pointer field -- it is passed by name,
	// like a pointer variable.
	// Covers: sa_expr_addr_of field-access branch -> raw-ptr leaf -> E_AddrOfNotPrimitive
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_107_addr_of_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of"), string::npos);
}

TEST(sa_error, addr_of_unknown_field)
{
	// `@s.z;` where Point has no field `z`.
	// Covers: sa_expr_addr_of field-access branch -> findFieldOrExit -> E_UnknownField
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_108_addr_of_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, addr_of_call_not_addressable)
{
	// `@f();` -- a call returning a non-struct, outside a call argument.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_110_addr_of_call.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this expression"), string::npos);
}

TEST(sa_error, addr_of_field_readonly_arr_elem)
{
	// `@!int64 p = @!rpts[0].x;` where `rpts` is `[4]@Point` (read-only
	// pointer-slot array) -- the arr-index hop itself is the read-only
	// element, not just a field along the way.
	// Covers: resolveObjectChain(forWrite=true) "arr-index" branch -> E_WriteToReadOnlyArrElem
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_111_addr_of_field_readonly_arr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer array element"), string::npos);
}

TEST(sa_error, write_through_readonly_ptr)
{
	// `@int64 p = @x; 99 -> p[0];` -- deref write through a read-only `@T`
	// Covers: sa_arr_assign_stmt isWritableThrough branch (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_094_write_through_readonly_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, write_readonly_ptr_var_field)
{
	// `@Point view = original; 20 -> view.x;` -- field write through a
	// read-only `@T`-typed plain local variable (not a struct field)
	// Covers: resolveObjectChain(forWrite=true) "id" isWritableThrough branch
	// (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_095_write_readonly_ptr_var_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade)
{
	// `@int64 p = @x; @!int64 q = p;` -- binding a read-only pointer to a
	// mutable-typed destination
	// Covers: sa_var_decl ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_096_ptr_mutability_upgrade.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_assign)
{
	// `p -> q;` where p is `@int64` and q is `@!int64` -- plain assign-stmt
	// upgrade
	// Covers: sa_assign_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_097_ptr_mutability_upgrade_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_return)
{
	// `func f() -> IntPtr { ...; return p; }` where IntPtr is an alias for
	// `@!int64` and p is `@int64` -- return-stmt upgrade
	// Covers: sa_return_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_098_ptr_mutability_upgrade_return.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_arr_assign)
{
	// `view -> wpts[0];` where view is `@Point` and wpts is `[4]@!Point` --
	// storing a read-only pointer value into a mutable pointer-slot array
	// element
	// Covers: sa_arr_assign_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_099_ptr_mutability_upgrade_arr_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_field_assign)
{
	// `ro -> n1.next;` where ro is `@Node` and `next` is a `@!Node` field --
	// storing a read-only pointer value into a mutable pointer field
	// Covers: sa_field_assign ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_100_ptr_mutability_upgrade_field_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_call_arg)
{
	// `take(ro);` where `take` takes `@!int64` and ro is `@int64` -- passing
	// a read-only pointer where a Palan function expects a mutable one
	// Covers: sa_expr_call ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_101_ptr_mutability_upgrade_call_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, assign_whole_struct_elem)
{
	// `other ->> pt[0];` -- `pt[0]` is the struct's storage, not a pointer
	// slot, so it cannot take over another struct's ownership.
	// Covers: sa_arr_assign_stmt addr-only guard (E_AssignToWholeStructElem)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_102_assign_whole_struct_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot assign to a struct element as a whole"), string::npos);
}

TEST(sa_error, write_readonly_struct_ptr_field)
{
	// `42 -> ro[0].x;` where ro is `@Point` -- writing a field through a
	// read-only struct pointer via `p[i].field` is rejected, same as for a
	// struct stored in a read-only borrowed array.
	// Covers: resolveObjectChain(forWrite=true) arr-index branch (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_103_write_readonly_struct_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, deref_unknown_struct_ptr)
{
	// `func f(@!Foo p) { p[0].bar; }` where `Foo` is never declared. gen-ast has
	// no symbol table, so `Foo` parses as a prim base-type, not a struct one --
	// sa_expr_arr_index's generic (non-struct) branch must reject
	// this at the SA boundary instead of letting elemSizeBytes' -1
	// "unknown type" sentinel leak into elem-size and crash palan-codegen
	// downstream (layer violation).
	// A local `@!Foo p;` var decl is
	// now rejected at declaration time (sa_var_decl), so only a parameter's
	// pointee (unchecked at signature normalization) still reaches this guard.
	// Covers: sa_expr_arr_index generic branch, sz<0 guard (E_UnknownType)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_104_deref_unknown_struct_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param)
{
	// `clock_getcpuclockid(int32(0), p);` where `p` is `@int32` (read-only)
	// and the C parameter is `clockid_t *` (non-const) -- C function
	// arguments are checked by
	// ptrPermissionOk() too, previously exempted (see the removed comment at
	// sa_expr_call's arg loop).
	// Covers: sa_expr_call checkArgPtrPermission, C-func branch (E_ReadOnlyPtrToNonConstCParam)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_112_readonly_ptr_to_nonconst_c_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param_alias)
{
	// Same as readonly_ptr_to_nonconst_c_param but through an aliased
	// cinclude (`cinclude <time.h> as T; T.clock_getcpuclockid(...)`) --
	// sa_expr_member_call had no pointer-permission check at all before this
	// ticket, for either C or Palan callees.
	// Covers: sa_expr_member_call checkArgPtrPermission, C-func branch (E_ReadOnlyPtrToNonConstCParam)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_113_readonly_ptr_to_nonconst_c_param_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param_unnamed)
{
	// `tmpnam(s)` -- glibc's stdio.h declares tmpnam's parameter with an
	// abstract declarator (no name), so c2ast's parameter entry has no
	// "name" key. checkArgPtrPermission falls back to a 1-based position
	// ("#1") for the diagnostic in that case.
	// Covers: checkArgPtrPermission, param.value("name","").empty() branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_114_readonly_ptr_to_nonconst_c_param_unnamed.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter '#1'"), string::npos);
}

TEST(sa_error, addr_of_ptr_elem_not_addressable)
{
	// `@!wpts[0]` where `wpts` is `[4]@!Point` (pointer-slot array) -- the
	// element itself is already a pointer, so taking its address would be
	// a double indirection; out of scope.
	// Covers: sa_expr_addr_of arr-index branch, elem value-type type-kind != "prim" -> E_AddrOfNotPrimitiveElem
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_118_addr_of_ptr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this array element"), string::npos);
}

TEST(sa_error, addr_of_readonly_ptr_elem)
{
	// `@!p[1]` where `p` is `@int64` (read-only pointer) -- `@!arr[i]` is
	// "get write permission to arr[i]", so it must be rejected the same way
	// a plain deref-write through a read-only pointer is.
	// Covers: sa_expr_addr_of arr-index branch, isMutable && !isWritableThrough -> E_WriteThroughReadOnlyPtr
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_119_addr_of_readonly_ptr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, ptr_decl_unknown_type)
{
	// `@!@!NoSuchStruct p;` (pntr-of-pntr, to exercise the walk-through loop
	// too) -- sa_var_decl used to have no "pntr" branch in its dispatch guard,
	// so the pointee name was never validated at declaration time; without an
	// initializer, this used to compile silently and leave `p` referencing a
	// nonexistent type.
	// Covers: sa_var_decl pntr branch (incl. pntr-of-pntr walk), unknown
	// pointee prim name -> E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_120_ptr_decl_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, incomplete_struct_var_decl)
{
	// `struct Tag { int x; int cells[2][3]; };` (cinclude'd, "cells" unsupported)
	// then `Tag t;` -- an owned declaration needs Tag's totalSize to calloc it.
	// registerCStruct downgrades Tag to an incomplete struct
	// (opaque handle) instead of leaving the tag unregistered, so this is a
	// diagnosed E_IncompleteStructType, not "unknown type".
	// Covers: sa_struct_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_123_incomplete_struct_var_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_forward_declared)
{
	// `struct Tag;` (forward-declared only, never defined in this header) then
	// `Tag t;` -- registerCStruct registers a
	// forward-declared-only tag as an incomplete struct too (previously it was
	// never registered at all, and a chain like this used to hit a raw
	// BOOST_ASSERT abort in requireCompleteStruct rather than a diagnostic).
	// The message distinguishes this reason ("forward-declared") from an
	// unsupported field shape.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_132_forward_declared_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' is only forward-declared in this header"), string::npos);
}

TEST(sa_error, c_union_unknown_field)
{
	// The unknown field sits mid-chain (u.c.a), so it is diagnosed while
	// resolving the object chain rather than at the final field store.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_187_c_union_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("union 'U' has no field 'c'"), string::npos);
}

TEST(sa_error, c_union_forward_declared)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_188_c_union_forward_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("union 'F' is only forward-declared in this header"), string::npos);
}

TEST(sa_error, incomplete_struct_owned_arr)
{
	// Same incomplete Tag as above; `[3]Tag a;` (owned pointer array) needs
	// Tag's layout to record its alloc-shape (recordAllocShape reads totalSize).
	// Covers: sa_owned_struct_arr_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_124_incomplete_struct_owned_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_embed_arr)
{
	// Same incomplete Tag; `[3]$Tag a;` (contiguous embedded array) needs Tag's
	// totalSize as the element stride for the single malloc(n * totalSize).
	// Covers: sa_embed_arr_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_125_incomplete_struct_embed_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_embed)
{
	// Same incomplete Tag, embedded in a native struct: `type Wrap { $Tag a; };`
	// declared inside a function body so it is only processed by sa_statements
	// (pass 3), after cinclude registers Tag (pass 2) -- at top level a struct-def
	// is also pre-scanned in pass 0, before cinclude, which would hit the
	// separate "name not yet registered" E_UnknownType path instead of
	// this one.
	// Covers: buildStructDef "embed" branch, sub.isComplete check
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_126_incomplete_struct_native_embed.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_field_access)
{
	// `func f(@!Tag p) { int64 v = p.x; }` -- `@!Tag p` itself declares fine (no
	// layout needed for a pointer parameter), but reading a field requires
	// looking up Tag's field list, which an incomplete struct doesn't have.
	// Without this check this would previously degrade to a misleading
	// "struct 'Tag' has no field 'x'" (E_UnknownField) instead of naming the
	// real problem.
	// Covers: findFieldOrExit -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_127_incomplete_struct_field_access.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
	ASSERT_EQ(sa.find("has no field"), string::npos);
}

TEST(sa_error, incomplete_struct_index)
{
	// `func f(@!Tag p) { int64 v = p[0].x; }` -- `p[0]` computes an address as
	// base + i*sizeof(Tag), which needs Tag's totalSize as the stride; an
	// incomplete Tag has none.
	// Covers: sa_expr_arr_index struct branch -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_128_incomplete_struct_index.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_owned_field)
{
	// Same incomplete Tag; `type Wrap { Tag a; };` (a bare-name field, i.e. an
	// owned struct-ptr field per native struct syntax) is declared inside a
	// function body, same reasoning as incomplete_struct_native_embed above,
	// so it processes after cinclude registers Tag as incomplete. An owned
	// struct-ptr field's declaring struct records an alloc-shape for it, which
	// needs the pointee's totalSize.
	// Covers: buildStructDef "prim name is a registered struct" branch (struct-ptr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_129_incomplete_struct_native_owned_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_owned_arr)
{
	// Same incomplete Tag; `type Wrap { [3]Tag a; };` (owned pointer array
	// field, cascades to __pln_alloc_arr_T/__pln_free_arr_T) needs the leaf's
	// totalSize when that cascade's alloc-shape is recorded.
	// Covers: buildStructDef "[n]T owned pointer array, struct leaf" branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_130_incomplete_struct_native_owned_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_embed_arr)
{
	// Same incomplete Tag; `type Wrap { [3]$Tag a; };` (contiguous embedded
	// array field within a *native* struct, distinct from the top-level
	// `[3]$Tag a;` var-decl covered by incomplete_struct_embed_arr above --
	// that goes through sa_embed_arr_var_decl, this goes through
	// buildStructDef's own embedded-array leaf case) needs the leaf's
	// totalSize/maxAlign/ownsFields to lay out the array stride.
	// Covers: buildStructDef "[n]$T embedded array, struct leaf" branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_131_incomplete_struct_native_embed_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, sized_arr_param)
{
	// `func f([3]int64 a) -> int64 { ... }` -- a sized-array parameter. []T /
	// []$[m]T (unsized) normalize to pntr via unsizedArrToPntr, but a sized
	// [n]T parameter keeps type-kind "arr", which PlnTypeRegistry::fromJson
	// cannot build. Previously this reached sa_expr_call's parameter-side
	// fromJson call, which was silently swallowed by a
	// `catch (const std::runtime_error&) {}` -- the argument itself would
	// still abort unguarded downstream. Registration now
	// validates the normalized signature and diagnoses it up front instead.
	// Covers: PlnSemanticAnalyzer::validateNativeSig (top-level registration)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_133_sized_arr_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("function 'f' has a parameter or return type this version cannot represent: 'array'"),
	          string::npos);
}

TEST(sa_error, c_unsupported_user_param)
{
	// `void take_handle(mystery_t h);` -- `mystery_t` is an identifier c2ast
	// never saw a typedef for, so it stays type-kind "user" through
	// normalizeCType. normalizeCFuncSig tags the registered
	// entry with "_unsupported-sig" and requireSupportedCFuncSig diagnoses it
	// at the call, instead of the unguarded fromJson at the argument site
	// aborting (or, before this ticket, the parameter-side try/catch quietly
	// degrading typeCompat/wrapConvert for the argument).
	// Covers: sa_expr_call -> requireSupportedCFuncSig (unaliased path)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_134_c_unsupported_user_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'take_handle'"), string::npos);
	ASSERT_NE(sa.find("'mystery_t'"), string::npos);
}

TEST(sa_error, c_unsupported_union_param)
{
	// `int use_val(union { int i; } v);` -- a nameless union in a signature
	// has no tag to register a layout under, so c2ast emits a bare
	// {"type-kind":"union"}. glibc always names or typedefs its unions, so
	// this is a hand-written header.
	// Covers: sa_expr_call -> requireSupportedCFuncSig
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_136_c_unsupported_union_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'use_val'"), string::npos);
	ASSERT_NE(sa.find("'union'"), string::npos);
}

TEST(sa_error, c_unsupported_enum_param)
{
	// `void pick(enum Color c);` where c2ast can't compute Color's values
	// (`= sizeof(struct Opaque)`, an incomplete type), so the enum is never registered.
	// Covers: sa_expr_call -> requireSupportedCFuncSig
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_137_c_unsupported_enum_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'pick'"), string::npos);
	ASSERT_NE(sa.find("'enum'"), string::npos);
}

TEST(sa_error, c_unsupported_union_ret)
{
	// `union { int i; } make_val(void);` -- the ret-type-only path: the callee has
	// no parameters, so the only way to reach a diagnosis is if
	// requireSupportedCFuncSig fires before sa_expr["value-type"] is set from
	// ret-type (sa_expr_call:457-459) -- proves the gate precedes that copy
	// rather than only catching it downstream once value-type is consumed.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, before ret-type copy
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_138_c_unsupported_union_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'make_val'"), string::npos);
	ASSERT_NE(sa.find("'union'"), string::npos);
}

TEST(sa_error, c_unsupported_func_param)
{
	// `void set_cb(int (*cb)(long double));` -- a function pointer whose own
	// parameter is unrepresentable makes the whole signature unsupported.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_139_c_unsupported_func_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'set_cb'"), string::npos);
	ASSERT_NE(sa.find("'function pointer'"), string::npos);
}

TEST(sa_error, c_unsupported_anon_strct_param)
{
	// `void f(struct { int a; } *p);` -- an inline anonymous struct behind a
	// pointer. c2ast's "strct" branch still omits type-name for a tagless
	// struct even after removing the definedStructs_ guard (that guard
	// only covered forward-declared *tagged* references); normalizeCType
	// folds it to a nameless {"type-kind":"struct"}.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, nameless struct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_140_c_unsupported_anon_strct_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'f'"), string::npos);
	ASSERT_NE(sa.find("'anonymous struct'"), string::npos);
}

TEST(sa_error, c_unsupported_long_double)
{
	// `acosl(1.0)` from <math.h> -- `long double` maps to c2ast's "flt128"
	// prim type-name (CParser.cpp), which is not in PrimTypeNames -- the
	// dominant real-world case: math.h alone has 309 such nodes across 150
	// functions in the empirical audit for this ticket, far more than every
	// other unsupported kind combined. Also proves unrepresentableTypeName's
	// "prim" branch (a resolvable type-kind, unresolvable type-name), not
	// just its type-kind branches, and that the diagnostic fires instead of
	// the pre-2906 abort ("unknown prim type-name: flt128", rc=134).
	// Covers: sa_expr_call -> requireSupportedCFuncSig, unresolved prim name
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_141_c_unsupported_long_double.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'acosl'"), string::npos);
	ASSERT_NE(sa.find("'flt128'"), string::npos);
	ASSERT_EQ(sa.find("unrepresentable type"), string::npos);  // diagnosed, not the raw fromJson throw text
}

TEST(sa_error, c_unsupported_aliased_call)
{
	// Same shape as c_unsupported_user_param, but through an aliased
	// `cinclude ... as M;` / `M.take_handle(...)` call -- exercises
	// sa_expr_member_call's separate gate rather than sa_expr_call's.
	// Covers: sa_expr_member_call -> requireSupportedCFuncSig (aliased path)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_142_c_unsupported_aliased_call.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'take_handle'"), string::npos);
	ASSERT_NE(sa.find("'mystery_t'"), string::npos);
}

TEST(sa_error, c_global_not_assignable)
{
	// `fopen(...) -> stderr;` -- a C global is read-only.
	// Covers: sa_assign_stmt findCGlobal branch, E_CGlobalNotAssignable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_143_c_global_not_assignable.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot assign to 'stderr'"), string::npos);
	ASSERT_NE(sa.find("read-only"), string::npos);
}

TEST(sa_error, c_global_addr_of)
{
	// `@stderr;` -- a C global is a value, not a storage location Palan owns.
	// Covers: sa_expr_addr_of "id" branch findCGlobal check, E_CGlobalNotAddressable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_144_c_global_addr_of.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("'stderr'"), string::npos);
	ASSERT_NE(sa.find("not a storage location"), string::npos);
}

TEST(sa_error, c_global_field_access)
{
	// `stderr._flags` -- field access requires resolveObjectChain to treat
	// the base as an ordinary struct-pointer local, which a C global is not.
	// Covers: resolveObjectChain "id" branch findCGlobal check, E_CGlobalNotAddressable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_145_c_global_field_access.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("'stderr'"), string::npos);
	ASSERT_NE(sa.find("not a storage location"), string::npos);
}

TEST(sa_error, c_global_out_of_block_scope)
{
	// `stderr` referenced after the block whose cinclude registered it --
	// cGlobalScopes is popped by leaveScope like cFuncScopes, so this is an
	// ordinary undefined-variable error, not a C-global-specific one.
	// Covers: enterScope/leaveScope cGlobalScopes pop, sa_expression "id" fallthrough
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_146_c_global_out_of_block_scope.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
	ASSERT_NE(sa.find("'stderr'"), string::npos);
}

TEST(sa_error, c_global_unsupported_type)
{
	// `extern long double ld;` -- cinclude succeeds (deferred, same policy as
	// _unsupported-sig), but referencing `ld` diagnoses its unrepresentable
	// "flt128" type instead of the pre-2908 abort a raw "user"/unresolved
	// prim type-name would otherwise cause downstream.
	// Covers: sa_expression "id" branch -> requireSupportedCGlobal, E_UnsupportedCGlobalType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_147_c_global_unsupported_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot reference C global variable 'ld'"), string::npos);
	ASSERT_NE(sa.find("'flt128'"), string::npos);
}

TEST(sa_error, arg_narrowing)
{
	// A call argument used to get no type
	// check beyond ImplicitWiden -- an int64 argument to an int32 parameter
	// silently passed through and produced a bad `movq` operand-width mismatch
	// in the emitted assembly. Now diagnosed at the call site, same message as
	// a narrowing var-decl initializer.
	// Covers: convertCallArg, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_148_arg_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
}

TEST(sa_error, arith_op_not_numeric)
{
	// `p + 1` on a pointer operand used to
	// silently fall through to `promoted = leftType`, accepting pointer
	// arithmetic that Palan has no syntax or semantics for. usualArithConv
	// returns nullptr for a non-Prim operand, which sa_expr_arith now
	// diagnoses instead of silently accepting.
	// Covers: sa_expr_arith non-Prim operand, E_ArithOpNotNumeric
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_149_arith_op_not_numeric.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Arithmetic operator operand must be a numeric type"), string::npos);
}

TEST(sa_error, arith_op_not_numeric_rhs)
{
	// Same check as arith_op_not_numeric above, but with the non-Prim operand
	// on the right (`1 + p`) instead of the left (`p + 1`) -- usualArithConv's
	// non-Prim guard checks both operands independently, so both orders need
	// a dedicated test.
	// Covers: sa_expr_arith non-Prim right operand, E_ArithOpNotNumeric
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_151_arith_op_not_numeric_rhs.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Arithmetic operator operand must be a numeric type"), string::npos);
}

TEST(sa_error, assign_narrowing)
{
	// An assignment (`big -> x`) used to
	// silently insert a narrowing convert -- unlike a var-decl initializer,
	// which has always rejected this. Assignment/array-assign/return/
	// field-assign now share the initializer's strict rule.
	// Covers: convertForBinding via sa_assign_stmt, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_150_assign_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
}


TEST(sa_error, call_arg_ptr_mismatch_native)
{
	// convertCallArg used to pass any non-Prim
	// (pointer/struct) argument through unchecked -- `@!int32` bound to a
	// `@!int64` parameter of a Palan function compiled with no diagnostic
	// and the wrong access width at runtime.
	// Covers: convertCallArg via sa_expr_call (Palan callee), E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_153_call_arg_ptr_mismatch_native.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, call_arg_ptr_mismatch_c)
{
	// Same gap as call_arg_ptr_mismatch_native, but through the C-callee
	// path (a cinclude'd `long *` parameter fed a `@!int32` argument).
	// Covers: convertCallArg via sa_expr_call (C callee), E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_154_call_arg_ptr_mismatch_c.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, convert_for_binding_ptr_mismatch)
{
	// Same Incompatible gap as call_arg_ptr_mismatch_native, but through
	// convertForBinding's binding-site path (var-decl initializer here;
	// shared by assignment/array-assign/return/field-assign too).
	// Covers: convertForBinding, E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_156_convert_for_binding_ptr_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, byval_struct_ret_discarded)
{
	// `div(7, 2);` as a bare statement -- a C function's
	// struct-by-value return reaching sa_expression_stmt with no
	// destination variable to write it into.
	// Covers: sa_expression_stmt struct value-type rejection, E_ByvalStructRetDiscarded
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_157_byval_struct_ret_discarded.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("the return value of this call (a 'div_t' returned by value) is discarded"), string::npos);
}

TEST(sa_error, callback_arg_not_func)
{
	// A variable of another type in a C function's function pointer parameter.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_159_callback_arg_not_func.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":5:1: error: cannot convert 'int64' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_arity_mismatch)
{
	// A function with fewer parameters than qsort's comparator.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_160_callback_sig_arity_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32) -> int32' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_ptr_permission)
{
	// qsort passes `const void*`: a function that may write through its
	// parameters can't take them.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_161_callback_ptr_permission.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@!int32, @!int32) -> int32' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_width_mismatch)
{
	// The callee is called with no conversion step, so int32 does not widen
	// to an int64 parameter as it would in a direct call.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_162_callback_sig_width_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(int64, @int8)' to '@func(int32, @!void)'."), string::npos) << sa;
}

TEST(sa_error, c_unsupported_func_ret)
{
	// `void set_cb(long double (*cb)(int));` -- the function pointer's return
	// type, not a parameter, is unrepresentable.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_163_c_unsupported_func_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'set_cb'"), string::npos);
	ASSERT_NE(sa.find("'function pointer'"), string::npos);
}

TEST(sa_error, callback_ret_void_mismatch)
{
	// A function returning a value where a void function is expected.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_164_callback_ret_void_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":3:1: error: cannot convert '@func() -> int32' to '@func()'."), string::npos) << sa;
}

TEST(sa_error, callback_ret_type_mismatch)
{
	// Matching parameters, different non-void return type.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_165_callback_ret_type_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32, @int32) -> int64' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_multi_ret_mismatch)
{
	// Multiple named returns have no single return register to bind to.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_169_callback_multi_ret_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:34: error: function 'cmp' cannot be used as a function pointer: it returns multiple values."), string::npos) << sa;
}

TEST(sa_error, callback_ret_missing_mismatch)
{
	// A void function where qsort's comparator returns int.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_170_callback_ret_missing_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32, @int32)' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, deref_void_pointer)
{
	// `p[0]` where p is @!void -- void has no size, so
	// this must give a dedicated diagnostic rather than the misleading
	// "unknown type 'void'." that the shared sz<0 guard in
	// sa_expr_arr_index would otherwise produce.
	// Covers: sa_expr_arr_index void-pointee guard, E_DerefVoidPointer
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_166_deref_void_pointer.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot index or dereference a void pointer"), string::npos);
}

TEST(sa_error, deref_void_pointer_write)
{
	// Same guard as deref_void_pointer, reached via sa_arr_assign_stmt's
	// write path (`v -> p[0]`) instead of a read.
	// Covers: sa_expr_arr_index void-pointee guard, E_DerefVoidPointer
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_167_deref_void_pointer_write.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot index or dereference a void pointer"), string::npos);
}

TEST(sa_error, field_access_through_prim_ptr_field)
{
	// Prerequisite fix: `s.p.q` where `p` is a
	// raw-ptr field with a primitive pointee (`@!int64 p;`) has nothing to
	// chain into -- resolveObjectChain must reject it as a non-struct hop,
	// not build a bogus pntr(struct("int64")) and fail later with an
	// unrelated "unknown type 'int64'." from requireCompleteStruct.
	// Covers: resolveObjectChain raw-ptr elemKind=="prim" guard,
	// E_FieldAccessOnNonStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_168_field_access_through_prim_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, invalid_link_lib_name)
{
	// A library name reaches build-mgr's `ld` command
	// string, so a name carrying shell metacharacters is rejected
	// at the one point a name enters "libs", not downstream.
	// Covers: isValidLinkLibName reject path, E_InvalidLinkLibName
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_171_invalid_link_lib.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(": error:"), string::npos);
	ASSERT_NE(sa.find("invalid library name"), string::npos);
}

TEST(sa_error, link_obj_not_found)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_611_link_obj_not_found.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:1: error: object file '"), string::npos);
	ASSERT_NE(sa.find("/test/testdata/sa/error_611_missing.o' in a cinclude link clause is not found."), string::npos);
}

TEST(sa_error, empty_link_lib_name)
{
	// `link "";` lexes fine (the link-clause strings are scanned in
	// INITIAL, where STRING allows zero characters) and would otherwise
	// emit a bare "-l" that swallows the next ld argument.
	// Covers: isValidLinkLibName empty() guard
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_172_empty_link_lib.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("invalid library name"), string::npos);
}

TEST(sa_error, syscall_number_not_constant)
{
	// A syscall number must be a literal integer.
	// Covers: validateSyscallDecl expr-type guard, E_SyscallNumberNotConstant
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_173_syscall_number_not_constant.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("number must be a literal integer"), string::npos);
}

TEST(sa_error, syscall_number_out_of_range)
{
	// Covers: validateSyscallDecl uint32 range guard, E_SyscallNumberOutOfRange
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_175_syscall_number_out_of_range.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("does not fit in a 32-bit unsigned integer"), string::npos);
}

TEST(sa_error, syscall_too_many_params)
{
	// Linux syscall ABI supports at most 6 register arguments.
	// Covers: validateSyscallDecl parameter-count guard, E_SyscallTooManyParams
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_176_syscall_too_many_params.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Linux syscall ABI supports at most 6"), string::npos);
}

TEST(sa_error, syscall_float_param)
{
	// flo32/flo64 do not fit in a GP register, which is all the Linux
	// syscall ABI passes arguments through.
	// Covers: validateSyscallDecl float-parameter guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_179_syscall_float_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'flo64'"), string::npos);
}

TEST(sa_error, syscall_float_return)
{
	// Covers: validateSyscallDecl float-return guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_180_syscall_float_return.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'flo64'"), string::npos);
}

TEST(sa_error, syscall_narrow_param)
{
	// int8/int16/uint8/uint16 have no dedicated slot in the Linux syscall
	// ABI either -- every GP register is read/written in full, so a
	// narrower declared type would leave stale high bits behind a
	// partial-register move instead of the extension the ABI assumes.
	// Covers: validateSyscallDecl narrow-int guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_184_syscall_narrow_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'int8'"), string::npos);
}

TEST(sa_error, syscall_duplicate)
{
	// Proves a syscall declaration actually lands in plnFuncScopes (shared
	// with "palan" func-defs, not a separate table) -- a second syscall of
	// the same name hits the same E_DuplicateFuncDef guard.
	// Covers: preregisterFunc -> registerPlnFunc shared table, E_DuplicateFuncDef
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_181_syscall_duplicate.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: Function 'foo' is already defined."), string::npos);
}

TEST(sa_error, syscall_call_narrowing)
{
	// A syscall call's argument checking reuses the same convertCallArg path
	// as a Palan/C call -- an implicit narrowing conversion is rejected
	// identically.
	// Covers: sa_expr_call syscall resolution -> saCallArgs -> convertCallArg, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_182_syscall_call_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32' is not allowed"), string::npos);
}

TEST(sa_error, syscall_call_ptr_permission)
{
	// A syscall call's pointer argument checking reuses checkArgPtrPermission
	// -- passing a read-only '@T' where the syscall declares a mutable
	// '@!T' parameter is rejected the same way a Palan call would be.
	// Covers: sa_expr_call syscall resolution -> checkArgPtrPermission (isCFunc=false path), E_PtrMutabilityUpgrade
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_183_syscall_call_ptr_permission.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, macro_backref_undefined)
{
	// A reference to a cinclude'd macro name that textually precedes its
	// cinclude statement is not folded by gen-ast (visibility is loc-order,
	// same as the macro table itself) -- it reaches SA as a plain "id" and
	// is diagnosed like any other undefined variable.
	// Covers: foldMacroConstants loc-order gate, E_UndefinedVariable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_185_macro_backref_undefined.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, macro_backref_arr_field)
{
	// Same loc-order gate as macro_backref_undefined, but for a struct
	// field array size -- the un-folded "id" names no const.
	// Covers: foldMacroConstants loc-order gate
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_186_macro_backref_arr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct field array size must be a compile-time constant integer; 'ARR_N' is not a const."), string::npos) << sa;
}

TEST(sa_error, untyped_var_decl)
{
	// `v = 7;` is a declaration with the type omitted (reserved for future type
	// inference), not an assignment; gen-ast keeps parsing it, so SA must reject it.
	// Covers: sa_statements "not-impl" branch with untyped-var, E_VarTypeInferenceNotImpl
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_190_untyped_var_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: variable 'v' is declared without a type"), string::npos);
}

TEST(sa_error, stmt_not_implemented)
{
	// Covers: sa_statements "not-impl" branch without untyped-var, E_StmtNotImplemented
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_191_stmt_not_implemented.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: this statement is not supported"), string::npos);

	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_522_chain_tapple_then_assign.pa -o " + ast_out), "");
	sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: this statement is not supported"), string::npos);
}

TEST(sa_error, arr_lit_func_arg)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_208_arr_lit_func_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:5: error: parameter 'a' of type '[?]int32' cannot take an array literal: only a struct or fixed-size array parameter can."), string::npos);
}

TEST(sa_error, arr_lit_init)
{
	const pair<string, string> cases[] = {
		{"error_209_arr_lit_count_mismatch.pa", ":1:1: error: array 'a' has size 3 but its array literal has 2 elements."},
		{"error_210_arr_lit_size_not_const.pa", ":2:1: error: array 'a' given an array literal must have a compile-time constant size."},
		{"error_211_arr_lit_dim_mismatch.pa", ":1:15: error: the array literal's dimensions do not match array 'a'."},
		{"error_212_arr_lit_elem_type.pa", ":2:11: error: a value of struct 'P' must be written as '[...]' or '{name: value, ...}'."},
		{"error_213_arr_lit_elem_narrowing.pa", ":2:17: error: Implicit conversion from 'int16' to 'int8' is not allowed"},
		{"error_214_arr_lit_elem_range.pa", ":1:17: error: Integer literal '300' is out of range for type 'int8'."},
		// Elements are analyzed before the array is declared.
		{"error_215_arr_lit_self_ref.pa", ":1:18: error: Undefined variable 'a'."},
		{"error_216_arr_lit_ptr_elem.pa", ":1:1: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_217_arr_lit_unknown_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_218_arr_lit_void_elem.pa", ":2:15: error: Void function call cannot be used as a value."},
		{"error_219_arr_lit_row_size.pa", ":1:24: error: array 'm' has rows of size 3 but a row of its array literal has 2 elements."},
		{"error_220_arr_lit_row_size_declared.pa", ":1:19: error: array 'm' has rows of size 3 but a row of its array literal has 2 elements."},
		{"error_221_arr_lit_row_count.pa", ":1:1: error: array 'm' has size 3 but its array literal has 2 elements."},
		{"error_222_arr_lit_2d_given_1d.pa", ":1:18: error: the array literal's dimensions do not match array 'm'."},
		{"error_223_arr_lit_3d.pa", ":1:1: error: array 'm' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_224_arr_lit_row_size_not_const.pa", ":2:1: error: array 'm' given an array literal must have a compile-time constant size."},
		{"error_375_struct_lit_void_field.pa", ":3:16: error: Void function call cannot be used as a value."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, int_literal_out_of_range)
{
	// Covers: checkIntLiteralRange for lit-int/lit-uint, via a var-decl
	// initializer and an arith operand typed from expectedType
	const pair<string, string> cases[] = {
		{"error_200_int_lit_range_int8.pa", ":1:10: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_201_int_lit_range_uint32.pa", "'5000000000' is out of range for type 'uint32'"},
		{"error_202_int_lit_range_int64.pa", "'99999999999999999999' is out of range for type 'int64'"},
		{"error_203_int_lit_range_uint64.pa", "'18446744073709551616' is out of range for type 'uint64'"},
		{"error_204_int_lit_range_arith.pa", ":2:14: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_205_int_lit_range_neg_unsigned.pa", "'-1' is out of range for type 'uint32'"},
		// An unsuffixed cinclude macro is an untyped literal, typed from its context.
		{"error_225_macro_untyped_range.pa", ":5:3: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_247_int_lit_range_const.pa", ":2:10: error: Integer literal '300' is out of range for type 'int8'."},
		// A constant expression is folded first, so the sum is what must fit.
		{"error_593_int_lit_range_folded.pa", ":1:10: error: Integer literal '200' is out of range for type 'int8'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, int_literal_typed_from_other_operand)
{
	// `300 + i` in an int8 initializer: 300 takes int64 from `i`, so the
	// error is the narrowing of the sum, not a range error on the literal.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_206_int_lit_arith_other_operand.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: Implicit conversion from 'int64' to 'int8'"), string::npos);
	ASSERT_EQ(sa.find("out of range"), string::npos);
}

TEST(sa_error, bool_conversion)
{
	const pair<string, string> cases[] = {
		{"error_226_bool_lit_range.pa", ":1:10: error: Integer literal '2' is out of range for type 'bool'."},
		{"error_227_bool_from_int.pa", ":2:1: error: Implicit conversion from 'int32' to 'bool'"},
		{"error_228_bool_arg_from_int.pa", ":3:1: error: Implicit conversion from 'int8' to 'bool'"},
		{"error_229_bool_arith_result.pa", ":2:1: error: Implicit conversion from 'int32' to 'bool'"},
		{"error_230_syscall_bool_param.pa", "cannot pass through the Linux syscall ABI: 'bool'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, arr_borrow)
{
	const pair<string, string> cases[] = {
		{"error_231_arr_borrow_write_ro.pa", ":1:26: error: cannot write through read-only pointer"},
		{"error_232_arr_borrow_write_ro_embed.pa", ":1:27: error: cannot write through read-only pointer"},
		{"error_233_arr_borrow_ro_to_mut.pa", ":2:1: error: cannot bind a read-only pointer"},
		{"error_234_arr_borrow_no_addr_of.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') must be written as"},
		{"error_235_arr_borrow_size_mismatch.pa", "array shape '[3][4]int32' does not match the borrowed array type '@[3][5]int32'."},
		{"error_236_arr_borrow_runtime_size.pa", "array shape '[?]int32' does not match the borrowed array type '@[2]int32'."},
		{"error_237_arr_borrow_embed_mismatch.pa", "array shape '[2]$[3]int32' does not match the borrowed array type '@[2][3]int32'."},
		{"error_238_arr_borrow_size_not_const.pa", ":2:1: error: every size in a borrowed array type"},
		{"error_239_addr_of_struct_arr.pa", "array shape '[2]P' does not match the borrowed array type '@[2]int32'."},
		{"error_240_arr_borrow_struct_elem.pa", "neither a struct nor a pointer inside a '$[m]' row"},
		{"error_241_arr_borrow_mut_of_ro.pa", "cannot bind a read-only pointer"},
		{"error_242_arr_borrow_local_no_addr_of.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') must be written as"},
		{"error_243_arr_borrow_assign_mismatch.pa", ":4:1: error: array shape '[3]int32' does not match"},
		{"error_244_arr_borrow_unsized.pa", "every size in a borrowed array type"},
		{"error_245_arr_borrow_return.pa", "cannot represent: 'array'"},
		{"error_246_arr_borrow_embed_prim.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_271_struct_arr_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer"},
		{"error_272_struct_arr_borrow_mut_elem_of_ro.pa", ":3:10: error: cannot write through read-only pointer"},
		{"error_273_struct_arr_borrow_elem_no_addr_of.pa", ":3:2: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_274_struct_arr_borrow_ptr_slots.pa", "array shape '[2]@P' does not match the borrowed array type '@[2]P'."},
		{"error_275_struct_arr_borrow_size_mismatch.pa", "array shape '[2][4]P' does not match the borrowed array type '@[2][3]P'."},
		{"error_276_embed_struct_arr_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer"},
		{"error_277_embed_struct_arr_borrow_mut_elem_of_ro.pa", ":3:10: error: cannot write through read-only pointer"},
		{"error_278_embed_struct_arr_borrow_owned_arr.pa", "array shape '[2]P' does not match the borrowed array type '@[2]$P'."},
		{"error_279_struct_arr_borrow_embed_arr.pa", "array shape '[2]$P' does not match the borrowed array type '@[2]P'."},
		{"error_280_embed_struct_arr_borrow_size_mismatch.pa", "array shape '[3]$P' does not match the borrowed array type '@[2]$P'."},
		{"error_281_embed_struct_arr_field_access.pa", ":3:11: error: field access on non-struct variable."},
		{"error_303_arr_borrow_addr_of_param.pa", ":1:25: error: 'g' is already a borrow"},
		{"error_304_arr_borrow_addr_of_local.pa", ":3:16: error: 'g' is already a borrow"},
		{"error_305_arr_borrow_row_by_name.pa", ":3:1: error: an array given to a borrowed array"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_borrow)
{
	const pair<string, string> cases[] = {
		{"error_248_struct_borrow_no_addr_of.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_249_struct_borrow_c_no_addr_of.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_250_struct_borrow_init_no_addr_of.pa", ":3:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_251_struct_borrow_ro_to_mut.pa", ":4:1: error: cannot bind a read-only pointer"},
		{"error_252_struct_borrow_owned_field_ro.pa", ":3:27: error: cannot write through read-only pointer"},
		{"error_253_ro_ptr_embed_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_254_ro_ptr_embed_arr_field_addr_mut.pa", ":3:30: error: cannot write through read-only pointer '@T'"},
		{"error_255_ro_ptr_owned_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_256_ro_ptr_embed_struct_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_257_ro_ptr_owned_struct_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_258_ro_ptr_ptr_slot_arr_field_store.pa", ":3:29: error: cannot write through read-only pointer '@T'"},
		{"error_259_ro_ptr_raw_ptr_field_index_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_260_mut_ptr_ro_elem_arr_field_write.pa", ":3:22: error: cannot write through read-only pointer array element"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_arr_2d)
{
	const pair<string, string> cases[] = {
		{"error_261_struct_arr_2d_embed_outer.pa", ":2:1: error: a two-dimensional struct array must be declared as '[m][n]T' or '[m][n]$T'."},
		{"error_262_struct_arr_2d_embed_both.pa", ":2:1: error: a two-dimensional struct array must be declared as '[m][n]T' or '[m][n]$T'."},
		{"error_360_unsized_struct_arr_2d_embed.pa", ":2:1: error: a two-dimensional struct array must be declared as '[m][n]T' or '[m][n]$T'."},
		{"error_291_embed_struct_arr_2d_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer '@T'"},
		{"error_292_embed_struct_arr_2d_borrow_owned.pa", "array shape '[2][3]$Point' does not match the borrowed array type '@[2][3]Point'."},
		{"error_293_struct_arr_2d_borrow_embed.pa", "array shape '[2][3]Point' does not match the borrowed array type '@[2][3]$Point'."},
		{"error_294_embed_struct_arr_2d_borrow_size.pa", "array shape '[2][4]$Point' does not match the borrowed array type '@[2][3]$Point'."},
		{"error_295_embed_struct_arr_2d_owned_sub.pa", ":3:1: error: cannot embed 'Box' with '$': it owns fields; use 'Box' without '$'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_arr_lit)
{
	const pair<string, string> cases[] = {
		{"error_263_struct_lit_count.pa", ":2:21: error: struct 'Point' has 2 fields but its literal has 1 values."},
		{"error_264_struct_lit_dup_field.pa", ":2:26: error: field 'x' is given more than once in a struct literal."},
		{"error_265_struct_lit_missing_field.pa", ":2:15: error: the literal of struct 'Point' is missing field 'y'."},
		{"error_266_struct_lit_unknown_field.pa", ":2:21: error: struct 'Point' has no field 'z'."},
		{"error_267_struct_lit_ptr_field.pa", ":2:15: error: field 'p' of struct 'P' cannot be initialized by a literal: only numeric, struct and array fields are supported."},
		{"error_268_struct_lit_narrowing.pa", ":3:17: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_269_dict_lit_prim_elem.pa", ":1:15: error: a '{name: value}' literal can only be used as a struct value: a struct variable's initializer, the source of '->' to a struct, an element of an array literal, or an argument for a struct parameter."},
		{"error_270_dict_lit_context.pa", ":3:5: error: parameter 'p' of type 'int32' cannot take a '{name: value}' literal: only a struct parameter can."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ptr_slot_arr)
{
	const pair<string, string> cases[] = {
		{"error_282_prim_ptr_slot_arr_write_ro.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
		{"error_283_ptr_slot_arr_borrow_elem_upgrade.pa", "array shape '[2]@P' does not match the borrowed array type '@[2]@!P'."},
		{"error_284_ptr_slot_arr_borrow_narrow_mut_slots.pa", "array shape '[2]@!P' does not match the borrowed array type '@![2]@P'."},
		{"error_285_ptr_slot_arr_borrow_owned_arr.pa", "array shape '[2]P' does not match the borrowed array type '@![2]@!P'."},
		{"error_286_ptr_slot_arr_borrow_write_ro_slot.pa", ":3:2: error: cannot write through read-only pointer '@T'"},
		{"error_287_ptr_slot_arr_borrow_write_ro_elem.pa", ":3:7: error: cannot write through read-only pointer array element"},
		{"error_288_prim_ptr_slot_arr_borrow_as_prim.pa", "array shape '[2]@int32' does not match the borrowed array type '@[2]int32'."},
		{"error_289_arr_borrow_embed_ptr_elem.pa", "neither a struct nor a pointer inside a '$[m]' row"},
		{"error_290_arr_borrow_runtime_inner_size.pa", "array shape '[2][?]int32' does not match the borrowed array type '@[2][3]int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, copy)
{
	const pair<string, string> cases[] = {
		{"error_207_arr_var_init_not_literal.pa", ":1:1: error: cannot copy 'int64' into '[3]int32'"},
		{"error_296_copy_size_mismatch.pa", ":3:1: error: cannot copy '[4]int32' into '[3]int32'"},
		{"error_297_copy_runtime_size.pa", ":4:1: error: cannot copy '[?]int32' into '[?]int32'"},
		{"error_298_copy_elem_type.pa", ":2:1: error: cannot copy '[3]int32' into '[3]int64'"},
		{"error_299_copy_struct_type.pa", ":5:1: error: cannot copy 'P' into 'Q'"},
		{"error_300_copy_into_ro_elem.pa", ":3:2: error: cannot write through read-only pointer '@T'"},
		{"error_301_copy_ptr_slots_into_structs.pa", ":4:1: error: cannot copy '[2]@P' into '[2]P'"},
		{"error_302_copy_unsupported_shape.pa", ":2:2: error: copying '@![2][3][4]int32' is not supported in this version."},
		{"error_374_arr_copy_void_init.pa", ":2:1: error: Void function call cannot be used as a value."},
		{"error_376_copy_runtime_row_size.pa", ":3:1: error: cannot copy '[3]$[?]int32' into '[3]$[?]int32'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, unknown_type_name)
{
	const pair<string, string> cases[] = {
		{"error_306_unknown_arr_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_307_unknown_ptr_slot_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_308_unknown_ptr_slot_unsized_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_309_unknown_arr2d_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_310_unknown_arr2d_embed_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_311_unknown_arr_borrow_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_312_unknown_arr_elem_via_alias.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_314_unknown_unsized_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_315_unknown_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_316_unknown_arr_borrow_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_317_unknown_named_ret.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_331_unknown_ptr_field.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_332_unknown_ptr_slot_field.pa", ":1:1: error: unknown type 'Foo'."},
		// A block's types are not visible from a sibling block.
		{"error_333_ptr_field_sibling_block_def.pa", ":2:2: error: unknown type 'B'."},
		// A declaration after another statement starts at its own first token.
		{"error_334_unknown_ptr_field_after_stmt.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_335_unknown_param_after_stmt.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_336_named_ret_init_narrowing.pa", ":1:30: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_337_named_ret_init_struct.pa", ":2:19: error: struct-type named return 'p' cannot have an initializer."},
		{"error_338_tapple_assign_narrowing.pa", ":3:13: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_339_tapple_assign_count.pa", ":3:1: error: Variable count does not match return count of 'f'."},
		{"error_340_tapple_assign_single_ret.pa", ":3:1: error: Function 'g' does not have multiple return values."},
		{"error_341_tapple_decl_narrowing.pa", ":2:1: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_342_tapple_decl_struct_mismatch.pa", ":3:1: error: cannot convert 'P' to 'int32'."},
		{"error_343_tapple_decl_unknown_type.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_344_transfer_non_owned_field.pa", ":3:1: error: '->>' needs a field that owns its value (T or [n]T); 'n' does not."},
		{"error_345_unsized_ret_copy.pa", ":3:1: error: cannot copy '[?]int32' into '[3]int32'"},
		{"error_346_ret_struct_borrow_decl.pa", ":3:1: error: a struct returned by a call has no owner to borrow from"},
		{"error_347_ret_struct_borrow_assign.pa", ":5:1: error: a struct returned by a call has no owner to borrow from"},
		{"error_348_ret_struct_borrow_slot.pa", ":4:1: error: a struct returned by a call has no owner to borrow from"},
		{"error_349_ret_struct_borrow_field.pa", ":5:1: error: a struct returned by a call has no owner to borrow from"},
		{"error_350_ret_struct_borrow_return.pa", ":3:18: error: a struct returned by a call has no owner to borrow from"},
		{"error_351_transfer_from_borrow_to_field.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_352_transfer_from_borrow_to_elem.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_353_transfer_from_param.pa", ":4:2: error: '->>' needs a source that owns its value"},
		{"error_354_transfer_from_ptr_field.pa", ":5:1: error: '->>' needs a source that owns its value"},
		{"error_355_transfer_from_readonly_elem.pa", ":4:2: error: cannot write through read-only pointer"},
		{"error_356_transfer_to_ptr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_357_transfer_to_arr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_358_transfer_row_size_mismatch.pa", ":3:1: error: cannot transfer '[5]int32' into '[3]int32'"},
		{"error_359_transfer_to_dynamic_row.pa", ":4:1: error: cannot transfer '[3]int32' into '[?]int32'"},
		{"error_361_struct_to_embed_struct_param.pa", ":4:1: error: cannot convert 'P' to '[?]$P'."},
		{"error_362_transfer_to_embed_ptr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_363_unsized_rows_arg_mismatch.pa", ":3:1: error: array shape '[2][4]int32' does not match '[?][3]int32'"},
		{"error_364_unsized_rows_ret_mismatch.pa", ":2:2: error: array shape '[?][4]int32' does not match '[?][3]int32'"},
		{"error_365_unsized_rows_slot_mismatch.pa", ":3:1: error: array shape '[2][4]int32' does not match '[?][3]int32'"},
		{"error_366_unsized_rows_size_not_const.pa", ":2:1: error: the row size in '[][m]T' must be a compile-time constant."},
		{"error_367_unsized_rows_3d.pa", ":1:1: error: '[][m]T' supports only"},
		{"error_368_unsized_rows_ptr_elem.pa", ":2:1: error: '[][m]T' supports only"},
		{"error_369_unsized_rows_1d_borrow.pa", ":3:1: error: array shape '@[3]int32' does not match '[?][3]int32'"},
		{"error_370_slot_arr_ret_as_owned_rows.pa", ":3:2: error: array shape '[?]@!int32' does not match '[?][?]int32'"},
		{"error_371_borrow_elems_arg_as_owned.pa", ":6:1: error: array shape '[1]@!P' does not match '[?]P'"},
		{"error_372_owned_elems_arg_as_borrow.pa", ":5:1: error: array shape '[2]P' does not match '[?]@!P'"},
		{"error_373_slot_arr_into_owned_rows_slot.pa", ":3:1: error: array shape '[3]@!int32' does not match '[?][?]int32'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, embed_elem_not_struct)
{
	const pair<string, string> cases[] = {
		{"error_318_embed_prim_2d_var.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_319_embed_prim_row_elem.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_320_embed_prim_alias.pa", ":2:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_321_embed_prim_param.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_322_embed_ptr_elem.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not '@int32'."},
		{"error_572_embed_prim_field.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, field_2d_arr_unsupported)
{
	const pair<string, string> cases[] = {
		{"error_323_field_2d_arr.pa", ":1:1: error: unsupported struct field type."},
		{"error_324_field_2d_embed_prim.pa", ":1:1: error: unsupported struct field type."},
		{"error_325_field_2d_embed_struct.pa", ":2:1: error: unsupported struct field type."},
		{"error_524_field_var_arr_type.pa", ":1:1: error: unsupported struct field type."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, embed_owning_struct)
{
	// Owned array fields count as owning just like owned struct fields,
	// and a plain $T field is rejected the same way as [n]$T.
	const pair<string, string> cases[] = {
		{"error_326_embed_arr_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns fields"},
		{"error_327_embed_arr_2d_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns fields"},
		{"error_328_embed_arr_field_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns fields"},
		{"error_329_embed_field_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns fields"},
		{"error_330_embed_field_owning_struct_field.pa", ":3:1: error: cannot embed 'Rect' with '$': it owns fields"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, row_borrow_readonly)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_377_row_borrow_readonly.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, row_borrow_size)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_378_row_borrow_size.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("array shape '[3]int32' does not match the borrowed array type '@[4]int32'"), string::npos);
}

TEST(sa_error, row_borrow_runtime_size)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_379_row_borrow_runtime_size.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("array shape '[?]int32' does not match the borrowed array type '@[4]int32'"), string::npos);
}

TEST(sa_error, row_by_name)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_380_row_by_name.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("must be written as '@name' or '@!name'"), string::npos);
}

TEST(sa_error, assign_chain_through_tapple)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_381_assign_chain_through_tapple.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: this statement is not supported"), string::npos);
}

TEST(sa_error, c_call_too_many_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_382_c_call_too_many_args.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'abs' takes 1 argument(s), but 2 given."), string::npos);
}

TEST(sa_error, c_call_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_383_c_call_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'abs' takes 1 argument(s), but 0 given."), string::npos);
}

TEST(sa_error, c_void_call_with_arg)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_384_c_void_call_with_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'getchar' takes 0 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, variadic_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_385_variadic_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'printf' takes at least 1 argument(s), but 0 given."), string::npos);
}

TEST(sa_error, pln_call_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_386_pln_call_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'f' takes 2 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, pln_call_too_many_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_387_pln_call_too_many_args.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'f' takes 0 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, alias_call_arg_count)
{
	cleanTestEnv();
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_388_alias_call_arg_count.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'square' takes 1 argument(s), but 2 given."), string::npos);
}

TEST(sa_error, enum_from_literal)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_389_enum_from_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_390_enum_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_from_other_enum)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_391_enum_from_other_enum.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'Fruit' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_to_narrower)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_392_enum_to_narrower.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'Color' to 'uint8' is not allowed; write an explicit cast 'uint8(...)'."), string::npos);
}

TEST(sa_error, enum_cast_from_float)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_393_enum_cast_from_float.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: Cannot cast type 'flo64' to 'Color'."), string::npos);
}

TEST(sa_error, unknown_enumerator)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_394_unknown_enumerator.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: enum 'Color' has no enumerator 'PURPLE'."), string::npos);
}

TEST(sa_error, duplicate_enumerator)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_395_duplicate_enumerator.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:18: error: enumerator 'A' is already defined in enum 'E'."), string::npos);
}

TEST(sa_error, duplicate_type_name)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_396_duplicate_type_name.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: type 'Color' is already defined."), string::npos);
}

TEST(sa_error, enum_value_not_const)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_397_enum_value_not_const.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:15: error: enumerator 'A' must be given an integer constant expression."), string::npos);
}

TEST(sa_error, enum_value_overflow)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_398_enum_value_overflow.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:40: error: value 9223372036854775808 of enumerator 'B' is out of the int64 range."), string::npos);
}

TEST(sa_error, enum_cast_arg_count)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_399_enum_cast_arg_count.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: cast 'Color(...)' takes exactly one argument."), string::npos);
}

TEST(sa_error, enum_param_from_literal)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_400_enum_param_from_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_value_out_of_range)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_401_enum_value_out_of_range.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:15: error: value 99999999999999999999 of enumerator 'A' is out of the int64 range."), string::npos);
}

TEST(sa_error, enum_name_conflicts_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_402_enum_name_conflicts_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: type 'P' is already defined."), string::npos);
}

TEST(sa_error, c_enum_field_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_403_c_enum_field_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'int32' to 'CSpace' is not allowed"), string::npos);
}

TEST(sa_error, c_enum_name_conflict)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_404_c_enum_name_conflict.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	// A top-level cinclude's types are registered before any Palan declaration.
	ASSERT_NE(sa.find(":1:1: error: type 'Color' is already defined."), string::npos);

	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_525_c_enum_name_conflict_nested.pa -o " + ast_out), "");
	sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:2: error: type 'Color' is already defined."), string::npos);
}

TEST(sa_error, type_name_conflict)
{
	const pair<string, string> cases[] = {
		{"error_526_struct_name_conflicts_c_struct.pa", ":1:1: error: type 'tm' is already defined."},
		{"error_527_struct_name_conflicts_c_enum.pa", ":1:1: error: type 'Color' is already defined."},
		{"error_528_alias_name_conflicts_c_typedef.pa", ":1:1: error: type 'time_t' is already defined."},
		{"error_529_struct_name_duplicate.pa", ":2:1: error: type 'P' is already defined."},
		{"error_530_alias_name_conflicts_struct.pa", ":2:1: error: type 'P' is already defined."},
		{"error_531_c_struct_name_conflict_nested.pa", ":3:2: error: type 'tm' is already defined."},
		{"error_532_c_typedef_name_conflict_nested.pa", ":3:2: error: type 'time_t' is already defined."},
		{"error_533_import_type_conflicts_c_struct.pa", ":1:1: error: type 'Vec' is already defined."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_types2.pa -o out/lib_sa_types2.pa.ast.json");
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, enum_field_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_405_enum_field_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed"), string::npos);
}

TEST(sa_error, sizeof)
{
	const pair<string, string> cases[] = {
		{"error_406_sizeof_unknown_type.pa", ":1:12: error: unknown type 'Foo'."},
		{"error_407_sizeof_incomplete_struct.pa", ":2:12: error: struct '__dirstream' is only forward-declared"},
		{"error_408_sizeof_unsized_arr.pa", ":1:12: error: every array size in the type of 'sizeof' must be a compile-time constant."},
		{"error_409_sizeof_arr_size_not_const.pa", ":2:12: error: every array size in the type of 'sizeof' must be a compile-time constant."},
		{"error_410_sizeof_embed_prim.pa", ":1:12: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_411_sizeof_embed_ptr.pa", ":1:12: error: '$' applies only to a struct element or a '$[m]' row, not '@int32'."},
		{"error_573_sizeof_embed_unknown.pa", ":1:12: error: unknown type 'Foo'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, expr_not_implemented)
{
	const pair<string, string> cases[] = {
		{"error_412_expr_not_impl_var_init.pa", ":1:11: error: this expression is not supported"},
		{"error_413_expr_not_impl_sizeof.pa", ":1:12: error: this expression is not supported"},
		{"error_414_expr_not_impl_nested_arg.pa", ":2:16: error: this expression is not supported"},
		{"error_415_expr_not_impl_return.pa", ":3:9: error: this expression is not supported"},
		{"error_416_expr_not_impl_assign.pa", ":2:1: error: this expression is not supported"},
		{"error_417_expr_not_impl_stmt.pa", ":1:1: error: this expression is not supported"},
		{"error_521_chain_tapple_value.pa", ":4:1: error: this expression is not supported"},
		{"error_523_expr_not_impl_sizeof_nested.pa", ":1:12: error: this expression is not supported"},
		{"error_553_arg_move_owner.pa", ":3:3: error: this expression is not supported"},
		{"error_554_c_arg_move_owner.pa", ":3:16: error: this expression is not supported"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

// None of these functions is called: the signature alone must be rejected.
TEST(sa_error, sig_not_implemented)
{
	const string defaultNotImpl = "has a default value; default values are not supported yet.";
	const pair<string, string> cases[] = {
		{"error_544_param_untyped_default.pa", ":1:8: error: parameter 'x' " + defaultNotImpl},
		{"error_545_param_typed_default.pa", ":1:18: error: parameter 'x' " + defaultNotImpl},
		{"error_546_param_inherit_default.pa", ":1:21: error: parameter 'b' " + defaultNotImpl},
		{"error_547_param_move_owner.pa", ":1:8: error: this parameter is not supported in this version."},
		{"error_548_param_var_arr_type.pa", ":1:8: error: this parameter is not supported in this version."},
		{"error_549_syscall_param_default.pa", ":1:11: error: parameter 'x' " + defaultNotImpl},
		{"error_550_inner_func_param.pa", ":2:13: error: this parameter is not supported in this version."},
		{"error_551_ret_var_arr_type.pa", ":1:13: error: this return value is not supported in this version."},
		{"error_552_ret_untyped.pa", ":1:13: error: this return value is not supported in this version."},
		{"error_555_import_not_impl_sig.pa", "lib_sa_not_impl_sig.pa:1:15: error: this parameter is not supported in this version."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_not_impl_sig.pa -o out/lib_sa_not_impl_sig.pa.ast.json");
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, method_call)
{
	const string notMethod = "cannot be called as a method: its first parameter must be a writable borrow ('@!').";
	const pair<string, string> cases[] = {
		{"error_418_method_ro_first_param.pa", ":4:1: error: 'f' " + notMethod},
		{"error_419_method_value_first_param.pa", ":3:1: error: 'f' " + notMethod},
		{"error_420_method_no_param.pa", ":4:1: error: 'f' " + notMethod},
		{"error_421_method_c_const_first_param.pa", ":3:11: error: 'strlen' " + notMethod},
		{"error_422_method_recv_not_addressable.pa", ":4:1: error: cannot take the address of this expression"},
		{"error_423_method_recv_ro_borrow.pa", ":4:2: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, import_type)
{
	const pair<string, string> cases[] = {
		{"error_424_import_type_unqualified.pa", ":2:1: error: type 'Vec' requires a module alias qualifier: 'V.Vec'."},
		{"error_425_import_enum_unqualified.pa", ":2:17: error: type 'Color' requires a module alias qualifier: 'V.Color'."},
		{"error_426_import_type_not_selected.pa", ":2:1: error: unknown type 'Vec'."},
		{"error_427_import_type_private_dep.pa", ":2:1: error: unknown type 'Inner'."},
		{"error_428_import_type_local_conflict.pa", ":2:1: error: type 'Vec' is already defined."},
		{"error_429_import_type_module_conflict.pa", ":2:1: error: type 'Vec' is already defined."},
		// The imported module's own path and line, not the importer's.
		{"error_430_import_bad_struct_def.pa", "lib_sa_bad_struct.pa:2:1: error: unsupported struct field type."},
		// A block's import makes its types writable only in that block.
		{"error_568_block_import_type_outside.pa", ":4:1: error: unknown type 'Vec'."},
		{"error_569_block_import_alias_outside.pa", ":4:1: error: unknown type 'MyInt'."},
		{"error_570_block_import_enum_outside.pa", ":4:11: error: unknown type 'Color'."},
		{"error_571_block_import_qualified_outside.pa", ":4:1: error: unknown type 'V.Vec'."},
		// PP is lib's P, a different type from main's P of the same name.
		{"error_641_import_alias_private_type.pa", ":4:1: error: cannot convert '@!P' to '@!P'."},
		// An import making a name writable still clashes with a local type of that name.
		{"error_642_block_local_then_import.pa", ":3:2: error: type 'Vec' is already defined."},
		{"error_643_top_local_block_import.pa", ":3:2: error: type 'Vec' is already defined."},
		{"error_644_top_alias_block_import.pa", ":3:2: error: type 'MyInt' is already defined."},
		{"error_645_hidden_after_block_local.pa", ":6:1: error: type 'Vec' requires a module alias qualifier: 'V.Vec'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		for (string lib : {"lib_sa_types", "lib_sa_types2", "lib_sa_bad_struct", "lib_sa_private_alias"})
			execTestCommand("bin/palan-gen-ast ../test/testdata/sa/" + lib + ".pa -o out/" + lib + ".pa.ast.json");
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, circular_type_decl)
{
	const pair<string, string> cases[] = {
		// Reported where the cycle closes: A, reached from the import of B.
		{"error_433_import_mutual_embed.pa", "error_433_import_mutual_embed.pa:2:1: error: type 'B' depends on itself: B -> A -> B."},
		{"error_434_owned_cycle.pa", ":2:1: error: type 'P' depends on itself: P -> Q -> P."},
		{"error_435_alias_cycle.pa", ":2:1: error: type 'X' depends on itself: X -> Y -> X."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		for (string f : {"lib_sa_mutual_cycle", "error_433_import_mutual_embed"})
			execTestCommand("bin/palan-gen-ast ../test/testdata/sa/" + f + ".pa -o out/" + f + ".pa.ast.json");
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, alias_cast)
{
	const pair<string, string> cases[] = {
		{"error_436_cast_alias_struct.pa", ":3:11: error: cannot cast to 'PT': only a primitive, enum, pointer, or borrowed array ('@[n]T'/'@![n]T') type can be a cast target."},
		{"error_437_cast_alias_array.pa", ":2:11: error: cannot cast to 'R': only a primitive, enum, pointer, or borrowed array ('@[n]T'/'@![n]T') type can be a cast target."},
		{"error_438_cast_opaque.pa", ":2:11: error: cannot cast to 'FILE': only a primitive, enum, pointer, or borrowed array ('@[n]T'/'@![n]T') type can be a cast target."},
		{"error_439_cast_alias_arg_count.pa", ":2:8: error: cast 'U8(...)' takes exactly one argument."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, as_cast)
{
	const pair<string, string> cases[] = {
		{"error_579_cast_owned_arr.pa", ":2:14: error: cannot cast to an owned array: only a borrowed array ('@[n]T'/'@![n]T') can be a cast target."},
		{"error_580_cast_ptr_to_narrow_int.pa", ":3:11: error: Cannot cast type '@!int32' to 'int32'."},
		{"error_581_cast_int_to_ptr.pa", ":2:13: error: Cannot cast type 'int32' to '@!int32'."},
		{"error_582_cast_int_to_borrowed_arr.pa", ":2:16: error: Cannot cast type 'uint64' to '@![4]int32'."},
		{"error_583_cast_struct_value.pa", ":3:7: error: cannot cast to 'P': only a primitive, enum, pointer, or borrowed array ('@[n]T'/'@![n]T') type can be a cast target."},
		{"error_584_cast_struct_to_ptr.pa", ":3:13: error: Cannot cast type 'P' to '@!uint8'."},
		{"error_585_cast_enum_to_ptr.pa", ":3:13: error: Cannot cast type 'Big' to '@!int32'."},
		{"error_586_cast_ptr_to_enum.pa", ":3:9: error: Cannot cast type '@!int32' to 'Big'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_field_size)
{
	const pair<string, string> cases[] = {
		{"error_431_struct_field_var_size.pa", ":4:2: error: struct field array size must be a compile-time constant integer; 'n' is not a const."},
		{"error_567_struct_field_var_size_top.pa", ":2:1: error: struct field array size must be a compile-time constant integer; 'n' is not a const."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, block_decl_scope)
{
	const pair<string, string> cases[] = {
		{"error_556_block_type_outside.pa", ":4:1: error: unknown type 'Foo'."},
		{"error_557_block_const_outside.pa", ":4:11: error: Undefined variable 'N'."},
		{"error_558_block_alias_outside.pa", ":4:1: error: unknown type 'I'."},
		{"error_559_block_enum_outside.pa", ":4:11: error: Undefined variable 'E'."},
		{"error_560_block_type_shadow_top.pa", ":3:2: error: type 'Foo' is already defined."},
		{"error_561_block_type_shadow_c.pa", ":3:2: error: type 'FILE' is already defined."},
		{"error_562_block_type_shadow_outer_block.pa", ":4:3: error: type 'Foo' is already defined."},
		{"error_563_block_const_shadow.pa", ":4:3: error: Constant 'N' is already defined."},
		{"error_564_block_dup_decl.pa", ":3:2: error: Constant 'Foo' is already defined."},
		// A C type a block's cinclude brings in clashes with the block's own.
		{"error_565_block_type_then_cinclude.pa", ":3:2: error: type 'tm' is already defined."},
		{"error_566_block_type_display_name.pa", ":4:2: error: cannot convert 'Foo' to 'int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, const_name_dup)
{
	const pair<string, string> cases[] = {
		{"error_630_top_const_dup.pa", ":2:1: error: Constant 'N' is already defined."},
		{"error_631_top_type_then_const.pa", ":2:1: error: Constant 'M' is already defined."},
		{"error_632_top_const_then_alias.pa", ":2:1: error: type 'M' is already defined."},
		{"error_633_top_alias_dup.pa", ":2:1: error: type 'A' is already defined."},
		{"error_634_top_var_then_const.pa", ":2:1: error: Constant 'K' is already defined."},
		{"error_635_top_const_then_var.pa", ":2:1: error: Variable 'K' is already declared."},
		{"error_636_top_const_param.pa", ":2:1: error: Variable 'N' is already declared."},
		{"error_637_block_const_outer_var.pa", ":3:2: error: Constant 'K' is already defined."},
		{"error_638_block_var_then_const.pa", ":3:2: error: Constant 'K' is already defined."},
		{"error_639_top_const_c_type.pa", ":2:1: error: Constant 'time_t' is already defined."},
		{"error_640_block_tapple_then_const.pa", ":6:2: error: Constant 'K' is already defined."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_ptr_elem_copy)
{
	const pair<string, string> cases[] = {
		{"error_440_copy_to_readonly_ptr_elem.pa", ":2:24: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_lit_init_assign)
{
	const pair<string, string> cases[] = {
		{"error_441_struct_lit_to_borrow.pa", ":4:1: error: a struct literal cannot be given to a borrow '@T'; write to its '[0]' to copy into the struct it points to."},
		{"error_442_struct_lit_init_borrow.pa", ":3:1: error: a struct literal cannot be given to a borrow '@T'; write to its '[0]' to copy into the struct it points to."},
		{"error_443_struct_lit_to_readonly_elem.pa", ":2:16: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_444_struct_lit_assign_count.pa", ":3:1: error: struct 'V' has 2 fields but its literal has 3 values."},
		{"error_445_struct_lit_assign_ptr_field.pa", ":4:2: error: field 'p' of struct 'P' cannot be initialized by a literal: only numeric, struct and array fields are supported."},
		{"error_446_struct_lit_transfer.pa", ":3:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, or a call result."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, named_ret_struct_redecl)
{
	const pair<string, string> cases[] = {
		{"error_447_named_ret_struct_redecl.pa", ":3:2: error: Variable 'p' is already declared."},
		{"error_448_named_ret_struct_nested_decl.pa", ":4:3: error: Variable 'p' is already declared."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, named_ret_single_return_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_449_named_ret_single_return_value.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:2: error: Function with named return values requires bare 'return;' statement."), string::npos) << sa;
}

TEST(sa_error, struct_init_from_c_ptr_ret)
{
	// A C pointer return has no owner to take over, unlike a Palan struct return.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_450_struct_init_from_c_ptr_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: a 'tm' variable can only be initialized from a call that returns a 'tm' (a Palan function, or a C function returning it by value)."), string::npos) << sa;
}

TEST(sa_error, struct_init_other_struct_ret)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_451_struct_init_other_struct_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: cannot copy 'Q' into 'P'"), string::npos) << sa;
}

TEST(sa_error, return_arr_lit_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_452_return_arr_lit_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:28: error: an array literal can only be used as"), string::npos) << sa;
}

TEST(sa_error, return_struct_lit_field_count)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_453_return_struct_lit_field_count.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:24: error: struct 'P' has 2 fields but its literal has 1 values."), string::npos) << sa;
}

TEST(sa_error, arr_lit_assign)
{
	const pair<string, string> cases[] = {
		{"error_454_arr_lit_assign_count.pa", ":3:2: error: array 'albedo' has size 4 but its array literal has 2 elements."},
		{"error_455_arr_lit_assign_row_size.pa", ":2:7: error: array 'g' has rows of size 2 but a row of its array literal has 3 elements."},
		{"error_456_arr_lit_assign_size_not_const.pa", ":3:1: error: array 'a' given an array literal must have a compile-time constant size."},
		{"error_457_arr_lit_assign_param.pa", ":2:2: error: array 'p' given an array literal must have a compile-time constant size."},
		{"error_458_arr_lit_assign_borrow.pa", ":2:2: error: an array literal cannot be given to the borrowed array 'b'; '->' to a borrow rebinds it, so write to its elements instead."},
		{"error_459_arr_lit_assign_ptr_elem.pa", ":3:1: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_460_arr_lit_assign_transfer.pa", ":3:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, or a call result."},
		{"error_461_arr_lit_assign_readonly_field.pa", ":3:12: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_462_addr_of_readonly_arr_field.pa", ":3:19: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_463_arr_lit_assign_dict.pa", ":2:1: error: a '{name: value}' literal can only be used as a struct value"},
		{"error_464_struct_lit_arr_field_count.pa", ":2:8: error: array 'albedo' has size 4 but its array literal has 2 elements."},
		{"error_465_struct_lit_arr_field_not_lit.pa", ":3:8: error: the array literal's dimensions do not match array 'v'."},
		{"error_466_struct_lit_arr_field_dict.pa", ":2:8: error: a '{name: value}' literal can only be used as a struct value"},
		{"error_467_struct_lit_ptr_arr_field.pa", ":3:8: error: field 'ps' of struct 'M' cannot be initialized by a literal: only numeric, struct and array fields are supported."},
		{"error_468_struct_lit_not_struct.pa", ":3:8: error: a value of struct 'P' must be written as '[...]' or '{name: value, ...}'."},
		{"error_469_struct_lit_other_struct.pa", ":4:12: error: cannot copy 'R' into 'P'"},
		{"error_470_embed_field_to_borrow.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@[..]' or '@![..]' for a literal)."},
		{"error_471_display_struct_value.pa", ":3:1: error: cannot convert 'P' to 'int64'."},
		{"error_472_display_struct_param.pa", ":3:1: error: cannot convert 'int64' to 'P'."},
		{"error_473_display_arr.pa", ":2:1: error: cannot convert '[3]int32' to 'int64'."},
		{"error_474_display_arr_2d.pa", ":2:1: error: cannot convert '[2][3]int32' to 'int64'."},
		{"error_475_display_arr_contig.pa", ":2:1: error: cannot convert '[2]$[3]int32' to 'int64'."},
		{"error_476_display_struct_arr.pa", ":3:1: error: cannot convert '[2]P' to 'int64'."},
		{"error_477_display_embed_struct_arr.pa", ":3:1: error: cannot convert '[2]$P' to 'int64'."},
		{"error_478_display_slot_arr.pa", ":2:1: error: cannot convert '[3]@!int32' to 'int64'."},
		{"error_479_display_arr_borrow.pa", ":2:2: error: cannot convert '@[2][3]int32' to 'int64'."},
		{"error_480_display_arr_borrow_row.pa", ":2:2: error: cannot convert '@[3]int32' to 'int64'."},
		{"error_481_display_cast_struct.pa", ":3:11: error: Cannot cast type 'P' to 'int64'."},
		{"error_482_call_borrow_needs_at.pa", ":4:11: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@[..]' or '@![..]' for a literal)."},
		{"error_483_call_borrow_decl.pa", ":3:8: error: a struct returned by a call has no owner to borrow from"},
		{"error_484_call_borrow_ro_to_mut.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_485_call_borrow_not_struct.pa", ":3:3: error: cannot take the address of this expression"},
		{"error_486_temp_borrow_decl.pa", ":5:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_487_temp_borrow_assign.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_488_temp_borrow_return.pa", ":5:29: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_489_temp_borrow_field.pa", ":7:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_490_temp_borrow_slot.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_491_temp_borrow_nested.pa", ":5:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_492_temp_borrow_prim.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_493_temp_borrow_tapple.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_494_member_call_borrow_not_arg.pa", ":4:9: error: a struct returned by a call has no owner to borrow from"},
		{"error_653_temp_borrow_pinned.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}


TEST(sa_error, lit_arg)
{
	const pair<string, string> cases[] = {
		{"error_495_lit_arg_needs_at.pa", ":3:1: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@[..]' or '@![..]' for a literal)."},
		{"error_496_arr_lit_arg_needs_at.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') must be written as '@name' or '@!name' ('@[..]' or '@![..]' for a literal)."},
		{"error_497_lit_arg_count.pa", ":2:5: error: array 'a' has size 3 but its array literal has 2 elements."},
		{"error_498_lit_arg_field_count.pa", ":3:5: error: struct 'P' has 2 fields but its literal has 1 values."},
		{"error_499_dict_lit_arr_arg.pa", ":2:5: error: parameter 'a' of type '@[3]int32' cannot take a '{name: value}' literal: only a struct parameter can."},
		{"error_500_lit_arg_c_func.pa", ":2:10: error: a literal cannot be passed to C function 'memset': only a by-value struct parameter takes one."},
		{"error_501_lit_arg_unsized.pa", ":2:5: error: parameter 'a' of type '[?]int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_502_lit_arg_ptr_slot.pa", ":4:5: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_503_lit_borrow_not_arg.pa", ":3:9: error: an array literal can only be used as"},
		{"error_504_lit_arg_borrow_escapes.pa", ":4:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_505_lit_arg_read_only.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_614_arr_lit_scalar_param.pa", ":2:4: error: parameter 'a' of type 'int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_615_arr_lit_ptr_param.pa", ":2:6: error: parameter 'p' of type '@!int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_616_lit_arg_c_variadic.pa", ":2:16: error: a literal cannot be passed to C function 'printf': only a by-value struct parameter takes one."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_macro_unexported)
{
	const pair<string, string> cases[] = {
		{"error_617_c_macro_func_like.pa", ":2:11: error: C macro 'FMAC' is function-like; Palan cannot use it."},
		{"error_618_c_macro_not_constant.pa", ":2:11: error: C macro 'NOFOLD' cannot be used: it does not expand to an integer constant."},
		{"error_619_c_macro_arr_field.pa", ":2:1: error: C macro 'NOFOLD' cannot be used: it does not expand to an integer constant."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}


TEST(sa_error, assign_in_expr)
{
	const string msg = "error: an assignment cannot be used inside an expression";
	const pair<string, string> cases[] = {
		{"error_506_assign_in_init.pa", ":3:11: " + msg},
		{"error_507_assign_in_arg.pa", ":3:3: " + msg},
		{"error_508_assign_in_cond.pa", ":3:5: " + msg},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}


TEST(sa_error, void_call_arg)
{
	const pair<string, string> cases[] = {
		{"error_509_void_call_arg.pa", ":3:11: error: Void function call cannot be used as a value."},
		{"error_510_void_call_variadic_arg.pa", ":3:1: error: Void function call cannot be used as a value."},
		{"error_654_void_value_call_target.pa", ":4:1: error: Void function call cannot be used as a value."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}


TEST(sa_error, chain_call)
{
	const pair<string, string> cases[] = {
		{"error_511_chain_type.pa", ":4:6: error: cannot convert 'V' to 'int32'."},
		{"error_512_chain_too_many.pa", ":3:6: error: function 'add' takes 2 argument(s), but 3 given."},
		{"error_513_chain_too_few.pa", ":3:17: error: function 'show' takes 3 argument(s), but 2 given."},
		{"error_514_chain_mut_needs_at.pa", ":4:6: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name'"},
		{"error_515_chain_multi_to_mut.pa", ":3:17: error: cannot convert 'int32' to '@!int32'."},
		{"error_516_chain_void_value.pa", ":3:14: error: Void function call cannot be used as a value."},
		{"error_517_chain_transfer.pa", ":3:1: error: this expression is not supported in this version."},
		{"error_518_chain_method_form.pa", ":5:6: error: '->' cannot pass a value to a method-form call 'x.f()'"},
		{"error_519_chain_multi_to_tapple.pa", ":3:1: error: the return values of a call chained with '->' into a multiple-return call cannot be received"},
		{"error_520_chain_multi_borrow_escapes.pa", ":4:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, unsupported_c_field)
{
	// A variadic function pointer can't be typed as `@func`.
	const string msg = "of 'CB': its type uses 'function pointer', a C type this version cannot represent.";
	const pair<string, string> cases[] = {
		{"error_534_c_func_ptr_field_read.pa", ":3:11: error: cannot access field 'fn' " + msg},
		{"error_535_c_func_ptr_field_write.pa", ":3:1: error: cannot access field 'fn' " + msg},
		{"error_536_c_func_ptr_field_elem.pa", ":3:11: error: cannot access field 'fns' " + msg},
		{"error_537_c_func_ptr_field_addr.pa", ":3:15: error: cannot access field 'fn' " + msg},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_const_ptr_field)
{
	const pair<string, string> cases[] = {
		{"error_574_c_const_ptr_field_write.pa", ":3:1: error: cannot write through read-only pointer '@T'"},
		{"error_575_c_const_ptr_field_hop.pa", ":3:6: error: cannot write through read-only pointer field '@T'"},
		{"error_576_c_ptr_field_bind_read_only.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_577_c_const_ptr_typedef_write.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ptr_ptr_field_chain)
{
	const pair<string, string> cases[] = {
		{"error_538_ptr_ptr_field_chain.pa", ":4:11: error: field access on non-struct variable."},
		{"error_539_c_ptr_ptr_field_chain.pa", ":3:11: error: field access on non-struct variable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ptr_inner_permission)
{
	const string upgrade = "error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'.";
	const string narrowing = "error: cannot bind '@!@!int32' to '@!@int32': "
		"a pointer reached through a mutable pointer '@!' must keep its permission.";
	const pair<string, string> cases[] = {
		{"error_540_ptr_inner_upgrade.pa", ":4:1: " + upgrade},
		{"error_541_ptr_inner_narrowing.pa", ":4:1: " + narrowing},
		{"error_542_ptr_ptr_field_inner_upgrade.pa", ":3:1: " + upgrade},
		{"error_543_c_ptr_inner_narrowing.pa", ":5:1: " + narrowing},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, const_expr)
{
	const string overflow = "error: constant expression overflows its 64-bit range.";
	const string divZero = "error: division by zero in a constant expression.";
	const pair<string, string> cases[] = {
		{"error_587_const_expr_overflow.pa", ":1:11: " + overflow},
		{"error_588_const_expr_uint_overflow.pa", ":1:11: " + overflow},
		{"error_589_const_expr_div_zero.pa", ":1:11: " + divZero},
		{"error_590_const_expr_flo_div_zero.pa", ":1:11: " + divZero},
		{"error_591_const_expr_flo_bitop.pa", ":1:11: error: Bitwise operator operand must be an integer type."},
		{"error_592_const_expr_flo_mod.pa", ":1:11: error: '%' operator is not supported for float types."},
		{"error_597_shift_float.pa", ":2:11: error: Bitwise operator operand must be an integer type."},
		{"error_598_const_shift_float.pa", ":1:11: error: Bitwise operator operand must be an integer type."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, owned_rows_c_ptrptr)
{
	// A row the array owns is not a '@!T' slot, whether or not a size is known.
	const string mismatch = "does not match '[?]@!uint8': its elements must have the same size and layout";
	const pair<string, string> cases[] = {
		{"error_594_owned_rows_c_ptrptr.pa", ":3:1: error: array shape '@![1][4]uint8' " + mismatch},
		{"error_595_owned_unsized_rows_c_ptrptr.pa", ":4:1: error: array shape '@![1][?]uint8' " + mismatch},
		{"error_596_owned_rows_runtime_outer_c_ptrptr.pa", ":4:1: error: array shape '@![?][4]uint8' " + mismatch},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, func_ptr)
{
	const string sigType = " cannot be a parameter or return type of a function pointer;";
	const pair<string, string> cases[] = {
		{"error_599_mutable_func_ptr.pa",
		 ":1:1: error: a function pointer is read-only; write '@func(...)', not '@!func(...)'."},
		{"error_600_func_ptr_struct_param.pa", ":2:1: error: 'P'" + sigType},
		{"error_601_func_ptr_arr_param.pa", ":1:1: error: 'array'" + sigType},
		{"error_602_func_ptr_alias_struct_param.pa", ":3:1: error: 'P'" + sigType},
		{"error_603_variadic_func_value.pa",
		 ":2:27: error: function 'printf' cannot be used as a function pointer: it takes variable arguments."},
		{"error_604_syscall_func_value.pa",
		 ":2:27: error: function 'wr' cannot be used as a function pointer: it is a syscall."},
		{"error_605_owned_param_func_value.pa",
		 ":3:15: error: function 'g' cannot be used as a function pointer: its parameter type 'P' cannot be passed through one."},
		{"error_606_owned_ret_func_value.pa",
		 ":3:19: error: function 'g' cannot be used as a function pointer: its return type 'P' cannot be returned through one."},
		{"error_607_ambiguous_func_ptr_call.pa",
		 ":5:1: error: call to 'f3' is ambiguous: it names both a function pointer and a function."},
		{"error_608_ambiguous_func_ptr_field_call.pa",
		 ":4:1: error: call to 'run' is ambiguous: it names both a function pointer and a function."},
		{"error_609_func_ptr_permission.pa",
		 ":2:1: error: cannot convert '@func(@!int32) -> int32' to '@func(@int32) -> int32'."},
		{"error_610_func_ptr_arg_count.pa", ":3:1: error: function 'f' takes 1 argument(s), but 2 given."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_struct_arg)
{
	const pair<string, string> cases[] = {
		{"error_155_c_by_value_struct_param.pa", ":3:11: error: cannot convert 'int64' to 'Point'."},
		{"error_612_c_struct_arg_func_ref.pa",
		 ":3:25: error: function 'take_i8' cannot be used as a function pointer: its parameter type 'I8' cannot be passed through one."},
		// Only a by-value struct parameter takes a literal; a pointer parameter still doesn't.
		{"error_613_c_ptr_param_lit.pa", ":4:13: error: a literal cannot be passed to C function 'take_i8_ptr': only a by-value struct parameter takes one."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, frame_var)
{
	const pair<string, string> cases[] = {
		{"error_620_frame_prim.pa", ":2:1: error: '$' on a variable applies only to a struct or an array, not 'int32'."},
		{"error_621_frame_ptr.pa", ":2:1: error: '$' on a variable applies only to a struct or an array, not '@P'."},
		{"error_622_frame_arr_size.pa", ":2:1: error: the size of stack array 'a' must be a compile-time constant."},
		{"error_623_return_frame_arr.pa", ":3:5: error: stack array 'a' cannot be returned; its storage ends with the function."},
		// A stack variable doesn't own its storage, so it has nothing to give away.
		{"error_624_transfer_frame_struct.pa", ":5:1: error: '->>' needs a source that owns its value"},
		{"error_625_transfer_frame_arr.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_626_frame_embed_owning.pa", ":3:1: error: cannot embed 'Q' with '$': it owns fields; use 'Q' without '$'."},
		{"error_627_frame_param.pa", ":2:1: error: this parameter is not supported in this version."},
		{"error_628_frame_named_ret.pa", ":2:1: error: this return value is not supported in this version."},
		{"error_629_frame_ret_type.pa", ":2:13: error: this return value is not supported in this version."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, expr_rooted_access)
{
	const pair<string, string> cases[] = {
		{"error_646_call_owned_struct_field.pa", ":3:11: error: a struct or array returned by a call has no owner to access it through"},
		{"error_647_call_owned_struct_field_write.pa", ":3:6: error: a struct or array returned by a call has no owner to access it through"},
		{"error_648_c_byval_struct_field.pa", ":2:17: error: a struct or array returned by a call has no owner to access it through"},
		{"error_649_call_temp_borrow_field.pa", ":4:11: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_650_readonly_cast_field_write.pa", ":4:7: error: cannot write through read-only pointer '@T'"},
		{"error_651_readonly_cast_index_write.pa", ":3:1: error: cannot write through read-only pointer '@T'"},
		{"error_652_call_prim_field.pa", ":2:11: error: field access on non-struct variable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}
