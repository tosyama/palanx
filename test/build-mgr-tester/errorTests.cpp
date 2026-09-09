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
	// native `[n]$[m]T` struct fields, also unsupported), so isSupportedCFieldType's
	// "arr" branch rejects it up front: since IT-2904 this registers Grid2D as an
	// incomplete struct (opaque handle, tag known but no layout) rather than
	// leaving the tag unregistered, so `Grid2D g;` (an owned declaration, which
	// needs the layout) is now a graceful E_IncompleteStructType diagnostic
	// instead of "unknown struct type", not a hard crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_053_cinclude_2d_arr_field.pa");
	ASSERT_NE(out.find("struct 'Grid2D' has no known layout"), string::npos);
}

TEST(build_mgr_error, cinclude_arr_field_cast_size) {
	// struct CastSized { int a[(int)4]; }; -- a cinclude'd array field whose
	// size-expr resolves to a non-null, non-literal shape ("cast", from c2ast's
	// constant_expression on "(int)4") rather than "lit-int"/"lit-uint".
	// isSupportedCFieldType's "arr" branch rejects any size-expr that isn't a
	// plain literal, so this hits the same incomplete-struct registration path as
	// cinclude_2d_arr_field above (see IT-2904), not a hard crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_054_cinclude_arr_field_cast_size.pa");
	ASSERT_NE(out.find("struct 'CastSized' has no known layout"), string::npos);
}

TEST(build_mgr_error, c_unsupported_sig) {
	// `long double` (c2ast's "flt128" prim type-name, unknown to Palan) used
	// as acosl's return type. IT-2906: previously an uncaught fromJson throw
	// aborted the process (rc=134, WIFEXITED false -- "return0:" prefix from
	// execTestCommand); now the signature is diagnosed at the call and the
	// process exits normally with rc=1 ("return1:" prefix).
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_055_c_unsupported_sig.pa");
	ASSERT_NE(out.find("cannot call C function 'acosl'"), string::npos);
	ASSERT_NE(out.find("'flt128'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_owned_file) {
	// IT-2026-09-06-2909: `FILE f;` -- an owned declaration of the real glibc
	// FILE. IT-2905 resolves the alias down to `struct _IO_FILE`, which IT-2904
	// registered as incomplete (glibc's "_unused2" field has a size-expr c2ast
	// cannot evaluate), so this is a clean E_IncompleteStructType naming the
	// underlying tag rather than the alias. sa-tester covers this rule on a
	// synthetic struct; this pins it on the real system header, through the
	// typedef, all the way out to the driver.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_056_stdio_owned_file.pa");
	ASSERT_NE(out.find("struct '_IO_FILE' has no known layout"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_size_t_narrowing) {
	// IT-2026-09-06-2909: the first thing anyone writing stdio code hits.
	// fwrite/fread/strlen return size_t; `int64 n = fwrite(...)` crosses
	// signedness and a variable-declaration initializer refuses that (unlike a
	// call argument, which gets no type check at all, and unlike the loose
	// `fwrite(...) -> n` form, which does convert). Pins the rule at the C-ABI
	// boundary, where sa-tester only covers it on native expressions.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_057_stdio_size_t_narrowing.pa");
	ASSERT_NE(out.find("Implicit conversion from 'uint64' to 'int64'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_incomplete_field) {
	// IT-2026-09-06-2909: `f._flags` -- field access on an incomplete struct
	// reached through a real `@!FILE` handle. Same requireCompleteStruct
	// diagnostic as the owned-declaration case above; this pins it on the
	// field-access path instead of the declaration path.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_058_stdio_incomplete_field.pa");
	ASSERT_NE(out.find("struct '_IO_FILE' has no known layout"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}
