#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(build_mgr, basic_tests) {
	cleanTestEnv();
	string output;
	output = execTestCommand("bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");

	output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");

}

