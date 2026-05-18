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

