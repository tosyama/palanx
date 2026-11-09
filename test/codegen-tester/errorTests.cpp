/// codegen error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(codegen_error, help) {
	cleanTestEnv();
	string out = execTestCommand("palan-codegen --help");
	ASSERT_NE(out.find("Usage: palan-codegen"), string::npos);
}

TEST(codegen_error, version) {
	cleanTestEnv();
	string out = execTestCommand("palan-codegen --version");
	ASSERT_NE(out.find("palan-codegen"), string::npos);
}

TEST(codegen_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("palan-codegen");
	ASSERT_NE(out.find("Usage: palan-codegen"), string::npos);
}

TEST(codegen_error, sa_target) {
	cleanTestEnv();
	string out = execTestCommand("palan-codegen ../test/testdata/codegen/error_001_unsupported_target.sa.json -o out/x.s");
	ASSERT_NE(out.find("palan-codegen: unsupported target 'aarch64-linux-gnu'"), string::npos);
	out = execTestCommand("palan-codegen ../test/testdata/codegen/error_002_no_target.sa.json -o out/x.s");
	ASSERT_NE(out.find("palan-codegen: unsupported target ''"), string::npos);
}

TEST(codegen_error, entry_target) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("palan-codegen --entry m..main -o out/e.s --target x86_64-linux-gnu"), "");
	string out = execTestCommand("palan-codegen --entry m..main -o out/e.s --target aarch64-linux-gnu");
	ASSERT_NE(out.find("palan-codegen: unsupported target 'aarch64-linux-gnu'"), string::npos);
}

TEST(codegen_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("palan-codegen no_such_file.sa.json");
	ASSERT_NE(out.find("Could not open file"), string::npos);
}
