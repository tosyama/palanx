#include <gtest/gtest.h>
#include "../test-base/testBase.h"

TEST(gen_ast, basic_tests) {

//	string output = execTestCommand("bin/gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");

	string output = execTestCommand("bin/gen-ast ../test/testdata/000_temp.pa");

	ASSERT_EQ(output, "OK");
}
