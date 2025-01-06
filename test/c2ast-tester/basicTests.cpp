#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, basic_tests) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -s stdio.h");
    // ASSERT_EQ(output, "");
}