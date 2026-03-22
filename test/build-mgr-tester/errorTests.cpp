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
