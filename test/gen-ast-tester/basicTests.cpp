#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

bool checkerr(json& ast) {
	if (ast["err"].is_array()) {
		std::cout << ast["err"][0]["loc"] << std::endl;
		std::cout << ast["err"][0]["message"] << std::endl;
		return false;
	}
	return true;
}

TEST(gen_ast, basic_tests) {
	string output;
//	output = execTestCommand("bin/gen-ast ../test/testdata/000_temp.pa");
	output = execTestCommand("bin/gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");
	json jout = json::parse(output);
	ASSERT_TRUE(checkerr(jout));
	ASSERT_EQ(jout["ast"]["statements"].size(), 19);
	
	output = execTestCommand("bin/gen-ast ../test/testdata/gen-ast/100_quicksort.pa");
	jout = json::parse(output);
	ASSERT_TRUE(checkerr(jout));
	ASSERT_EQ(jout["ast"]["statements"].size(), 8);

	output = execTestCommand("bin/gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	jout = json::parse(output);
	ASSERT_TRUE(checkerr(jout));
	ASSERT_EQ(jout["ast"]["statements"].size(), 12);
}
