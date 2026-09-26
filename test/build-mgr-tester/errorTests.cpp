/// build-mgr error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include "../test-base/testBase.h"

using namespace std;
namespace fs = std::filesystem;

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
	// A nested-array struct field (matches native `[n]$[m]T` fields, also
	// unsupported) makes buildStructDef register Grid2D as incomplete rather
	// than leaving the tag unregistered -- a graceful diagnostic, not a crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_053_cinclude_2d_arr_field.pa");
	ASSERT_NE(out.find("struct 'Grid2D' has no known layout"), string::npos);
}

TEST(build_mgr_error, cinclude_arr_field_cast_size) {
	// A non-literal array size-expr ("(int)4.0" resolves to "cast", not
	// "lit-int") hits the same incomplete-struct path as cinclude_2d_arr_field.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_054_cinclude_arr_field_cast_size.pa");
	ASSERT_NE(out.find("struct 'CastSized' has no known layout"), string::npos);
}

TEST(build_mgr_error, c_unsupported_sig) {
	// Regression: `long double` (c2ast's "flt128") on acosl's return type used
	// to abort via an uncaught fromJson throw (rc=134); now diagnosed at the call.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_055_c_unsupported_sig.pa");
	ASSERT_NE(out.find("cannot call C function 'acosl'"), string::npos);
	ASSERT_NE(out.find("'flt128'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_owned_file) {
	// FILE resolves to `struct _IO_FILE`, registered incomplete (glibc's
	// "_unused2" field has an unevaluable size-expr); the diagnostic names the
	// underlying tag, not the alias.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_056_stdio_owned_file.pa");
	ASSERT_NE(out.find("struct '_IO_FILE' has no known layout"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_size_t_narrowing) {
	// fwrite/fread/strlen return size_t; `int64 n = fwrite(...)` crosses
	// signedness and a var-decl initializer refuses it (the loose `-> n` form
	// converts instead). Pins the rule at the C-ABI boundary.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_057_stdio_size_t_narrowing.pa");
	ASSERT_NE(out.find("Implicit conversion from 'uint64' to 'int64'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_incomplete_field) {
	// Same requireCompleteStruct diagnostic as stdio_owned_file above, but
	// reached via field access (`f._flags`) through a `@!FILE` handle.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_058_stdio_incomplete_field.pa");
	ASSERT_NE(out.find("struct '_IO_FILE' has no known layout"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stat_func_macro) {
	// S_ISDIR is a function-like macro; c2ast/gen-ast only export object-like
	// macros, so this must be a clean diagnostic, not an abort.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_059_stat_func_macro.pa");
	ASSERT_NE(out.find("Undefined function 'S_ISDIR'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, arg_narrowing) {
	// Regression: an int64 argument to an int32 parameter used to pass through
	// unchecked, producing a bad `movq` width mismatch at the assembler stage.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_060_arg_narrowing.pa");
	ASSERT_NE(out.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, cinclude_not_found) {
	// Regression: gen-ast used to swallow a failed header read and let palan
	// exit 0 as if the header were empty.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_061_cinclude_not_found.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, missing_lib) {
	// A `link` clause naming a library ld can't find is not diagnosed by the
	// compiler: reproducing ld's search path belongs to ld, not Palan.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_062_missing_lib.pa");
	ASSERT_NE(out.find("cannot find -lnosuchlib"), string::npos);
}

TEST(build_mgr_error, assembler_not_found) {
	// `as` is the only PATH-resolved tool on the happy path (the other stages
	// launch by absolute /proc/self/exe path), so hiding PATH only breaks it.
	cleanTestEnv();
	string out = execTestCommand(
		"env PATH=/nonexistent bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_NE(out.find("failed to execute 'as'"), string::npos);
}

TEST(build_mgr_error, child_killed_by_signal) {
	// palan must exit -1 (255) on PlnSpawnStatus::Signaled, not crash itself.
	// ulimit -c 0 keeps the compiled program's core dump out of build/.
	cleanTestEnv();
	string out = execTestCommand(
		"ulimit -c 0; bin/palan ../test/testdata/build-mgr/error_063_abort.pa");
	ASSERT_NE(out.find("return1:"), string::npos);
}
