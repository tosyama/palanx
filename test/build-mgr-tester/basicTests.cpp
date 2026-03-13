#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(build_mgr, helloworld) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");

	output = execTestCommand("./a.out");
	ASSERT_EQ(output, "Hello World!\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
