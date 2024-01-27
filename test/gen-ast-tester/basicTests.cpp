#include <gtest/gtest.h>
#include "../test-base/testBase.h"

TEST(gen_ast, basic_tests) {
	string output = execTestCommand("bin/gen-ast abc");

	ASSERT_EQ(output, "abc\nOK");
}
