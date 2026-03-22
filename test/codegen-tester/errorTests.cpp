/// codegen error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(codegen_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-codegen --help");
	ASSERT_NE(out.find("Usage: palan-codegen"), string::npos);
}

TEST(codegen_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-codegen --version");
	ASSERT_NE(out.find("palan-codegen"), string::npos);
}

TEST(codegen_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-codegen");
	ASSERT_NE(out.find("Usage: palan-codegen"), string::npos);
}

TEST(codegen_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan-codegen no_such_file.sa.json");
	ASSERT_NE(out.find("Could not open file"), string::npos);
}
