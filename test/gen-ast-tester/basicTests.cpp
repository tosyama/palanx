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
	ASSERT_EQ(jout["ast"]["statements"].size(), 19);
//	cout << output << endl;

	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/100_quicksort.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 6);

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

TEST(gen_ast, var_decl_int32) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/build-mgr/004_int32_widening.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "x") {
				ASSERT_EQ(v["var-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["type-name"], "int32");
				found = true;
			}
		}
	}
	ASSERT_TRUE(found);
}

TEST(gen_ast, cast_node) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/003_cast_syntax.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast") continue;
		ASSERT_EQ(expr["target-type"]["type-name"], "int32");
		ASSERT_EQ(expr["src"]["expr-type"], "id");
		ASSERT_EQ(expr["src"]["name"], "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(gen_ast, cast_compound_expr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/004_cast_compound.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast") continue;
		ASSERT_EQ(expr["target-type"]["type-name"], "uint64");
		ASSERT_EQ(expr["src"]["expr-type"], "add");
		ASSERT_EQ(expr["src"]["left"]["expr-type"],  "id");
		ASSERT_EQ(expr["src"]["right"]["expr-type"], "id");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(gen_ast, func_def) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/005_func_def.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	ASSERT_EQ(jout["ast"]["statements"].size(), 0);
	ASSERT_EQ(jout["ast"]["functions"].size(), 3);

	// add: parameters, ret-type, return stmt
	bool found_add = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "add") continue;
		ASSERT_EQ(f["func-type"], "palan");
		ASSERT_EQ(f["parameters"].size(), 2u);
		ASSERT_EQ(f["parameters"][0]["name"], "a");
		ASSERT_EQ(f["parameters"][0]["var-type"]["type-name"], "int32");
		ASSERT_TRUE(f.contains("ret-type"));
		ASSERT_EQ(f["ret-type"]["type-name"], "int32");
		bool has_return = false;
		for (auto& s : f["body"])
			if (s["stmt-type"] == "return") { has_return = true; }
		ASSERT_TRUE(has_return);
		found_add = true;
	}
	ASSERT_TRUE(found_add);

	// divmod: rets, assign stmt, bare return
	bool found_divmod = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "divmod") continue;
		ASSERT_FALSE(f.contains("ret-type"));
		ASSERT_TRUE(f.contains("rets"));
		ASSERT_EQ(f["rets"].size(), 2u);
		ASSERT_EQ(f["rets"][0]["name"], "q");
		ASSERT_EQ(f["rets"][1]["name"], "r");
		bool has_assign = false;
		for (auto& s : f["body"])
			if (s["stmt-type"] == "assign") {
				ASSERT_EQ(s["name"], "q");
				has_assign = true;
			}
		ASSERT_TRUE(has_assign);
		found_divmod = true;
	}
	ASSERT_TRUE(found_divmod);
}

TEST(gen_ast, block_stmt) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/006_block.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// top-level: statements has 1 block, functions has outer
	ASSERT_EQ(jout["ast"]["statements"].size(), 1);
	ASSERT_EQ(jout["ast"]["functions"].size(), 1);

	// block stmt structure
	const auto& blk = jout["ast"]["statements"][0];
	ASSERT_EQ(blk["stmt-type"], "block");
	ASSERT_EQ(blk["body"].size(), 2);  // var-decl + assign

	// outer function body contains one block
	const auto& outerFunc = jout["ast"]["functions"][0];
	ASSERT_EQ(outerFunc["name"], "outer");
	const auto& outerBody = outerFunc["body"];
	ASSERT_EQ(outerBody.size(), 1);
	ASSERT_EQ(outerBody[0]["stmt-type"], "block");

	// inner func-def is embedded in block body, not in ast["ast"]["functions"]
	const auto& innerBlock = outerBody[0]["body"];
	ASSERT_EQ(innerBlock[0]["stmt-type"], "func-def");
	ASSERT_EQ(innerBlock[0]["name"], "inner");
	ASSERT_EQ(jout["ast"]["functions"].size(), 1);  // outer only
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
