#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(build_mgr, helloworld) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "Hello World!\n");
}

TEST(build_mgr, var_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/002_var_decl.pa");
	ASSERT_EQ(output, "10\n");
}

TEST(build_mgr, addition) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/003_addition.pa");
	ASSERT_EQ(output, "30\n");
}

TEST(build_mgr, int32_widening) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/004_int32_widening.pa");
	ASSERT_EQ(output, "10\n");
}

TEST(build_mgr, variadic_promotion) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/006_variadic_promotion.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, cast_narrowing_int16_int8) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/007_cast_narrowing_int16_int8.pa");
	ASSERT_EQ(output, "100 100 50 50\n");
}

TEST(build_mgr, cast_explicit) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/005_cast_explicit.pa");
	ASSERT_EQ(output, "100\n");
}

TEST(build_mgr, user_func_simple) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/008_user_func_simple.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, assign_stmt) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/009_assign.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, named_ret_narrowing) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/010_named_ret_narrowing.pa");
	ASSERT_EQ(output, "15\n");
}

TEST(build_mgr, named_ret_double_assign) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/011_named_ret_double_assign.pa");
	ASSERT_EQ(output, "16\n");
}

TEST(build_mgr, return_narrowing) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/012_return_narrowing.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, multiret) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/013_multiret.pa");
	ASSERT_EQ(output, "3 5\n");
}

TEST(build_mgr, type_inherit_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/014_type_inherit_decl.pa");
	ASSERT_EQ(output, "5 10\n");
}

TEST(build_mgr, type_inherit_tapple) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/015_type_inherit_tapple.pa");
	ASSERT_EQ(output, "3 5\n");
}

TEST(build_mgr, multiret_implicit_return) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/017_multiret_implicit_return.pa");
	ASSERT_EQ(output, "3 5\n");
}

TEST(build_mgr, palan_call_as_stmt) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/016_palan_call_as_stmt.pa");
	ASSERT_EQ(output, "hello\n");
}

TEST(build_mgr, block_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/018_block_basic.pa");
	ASSERT_EQ(output, "10 20\n10\n");
}

TEST(build_mgr, block_in_func) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/019_block_in_func.pa");
	ASSERT_EQ(output, "6\n");
}

TEST(build_mgr, block_cinclude) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/020_block_cinclude.pa");
	ASSERT_EQ(output, "hello from block\n");
}

TEST(build_mgr, block_func_def) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/021_block_func_def.pa");
	ASSERT_EQ(output, "14\n");
}

TEST(build_mgr, block_nested) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/022_block_nested.pa");
	ASSERT_EQ(output, "1 2 3\n1 2\n1\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
