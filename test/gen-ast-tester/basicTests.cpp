#include <gtest/gtest.h>
#include "../test-base/testBase.h"

TEST(gen_ast, basic_tests) {
	string output = execTestCommand("bin/gen_ast");

	ASSERT_EQ(output, "OK");
}
