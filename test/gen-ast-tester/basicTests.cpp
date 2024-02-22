#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

bool checkerr(json& ast) {
	if (ast["err"].is_array()) {
		cout << ast["err"][0]["loc"] << endl;
		cout << ast["err"][0]["message"] << endl;
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

TEST(gen_ast, cli_tests) {
	string output = execTestCommand("bin/gen-ast -h");
	ASSERT_EQ(output.substr(0,7), "Usage: ");

	output = execTestCommand("bin/gen-ast not_exist_file.pa");
	ASSERT_EQ(output, "return1:Could not open file 'not_exist_file.pa'.\n:");

}
