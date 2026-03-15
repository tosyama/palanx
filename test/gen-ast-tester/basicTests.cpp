#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

bool checkerr(string &output) {
	if (output[0] != '{') {
		cout << output;
		return false;
	}
	return true;
}

TEST(gen_ast, basic_tests) {
	cleanTestEnv();
	string output;
	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 21);
//	cout << output << endl;
	
	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/100_quicksort.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 8);

	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 12);
}

TEST(gen_ast, cinclude_functions_in_ast) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_printf = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "printf" && f["func-type"] == "c") {
				found_printf = true;
				break;
			}
		}
		if (found_printf) break;
	}
	ASSERT_TRUE(found_printf);
}

TEST(gen_ast, addition) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/build-mgr/003_addition.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] == "add") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					found = true;
				}
			}
		}
	}
	ASSERT_TRUE(found);
}

TEST(gen_ast, cli_tests) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast -h");
	ASSERT_EQ(output.substr(0,7), "Usage: ");

	output = execTestCommand("bin/palan-gen-ast not_exist_file.pa");
	ASSERT_EQ(output, "return1::Could not open file 'not_exist_file.pa'.\n");

	output = execTestCommand("bin/palan-gen-ast -i ../test/testdata/gen-ast/001_basicPattern.pa -o out/001ast.json");
	ASSERT_EQ(output, "");
}
