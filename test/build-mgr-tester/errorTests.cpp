/// build-mgr error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(build_mgr_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan --help");
	ASSERT_NE(out.find("Usage: palan"), string::npos);
}

TEST(build_mgr_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan --version");
	ASSERT_NE(out.find("palan"), string::npos);
}

TEST(build_mgr_error, wrong_number_of_args) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan");
	ASSERT_NE(out.find("wrong number of arguments"), string::npos);
}

TEST(build_mgr_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan no_such_file.pa");
	ASSERT_NE(out.find("could not open file"), string::npos);
}

TEST(build_mgr_error, block_import_scope_out) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_045_block_import_scope.pa");
	ASSERT_NE(out.find("Undefined function"), string::npos);
}

TEST(build_mgr_error, unqualified_alias_call) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_046_unqualified_alias.pa");
	ASSERT_NE(out.find("requires a module alias qualifier"), string::npos);
}

TEST(build_mgr_error, ambiguous_call) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_047_ambiguous_call.pa");
	ASSERT_NE(out.find("Ambiguous function call"), string::npos);
}

TEST(build_mgr_error, unknown_struct_type) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_048_unknown_struct.pa");
	ASSERT_NE(out.find("unknown struct type"), string::npos);
}

TEST(build_mgr_error, unknown_field) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_049_unknown_field.pa");
	ASSERT_NE(out.find("has no field"), string::npos);
}

TEST(build_mgr_error, recursive_embed) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_050_recursive_embed.pa");
	ASSERT_NE(out.find("recursively contains itself"), string::npos);
}

TEST(build_mgr_error, unsupported_field) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_051_unsupported_field.pa");
	ASSERT_NE(out.find("unsupported struct field type"), string::npos);
}

TEST(build_mgr_error, inline_as_value) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_052_inline_as_value.pa");
	ASSERT_NE(out.find("inline struct field cannot be used as a standalone value"), string::npos);
}

TEST(build_mgr_error, cinclude_2d_arr_field) {
	// struct Grid2D { int cells[2][3]; }; -- a cinclude'd struct with a 2D array
	// field. buildStructDef has no layout rule for nested "arr" fields (matches
	// native `[n]$[m]T` struct fields, also unsupported), so IT-2802's
	// isSupportedCFieldType "arr" branch rejects it up front: the whole struct is
	// left unregistered (same graceful "don't register this struct" path as any
	// other cinclude-only construct SA can't lay out), not a hard crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_053_cinclude_2d_arr_field.pa");
	ASSERT_NE(out.find("unknown struct type"), string::npos);
}

TEST(build_mgr_error, cinclude_arr_field_cast_size) {
	// struct CastSized { int a[(int)4]; }; -- a cinclude'd array field whose
	// size-expr resolves to a non-null, non-literal shape ("cast", from c2ast's
	// constant_expression on "(int)4") rather than "lit-int"/"lit-uint".
	// isSupportedCFieldType's "arr" branch rejects any size-expr that isn't a
	// plain literal, so this hits the same graceful skip-the-whole-struct path
	// as cinclude_2d_arr_field above, not a hard crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_054_cinclude_arr_field_cast_size.pa");
	ASSERT_NE(out.find("unknown struct type"), string::npos);
}
