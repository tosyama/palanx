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

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
