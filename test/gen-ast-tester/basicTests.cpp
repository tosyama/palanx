#include <gtest/gtest.h>
#include "../test-base/testBase.h"

TEST(gen_ast, basic_tests) {
	string output = execTestCommand("bin/gen-ast ../test/testdata/000_temp.pa");

	ASSERT_EQ(output, "123 1 3 125 3 4 OK");
}
