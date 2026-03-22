/// gen-ast error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(gen_ast_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast");
	ASSERT_NE(out.find("no input file"), string::npos);
}

TEST(gen_ast_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-gen-ast no_such_file.pa");
	ASSERT_NE(out.find("Could not open file"), string::npos);
}
