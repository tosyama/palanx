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
	ASSERT_NE(sa.find("Unknown module alias"), string::npos);
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
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
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
	ASSERT_NE(sa.find("unsupported struct field type"), string::npos);
}

TEST(sa_error, recursive_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_069_recursive_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("recursively contains itself"), string::npos);
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
	ASSERT_NE(sa.find("inline struct field"), string::npos);
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
	// Covers: buildStructDef E_UnknownStructType for prim field (sz < 0 branch)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_073_struct_field_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
}

TEST(sa_error, embed_unknown_struct)
{
	// type Foo { $Unknown a; } — embed field with undefined struct name
	// Covers: buildStructDef E_UnknownStructType for embed field
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_074_embed_unknown_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
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
	ASSERT_NE(sa.find("owned sub-struct"), string::npos);
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
	ASSERT_NE(sa.find("read-only pointer array element"), string::npos);
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

TEST(sa_error, field_assign_on_call_base)
{
	// type Point{...}; func f() -> @!Point r {...}; 1 -> f().x; -- the store_loc
	// grammar's func_call alternative normalizes to an unaddressable "not-impl"
	// base (see storeLocToExpr), which resolveObjectChain's field-access
	// recursion must reject explicitly rather than assume a nested object/field
	// shape (previously an unguarded obj["object"] access aborted on this input).
	// Covers: resolveObjectChain(forWrite=true) non-field-access base branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_115_field_assign_on_call_base.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_access_on_tuple_base)
{
	// type Point{...}; type Line{$Point a; $Point b;}; int64 v = (1,2).a.x; --
	// a multi-expression tuple grouping normalizes to "not-impl" (see term's
	// '(' tapple_inner ')' rule), reached here through a read-side two-level
	// field-access chain rather than store_loc.
	// Covers: resolveObjectChain(forWrite=false) non-field-access base branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_116_field_access_on_tuple_base.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, arr_field_size_not_constant)
{
	// type Buf { [1+1]$int64 data; }; -- size-expr is not a lit-int/lit-uint literal
	// Covers: buildStructDef "arr" branch, E_ArrFieldSizeNotConstant
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_080_arr_field_size_not_constant.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("must be a compile-time constant"), string::npos);
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
	ASSERT_NE(sa.find("owned sub-struct"), string::npos);
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
	ASSERT_NE(sa.find("recursively contains itself"), string::npos);
}

TEST(sa_error, write_readonly_arr_field_elem)
{
	// type Watch { [3]@Point observed; }; p -> w.observed[0]; 42 -> w.observed[0].x;
	// -- write-through to a non-mutable embed-ptr-arr struct field element
	// Covers: IT-2508 — resolveObjectChain(forWrite=true) arr-index base case, mutable:false
	// branch (E_WriteToReadOnlyArrElem), exercised via a struct field (embed-ptr-arr)
	// rather than a plain variable array (already covered by write_readonly_arr_elem).
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_083_write_readonly_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("read-only pointer array element"), string::npos);
}

TEST(sa_error, field_access_on_prim_arr_field_elem)
{
	// type Buf { [4]$int64 data; }; printf("%ld\n", buf.data[0].sub);
	// -- data[0] is a primitive embed-arr leaf, not a struct pointer
	// Covers: IT-2508 — sa_expr_arr_index primitive leaf branch (IT-2507) feeding
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
	// Covers: IT-2508 — resolveObjectChain(forWrite=true) arr-index base case, non-struct
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
	// E_UnknownStructType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_086_arr_ptr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
}

TEST(sa_error, struct_embed_arr_field_unknown_prim_type)
{
	// type T { [4]$NoSuchType field; }; -- [n]$T embed-arr, prim leaf unknown
	// Covers: buildStructDef "arr" branch, embed-arr prim-leaf case,
	// E_UnknownStructType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_087_embed_arr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
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
	ASSERT_NE(sa.find("unsupported struct field type"), string::npos);
}

TEST(sa_error, cinclude_typedef_conflict)
{
	// type size_t = int32; then cinclude <string.h>; which resolves size_t to uint64
	// Covers: registerTypedefAliasInType E_ConflictingTypedef branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_089_cinclude_typedef_conflict.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("conflict"), string::npos);
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

TEST(sa_error, addr_of_on_struct)
{
	// `@p;` where p is a struct-typed local variable
	// Covers: sa_expression addr-of E_AddrOfNotPrimitive branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_092_addr_of_on_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of"), string::npos);
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

TEST(sa_error, addr_of_embed_field)
{
	// `@s.in;` where `in` is a struct-typed field (non-primitive leaf) --
	// address-of on a struct field is limited to primitive-typed fields.
	// Covers: sa_expr_addr_of field-access branch -> leaf typeKind != "prim" -> E_AddrOfNotPrimitive
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_107_addr_of_embed_field.pa -o " + ast_out), "");
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
	// `@f();` -- a call expression is not an addressable location.
	// Covers: sa_expr_addr_of fallback branch, "not-impl" object case -> E_AddrOfNotAddressable
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
	// `other -> pt[0];` -- `pt[0]` on a struct pointer is an address
	// computation (addr-only), not a pointer slot; writing the whole element
	// is rejected. IT-2805: guards the new struct-deref addr-only path.
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
	// read-only struct pointer via `p[i].field` is rejected, same as the
	// existing `[n]@Point` array-element case.
	// Covers: resolveObjectChain(forWrite=true) arr-index branch (E_WriteToReadOnlyArrElem)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_103_write_readonly_struct_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("read-only pointer array element"), string::npos);
}

TEST(sa_error, deref_unknown_struct_ptr)
{
	// `func f(@!Foo p) { p[0].bar; }` where `Foo` is never declared. gen-ast has
	// no symbol table, so `Foo` parses as a prim base-type, not a struct one --
	// IT-2805: sa_expr_arr_index's generic (non-struct) branch must reject
	// this at the SA boundary instead of letting elemSizeBytes' -1
	// "unknown type" sentinel leak into elem-size and crash palan-codegen
	// downstream (layer violation).
	// IT-2902 moved this case to a parameter: a local `@!Foo p;` var decl is
	// now rejected at declaration time (sa_var_decl), so only a parameter's
	// pointee (unchecked at signature normalization) still reaches this guard.
	// Covers: sa_expr_arr_index generic branch, sz<0 guard (E_UnknownStructType)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_104_deref_unknown_struct_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param)
{
	// `clock_getcpuclockid(int32(0), p);` where `p` is `@int32` (read-only)
	// and the C parameter is `clockid_t *` (non-const) -- IT-2026-08-31-c2ast-
	// const-capture: C function arguments are now checked by
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

TEST(sa_error, addr_of_arr_row_not_addressable)
{
	// `@!mat[0]` where `mat` is `[2]$[3]int64` -- the row itself is already
	// an address computation (embedded 2D row access), not a storage slot.
	// Covers: sa_expr_addr_of arr-index branch, addr-only(true) input -> E_AddrOfNotPrimitiveElem
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_117_addr_of_arr_row.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this array element"), string::npos);
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
	// too) -- IT-2902: sa_var_decl had no "pntr" branch in its dispatch guard,
	// so the pointee name was never validated at declaration time; without an
	// initializer, this used to compile silently and leave `p` referencing a
	// nonexistent type.
	// Covers: sa_var_decl pntr branch (incl. pntr-of-pntr walk), unknown
	// pointee prim name -> E_UnknownStructType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_120_ptr_decl_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
}

TEST(sa_error, incomplete_struct_var_decl)
{
	// `struct Tag { int x; int cells[2][3]; };` (cinclude'd, "cells" unsupported)
	// then `Tag t;` -- an owned declaration needs Tag's totalSize to calloc it.
	// IT-2904: registerCStruct now downgrades Tag to an incomplete struct
	// (opaque handle) instead of leaving the tag unregistered, so this is a
	// diagnosed E_IncompleteStructType, not "unknown struct type".
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
	// `Tag t;` -- IT-2026-09-06-2905: registerCStruct now registers a
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
	// separate "name not yet registered" E_UnknownStructType path instead of
	// this one.
	// Covers: buildStructDef "embed" branch, sub.isComplete check
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_126_incomplete_struct_native_embed.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
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
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
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
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_embed_arr)
{
	// Same incomplete Tag; `type Wrap { [3]$Tag a; };` (contiguous embedded
	// array field within a *native* struct, distinct from the top-level
	// `[3]$Tag a;` var-decl covered by incomplete_struct_embed_arr above --
	// that goes through sa_embed_arr_var_decl, this goes through
	// buildStructDef's own embedded-array leaf case) needs the leaf's
	// totalSize/maxAlign/hasOwnedStructFields to lay out the array stride.
	// Covers: buildStructDef "[n]$T embedded array, struct leaf" branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_131_incomplete_struct_native_embed_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, sized_arr_param)
{
	// `func f([3]int64 a) -> int64 { ... }` -- a sized-array parameter. []T /
	// []$[m]T (unsized) normalize to pntr via unsizedArrToPntr, but a sized
	// [n]T parameter keeps type-kind "arr", which PlnTypeRegistry::fromJson
	// cannot build. Previously this reached sa_expr_call's parameter-side
	// fromJson call, which was silently swallowed by a
	// `catch (const std::runtime_error&) {}` -- the argument itself would
	// still abort unguarded downstream. IT-2026-09-08: registration now
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
	// normalizeCType. IT-2906: normalizeCFuncSig now tags the registered
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

TEST(sa_error, c_unsupported_anon_typedef_struct)
{
	// `typedef struct { int a; int b; } Pair;` -- an anonymous-body typedef.
	// IT-2905 only taught c2ast/SA to resolve the *tagged* form
	// (`typedef struct Tag X;`); an anonymous body has no tag to alias, so
	// `Pair` stays type-kind "user" at the reference site. Explicitly the
	// case IT-2905 deferred to this ticket.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, parameter side
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_135_c_unsupported_anon_typedef_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'pair_sum'"), string::npos);
	ASSERT_NE(sa.find("'Pair'"), string::npos);
}

TEST(sa_error, c_unsupported_union_param)
{
	// `int use_val(union Val v);` -- c2ast parses union bodies but discards
	// them, emitting a bare {"type-kind":"union"} with no name/fields
	// (CParser.cpp's union branch never calls captureStructTag). No system
	// header in the empirical audit for this ticket produced a bare `union`
	// reference (glibc always typedefs anonymous unions), so this is a
	// hand-written header.
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
	// `void pick(enum Color c);` -- same reasoning as the union case above,
	// "enum" is discarded to a bare {"type-kind":"enum"}. (A *tagged* enum
	// used directly as a top-level return type hits an unrelated c2ast parser
	// gap -- CParser::declaration's enum branch has no backtrack counterpart
	// to the struct/union one -- so this exercises the parameter position;
	// the return-type position is covered by c_unsupported_union_ret below
	// via a type that does parse there.)
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
	// `union Val make_val(void);` -- the ret-type-only path: the callee has
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
	// `void set_cb(int (*cb)(int));` -- a function-pointer parameter is
	// pntr(func(...)); unrepresentableTypeName recurses through the pntr to
	// find the "func" kind underneath, proving the pntr-chain recursion.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, pntr recursion
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
	// struct even after IT-2905 removed the definedStructs_ guard (that guard
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
	// IT-2026-09-11-usual-arith-conv: a call argument used to get no type
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
	// IT-2026-09-11-usual-arith-conv: `p + 1` on a pointer operand used to
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
	// IT-2026-09-11-usual-arith-conv: an assignment (`big -> x`) used to
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

