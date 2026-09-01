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
	// Covers: resolveStoreLocChain arr-index base case, mutable:false branch (E_WriteToReadOnlyArrElem)
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
	// Covers: resolveStoreLocChain arr-index base case, non-struct value-type branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_079_field_assign_on_arr_index_non_struct.pa -o " + ast_out), "");
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
	// Covers: IT-2508 — resolveStoreLocChain arr-index base case, mutable:false
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
	// Covers: IT-2508 — resolveStoreLocChain arr-index base case, non-struct
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

TEST(sa_error, addr_of_arr_index_not_addressable)
{
	// `@arr[0];` -- array-element address-of is IT-2807's scope, not this
	// ticket's; the grammar accepts it (store_loc covers arr-index too) but
	// SA must reject it explicitly rather than silently mis-lowering it.
	// Covers: sa_expr_addr_of fallback branch -> E_AddrOfNotAddressable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_109_addr_of_arr_index.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this expression"), string::npos);
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
	// Covers: resolveStoreLocChain kind=="var" isWritableThrough branch
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
	// Covers: resolveStoreLocChain arr-index branch (E_WriteToReadOnlyArrElem)
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
	// `@!Foo p; p[0].bar;` where `Foo` is never declared. gen-ast has no
	// symbol table, so `Foo` parses as a prim base-type, not a struct one --
	// IT-2805: sa_expr_arr_index's generic (non-struct) branch must reject
	// this at the SA boundary instead of letting elemSizeBytes' -1
	// "unknown type" sentinel leak into elem-size and crash palan-codegen
	// downstream (layer violation).
	// Covers: sa_expr_arr_index generic branch, sz<0 guard (E_UnknownStructType)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_104_deref_unknown_struct_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown struct type"), string::npos);
}

