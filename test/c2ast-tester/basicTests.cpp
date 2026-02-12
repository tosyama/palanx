#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, basic_tests) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/000_temp_c_header.h");
    ASSERT_EQ(output, "");
}
