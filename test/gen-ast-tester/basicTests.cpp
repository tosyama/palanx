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

	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/100_quicksort.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 6);

	output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 12);
}

TEST(gen_ast, addition) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/build-mgr/003_addition.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_add = false;
	bool found_sub = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] == "add") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					found_add = true;
				}
				if (arg["expr-type"] == "sub") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					found_sub = true;
				}
			}
		}
	}
	ASSERT_TRUE(found_add);
	ASSERT_TRUE(found_sub);
}

TEST(gen_ast, comparison) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/build-mgr/005_comparison.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_lt = false;
	bool found_eq = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] != "cmp") continue;
				ASSERT_EQ(arg["left"]["expr-type"],  "id");
				ASSERT_EQ(arg["right"]["expr-type"], "id");
				if (arg["op"] == "<")  found_lt = true;
				if (arg["op"] == "==") found_eq = true;
			}
		}
	}
	ASSERT_TRUE(found_lt);
	ASSERT_TRUE(found_eq);
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

TEST(gen_ast, cast) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/101_cast.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// cast_node: int32(x) — cast of an identifier
	bool found_simple = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast" || expr["target-type"]["type-name"] != "int32") continue;
		ASSERT_EQ(expr["src"]["expr-type"], "id");
		ASSERT_EQ(expr["src"]["name"], "x");
		found_simple = true;
		break;
	}
	ASSERT_TRUE(found_simple);

	// cast_compound_expr: uint64(y+z) — cast of a compound expression
	bool found_compound = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast" || expr["target-type"]["type-name"] != "uint64") continue;
		ASSERT_EQ(expr["src"]["expr-type"], "add");
		ASSERT_EQ(expr["src"]["left"]["expr-type"],  "id");
		ASSERT_EQ(expr["src"]["right"]["expr-type"], "id");
		found_compound = true;
		break;
	}
	ASSERT_TRUE(found_compound);
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
		for (auto& s : f["block"]["body"])
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
		for (auto& s : f["block"]["body"])
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

	// top-level: statements has 1 block, functions has outer only
	ASSERT_EQ(jout["ast"]["statements"].size(), 1);
	ASSERT_EQ(jout["ast"]["functions"].size(), 1);

	// block stmt structure: top-level block has functions=[] and body=[var-decl, assign]
	const auto& blk = jout["ast"]["statements"][0];
	ASSERT_EQ(blk["stmt-type"], "block");
	ASSERT_EQ(blk["body"].size(), 2);  // var-decl + assign
	ASSERT_EQ(blk["functions"].size(), 0);

	// outer function block contains one block in body
	const auto& outerFunc = jout["ast"]["functions"][0];
	ASSERT_EQ(outerFunc["name"], "outer");
	const auto& outerBody = outerFunc["block"]["body"];
	ASSERT_EQ(outerBody.size(), 1);
	ASSERT_EQ(outerBody[0]["stmt-type"], "block");

	// inner func-def is in block.functions, body has the return stmt
	ASSERT_EQ(outerBody[0]["functions"].size(), 1);
	ASSERT_EQ(outerBody[0]["functions"][0]["name"], "inner");
	ASSERT_EQ(outerBody[0]["body"].size(), 1);
	ASSERT_EQ(outerBody[0]["body"][0]["stmt-type"], "return");
}

TEST(gen_ast, export_func) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan-gen-ast ../test/testdata/gen-ast/007_export_func.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// root "export" array check (SA reads this)
	ASSERT_TRUE(jout.contains("export"));
	ASSERT_EQ(jout["export"].size(), 1u);
	ASSERT_EQ(jout["export"][0]["name"], "add");
	ASSERT_FALSE(jout["export"][0].contains("block"));  // block is excluded

	// ast["ast"]["functions"] export flag check
	bool found_export = false, found_noexport = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] == "add") {
			ASSERT_TRUE(f.value("export", false));
			found_export = true;
		}
		if (f["name"] == "noexport") {
			ASSERT_FALSE(f.value("export", false));
			found_noexport = true;
		}
	}
	ASSERT_TRUE(found_export);
	ASSERT_TRUE(found_noexport);
}

TEST(gen_ast, if_stmt) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/008_if_stmt.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// clamp function: body has 2 stmts (if + return)
	bool found_clamp = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "clamp") continue;
		found_clamp = true;
		const auto& body = f["block"]["body"];
		ASSERT_EQ(body.size(), 2u);
		const auto& if_node = body[0];
		ASSERT_EQ(if_node["stmt-type"], "if");
		ASSERT_EQ(if_node["cond"]["expr-type"], "cmp");
		ASSERT_EQ(if_node["cond"]["op"], "<");
		ASSERT_TRUE(if_node.contains("then"));
		ASSERT_FALSE(if_node.contains("else"));
	}
	ASSERT_TRUE(found_clamp);

	// sign function: if-else
	bool found_sign = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "sign") continue;
		found_sign = true;
		const auto& body = f["block"]["body"];
		ASSERT_EQ(body.size(), 2u);
		const auto& if_node = body[0];
		ASSERT_EQ(if_node["stmt-type"], "if");
		ASSERT_TRUE(if_node.contains("then"));
		ASSERT_TRUE(if_node.contains("else"));
		ASSERT_EQ(if_node["else"]["stmt-type"], "block");
	}
	ASSERT_TRUE(found_sign);
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
