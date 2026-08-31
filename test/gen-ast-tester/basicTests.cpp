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

TEST(gen_ast, unary_minus) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/build-mgr/007_unary_minus.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// -42 in printf args should produce neg expr-type
	bool found_neg = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		for (auto& arg : body["args"]) {
			if (arg["expr-type"] == "neg") {
				ASSERT_TRUE(arg.contains("operand"));
				found_neg = true;
			}
		}
	}
	ASSERT_TRUE(found_neg);
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

TEST(gen_ast, while_loop) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/009_while_loop.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_countdown = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "countDown") continue;
		found_countdown = true;
		const auto& body = f["block"]["body"];
		ASSERT_GE(body.size(), 1u);
		const auto& wh = body[0];
		ASSERT_EQ(wh["stmt-type"], "while");
		ASSERT_EQ(wh["cond"]["expr-type"], "cmp");
		ASSERT_EQ(wh["cond"]["op"], ">");
		ASSERT_TRUE(wh.contains("body"));
		ASSERT_EQ(wh["body"].size(), 1u);
		ASSERT_EQ(wh["body"][0]["stmt-type"], "assign");
	}
	ASSERT_TRUE(found_countdown);
}

TEST(gen_ast, stmt_list_patterns) {
	cleanTestEnv();
	// Covers stmt_list_b/body_list_b grammar rules added for optional semicolon:
	// B;E and B;func_def at top-level, and B B / B;B / B func_def / B;func_def inside blocks.
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/012_stmt_list_patterns.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// top-level: x var-decl and two if stmts plus helper/outer functions
	ASSERT_GE(jout["ast"]["statements"].size(), 1u);
	ASSERT_GE(jout["ast"]["functions"].size(), 2u);  // helper + outer

	// outer function has multiple statements (while/if mix)
	bool found_outer = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "outer") continue;
		found_outer = true;
		ASSERT_GE(f["block"]["body"].size(), 4u);
		// block-local inner funcs declared inside outer
		ASSERT_GE(f["block"]["functions"].size(), 2u);
	}
	ASSERT_TRUE(found_outer);
}

TEST(gen_ast, break_continue) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/011_break_continue.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// while body: if-break, if-continue
	const auto& stmts = jout["ast"]["statements"];
	bool found_break = false;
	bool found_continue = false;
	for (auto& stmt : stmts) {
		if (stmt["stmt-type"] != "while") continue;
		for (auto& s : stmt["body"]) {
			if (s["stmt-type"] != "if") continue;
			for (auto& b : s["then"]["body"]) {
				if (b["stmt-type"] == "break")    found_break    = true;
				if (b["stmt-type"] == "continue") found_continue = true;
			}
		}
	}
	ASSERT_TRUE(found_break);
	ASSERT_TRUE(found_continue);
}

TEST(gen_ast, optional_semicolon) {
	cleanTestEnv();
	// Last statement in a block can omit the semicolon.
	// The AST output should be identical to the version with semicolon.
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/010_optional_semicolon.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// cinclude + 2 var-decls (x and y); y has no trailing ';'
	ASSERT_EQ(jout["ast"]["statements"].size(), 3u);
	const auto& y_decl = jout["ast"]["statements"][2];
	ASSERT_EQ(y_decl["stmt-type"], "var-decl");
	ASSERT_EQ(y_decl["vars"][0]["name"], "y");
	ASSERT_EQ(y_decl["vars"][0]["init"]["expr-type"], "add");
}

TEST(gen_ast, float_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/013_float_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	ASSERT_EQ(jout["ast"]["statements"].size(), 2u);

	// flo64 x = 3.14
	const auto& x_decl = jout["ast"]["statements"][0];
	ASSERT_EQ(x_decl["stmt-type"], "var-decl");
	ASSERT_EQ(x_decl["vars"][0]["name"], "x");
	ASSERT_EQ(x_decl["vars"][0]["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x_decl["vars"][0]["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(x_decl["vars"][0]["init"]["value"], "3.14");

	// flo32 y = 1.5
	const auto& y_decl = jout["ast"]["statements"][1];
	ASSERT_EQ(y_decl["vars"][0]["name"], "y");
	ASSERT_EQ(y_decl["vars"][0]["var-type"]["type-name"], "flo32");
	ASSERT_EQ(y_decl["vars"][0]["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(y_decl["vars"][0]["init"]["value"], "1.5");
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

TEST(gen_ast, array_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/014_array_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	bool found_buf = false, found_arr = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "buf") {
				ASSERT_EQ(v["var-type"]["type-kind"], "arr");
				ASSERT_EQ(v["var-type"]["size-expr"]["expr-type"], "lit-int");
				ASSERT_EQ(v["var-type"]["size-expr"]["value"], "64");
				ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "uint8");
				found_buf = true;
			}
			if (v["name"] == "arr") {
				ASSERT_EQ(v["var-type"]["type-kind"], "arr");
				ASSERT_EQ(v["var-type"]["size-expr"]["expr-type"], "id");
				ASSERT_EQ(v["var-type"]["size-expr"]["name"], "n");
				ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int64");
				found_arr = true;
			}
		}
	}
	ASSERT_TRUE(found_buf);
	ASSERT_TRUE(found_arr);
}

TEST(gen_ast, arr_index_expr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/015_arr_index_expr.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	// statements: var-decl, expr(a[0]), expr(a[i+1])
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	// a[0]: expr stmt with arr-index body
	const auto& s1 = jout["ast"]["statements"][1];
	ASSERT_EQ(s1["stmt-type"], "expr");
	ASSERT_EQ(s1["body"]["expr-type"], "arr-index");
	ASSERT_EQ(s1["body"]["array"]["expr-type"], "id");
	ASSERT_EQ(s1["body"]["array"]["name"], "a");
	ASSERT_EQ(s1["body"]["index"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["body"]["index"]["value"], "0");

	// a[i+1]: arr-index with add expression as index
	const auto& s2 = jout["ast"]["statements"][2];
	ASSERT_EQ(s2["stmt-type"], "expr");
	ASSERT_EQ(s2["body"]["expr-type"], "arr-index");
	ASSERT_EQ(s2["body"]["index"]["expr-type"], "add");
}

TEST(gen_ast, arr_assign_stmt) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/016_arr_assign_stmt.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	// statements: var-decl, arr-assign(1->a[0]), arr-assign(x->a[i])
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	// 1 -> a[0]
	const auto& s1 = jout["ast"]["statements"][1];
	ASSERT_EQ(s1["stmt-type"], "arr-assign");
	ASSERT_EQ(s1["target"]["expr-type"], "arr-index");
	ASSERT_EQ(s1["target"]["array"]["name"], "a");
	ASSERT_EQ(s1["target"]["index"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["target"]["index"]["value"], "0");
	ASSERT_EQ(s1["value"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["value"]["value"], "1");

	// x -> a[i]
	const auto& s2 = jout["ast"]["statements"][2];
	ASSERT_EQ(s2["stmt-type"], "arr-assign");
	ASSERT_EQ(s2["target"]["array"]["name"], "a");
	ASSERT_EQ(s2["target"]["index"]["expr-type"], "id");
	ASSERT_EQ(s2["target"]["index"]["name"], "i");
	ASSERT_EQ(s2["value"]["expr-type"], "id");
	ASSERT_EQ(s2["value"]["name"], "x");
}

TEST(gen_ast, multidim_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/017_multidim_arr.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);  // m-decl, n-decl, mat-decl, arr-assign

	// var-decl: [m][n]int32 mat
	const auto& mat_decl = stmts[2];
	ASSERT_EQ(mat_decl["stmt-type"], "var-decl");
	const auto& vt = mat_decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_FALSE(vt["size-expr"].is_null());
	const auto& inner = vt["base-type"];
	ASSERT_EQ(inner["type-kind"], "arr");
	ASSERT_EQ(inner["specifier"], "raw");
	ASSERT_FALSE(inner["size-expr"].is_null());
	ASSERT_EQ(inner["base-type"]["type-kind"], "prim");
	ASSERT_EQ(inner["base-type"]["type-name"], "int32");

	// arr-assign: int32(1) -> mat[0][1]  (chained store_loc)
	const auto& assign = stmts[3];
	ASSERT_EQ(assign["stmt-type"], "arr-assign");
	const auto& target = assign["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");           // outer index [1]
	ASSERT_EQ(target["array"]["expr-type"], "arr-index");  // inner index [0]
	ASSERT_EQ(target["array"]["array"]["expr-type"], "id");
	ASSERT_EQ(target["array"]["array"]["name"], "mat");
}

TEST(gen_ast, embed_arr_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/019_embed_arr_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);  // rows-decl, mat-decl

	// var-decl: [rows]$[4]int32 mat
	const auto& mat_decl = stmts[1];
	ASSERT_EQ(mat_decl["stmt-type"], "var-decl");
	const auto& vt = mat_decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_FALSE(vt["size-expr"].is_null());
	ASSERT_TRUE(vt.value("embedded", false));
	const auto& inner = vt["base-type"];
	ASSERT_EQ(inner["type-kind"], "arr");
	ASSERT_EQ(inner["specifier"], "raw");
	ASSERT_FALSE(inner["size-expr"].is_null());
	ASSERT_EQ(inner["base-type"]["type-kind"], "prim");
	ASSERT_EQ(inner["base-type"]["type-name"], "int32");
}

TEST(gen_ast, logical_ops) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/018_logical_ops.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);  // 2 var-decls + 4 expr-stmts

	// a && b
	ASSERT_EQ(stmts[2]["stmt-type"], "expr");
	const auto& land = stmts[2]["body"];
	ASSERT_EQ(land["expr-type"], "logical-and");
	ASSERT_EQ(land["left"]["name"],  "a");
	ASSERT_EQ(land["right"]["name"], "b");

	// a || b
	const auto& lor = stmts[3]["body"];
	ASSERT_EQ(lor["expr-type"], "logical-or");
	ASSERT_EQ(lor["left"]["name"],  "a");
	ASSERT_EQ(lor["right"]["name"], "b");

	// !a
	const auto& lnot = stmts[4]["body"];
	ASSERT_EQ(lnot["expr-type"], "logical-not");
	ASSERT_EQ(lnot["operand"]["name"], "a");

	// a && b || !a  →  (a && b) || (!a)  (&& binds tighter than ||)
	const auto& mixed = stmts[5]["body"];
	ASSERT_EQ(mixed["expr-type"], "logical-or");
	ASSERT_EQ(mixed["left"]["expr-type"],  "logical-and");
	ASSERT_EQ(mixed["right"]["expr-type"], "logical-not");
}

TEST(gen_ast, addr_of) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/104_addr_of.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	// @int64 p = @x;
	const auto& p = stmts[2]["vars"][0];
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["mutable"], false);
	const auto& addr_x = p["init"];
	ASSERT_EQ(addr_x["expr-type"], "addr-of");
	ASSERT_EQ(addr_x["name"], "x");
	ASSERT_EQ(addr_x["mutable"], false);
	ASSERT_FALSE(addr_x["loc"].is_null());

	// @!int64 q = @!y;
	const auto& q = stmts[3]["vars"][0];
	ASSERT_EQ(q["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(q["var-type"]["mutable"], true);
	const auto& addr_y = q["init"];
	ASSERT_EQ(addr_y["expr-type"], "addr-of");
	ASSERT_EQ(addr_y["name"], "y");
	ASSERT_EQ(addr_y["mutable"], true);
}

TEST(gen_ast, addr_of_type_alias_mix) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/105_addr_of_type_alias_mix.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	// type IntPtr = @int64;  (type-position '@', must still resolve as a type)
	ASSERT_EQ(stmts[0]["stmt-type"], "type-alias");
	ASSERT_EQ(stmts[0]["type"]["type-kind"], "pntr");

	// @z;
	ASSERT_EQ(stmts[2]["body"]["expr-type"], "addr-of");
	ASSERT_EQ(stmts[2]["body"]["name"], "z");

	// @!z;
	ASSERT_EQ(stmts[3]["body"]["expr-type"], "addr-of");
	ASSERT_EQ(stmts[3]["body"]["mutable"], true);
}

TEST(gen_ast, uint_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/020_uint_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	const auto& decl = stmts[0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& var = decl["vars"][0];
	ASSERT_EQ(var["var-type"]["type-kind"], "prim");
	ASSERT_EQ(var["var-type"]["type-name"], "uint64");

	const auto& init = var["init"];
	ASSERT_EQ(init["expr-type"], "lit-uint");
	ASSERT_EQ(init["value"], "18446744073709551615");
}

TEST(gen_ast, member_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/021_member_call.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// M.f(1, 2) → member-call
	const auto& mc = stmts[0]["body"];
	ASSERT_EQ(mc["expr-type"], "member-call");
	ASSERT_EQ(mc["object"]["expr-type"], "id");
	ASSERT_EQ(mc["object"]["name"], "M");
	ASSERT_EQ(mc["method"], "f");
	ASSERT_EQ(mc["args"].size(), 2);

	// f(1, 2) → call (unchanged)
	const auto& c = stmts[1]["body"];
	ASSERT_EQ(c["expr-type"], "call");
	ASSERT_EQ(c["name"], "f");
	ASSERT_EQ(c["args"].size(), 2);
}

TEST(gen_ast, struct_def) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/022_struct_def.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Point { int64 x; int64 y; } → struct-def
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Point");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "x");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-name"], "int64");
	ASSERT_EQ(s["fields"][1]["name"], "y");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-name"], "int64");
}

TEST(gen_ast, type_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/034_type_alias.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type MyInt = int64; → type-alias
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "type-alias");
	ASSERT_EQ(s["name"], "MyInt");
	ASSERT_EQ(s["type"]["type-kind"], "prim");
	ASSERT_EQ(s["type"]["type-name"], "int64");
}

TEST(gen_ast, const_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/102_const_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// const MAX = 100; → const-decl
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "const-decl");
	ASSERT_EQ(s["name"], "MAX");
	ASSERT_EQ(s["value"]["expr-type"], "lit-int");
	ASSERT_EQ(s["value"]["value"], "100");
}

TEST(gen_ast, field_access) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/023_field_access.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// p.x → field-access expr
	const auto& fa = stmts[0]["body"];
	ASSERT_EQ(fa["expr-type"], "field-access");
	ASSERT_EQ(fa["object"]["expr-type"], "id");
	ASSERT_EQ(fa["object"]["name"], "p");
	ASSERT_EQ(fa["field"], "x");

	// 10 -> p.x → field-assign
	const auto& as = stmts[1];
	ASSERT_EQ(as["stmt-type"], "field-assign");
	ASSERT_EQ(as["object"]["kind"], "var");
	ASSERT_EQ(as["object"]["name"], "p");
	ASSERT_EQ(as["field"], "x");
	ASSERT_EQ(as["value"]["expr-type"], "lit-int");
	ASSERT_EQ(as["value"]["value"], "10");
}

TEST(gen_ast, embed_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/024_embed_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// type Point { int64 x; int64 y; } — first struct parses correctly
	ASSERT_EQ(stmts[0]["name"], "Point");
	ASSERT_EQ(stmts[0]["fields"].size(), 2);

	// type Line { $Point a; $Point b; }
	const auto& s = stmts[1];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Line");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "a");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(s["fields"][0]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(s["fields"][1]["name"], "b");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, owned_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/025_owned_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// type Point { int64 x; int64 y; } — first struct parses correctly
	ASSERT_EQ(stmts[0]["name"], "Point");
	ASSERT_EQ(stmts[0]["fields"].size(), 2);

	// type Rect { Point tl; Point br; }
	const auto& s = stmts[1];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Rect");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "tl");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-name"], "Point");
	ASSERT_EQ(s["fields"][1]["name"], "br");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-name"], "Point");
}

TEST(gen_ast, ptr_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/026_ptr_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Node { int64 val; @Node next; }
	const auto& s = stmts[0];
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][1]["name"], "next");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "pntr");
	ASSERT_FALSE(s["fields"][1]["var-type"].value("mutable", false));
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Node");
}

TEST(gen_ast, mutable_ptr_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/027_mutable_ptr_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Node { int64 val; @!Node next; }
	const auto& s = stmts[0];
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][1]["name"], "next");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(s["fields"][1]["var-type"]["mutable"], true);
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Node");
}

TEST(gen_ast, write_field_arr_index) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/031_write_field_arr_index.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// 10 -> s.f[0]; → arr-assign, target.array is a field-access node (not a crash)
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "arr-assign");
	const auto& target = s["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["array"]["expr-type"], "field-access");
	ASSERT_EQ(target["array"]["field"], "f");
	ASSERT_EQ(target["array"]["object"]["expr-type"], "id");
	ASSERT_EQ(target["array"]["object"]["name"], "s");
	ASSERT_EQ(target["index"]["value"], "0");
}

TEST(gen_ast, write_nested_field_arr_index) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/032_write_nested_field_arr_index.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// 10 -> s.f[0].sub; → field-assign, object is an arr-index (kind-tagged) node
	// whose array is a field-access node
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "field-assign");
	ASSERT_EQ(s["field"], "sub");
	const auto& obj = s["object"];
	ASSERT_EQ(obj["kind"], "arr-index");
	ASSERT_EQ(obj["array"]["expr-type"], "field-access");
	ASSERT_EQ(obj["array"]["field"], "f");
	ASSERT_EQ(obj["array"]["object"]["name"], "s");
}

TEST(gen_ast, write_arr_index_regression) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/033_write_arr_index_regression.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 3);

	// 10 -> pts[0]; → var-based arr-index, unchanged shape
	ASSERT_EQ(stmts[0]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[0]["target"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[0]["target"]["array"]["name"], "pts");

	// 20 -> pts[0].x; → field-assign over an arr-index object, array unchanged
	ASSERT_EQ(stmts[1]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[1]["object"]["kind"], "arr-index");
	ASSERT_EQ(stmts[1]["object"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[1]["object"]["array"]["name"], "pts");

	// 30 -> mat[0][1]; → nested arr-index (2D), unchanged shape
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[2]["target"]["array"]["expr-type"], "arr-index");
	ASSERT_EQ(stmts[2]["target"]["array"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[2]["target"]["array"]["array"]["name"], "mat");
}

TEST(gen_ast, arr_at_struct) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/028_arr_at_struct.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// [4]@Point rpts;
	const auto& decl = stmts[1];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& vt = decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_EQ(vt["size-expr"]["value"], "4");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_FALSE(vt["base-type"].value("mutable", false));
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, arr_at_bang_struct) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/029_arr_at_bang_struct.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// [4]@!Point wpts;
	const auto& decl = stmts[1];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& vt = decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_EQ(vt["size-expr"]["value"], "4");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["mutable"], true);
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, func_arr_at_struct_param) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/030_func_arr_at_struct_param.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& funcs = jout["ast"]["functions"];
	ASSERT_EQ(funcs.size(), 1);

	// func f([]@!Point pts, int64 n) { }
	const auto& params = funcs[0]["parameters"];
	ASSERT_EQ(params.size(), 2);
	const auto& vt = params[0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_TRUE(vt["size-expr"].is_null());
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["mutable"], true);
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, cinclude_local_header) {
	cleanTestEnv();
	// bin/palan-gen-ast runs from build/, while the .pa and .h fixtures live under
	// test/testdata/gen-ast/ - this only passes if the quoted local header path is
	// resolved relative to the source file, not the process cwd.
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/035_cinclude_local_header.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_add_one = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "add_one" && f["func-type"] == "c") {
				found_add_one = true;
				break;
			}
		}
		if (found_add_one) break;
	}
	ASSERT_TRUE(found_add_one);
}

TEST(gen_ast, cinclude_constant) {
	cleanTestEnv();
	// IT-2608: c2ast exports object-like macro constants into ast.constants (IT-2607),
	// but PlnParser.yy's cinclude rule only copied ast.functions into the cinclude stmt.
	// This verifies the constants array is now copied through as well.
	string output = execTestCommand("bin/palan-gen-ast ../test/testdata/gen-ast/103_cinclude_constant.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_answer = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		ASSERT_TRUE(stmt.contains("constants"));
		for (auto& c : stmt["constants"]) {
			if (c["name"] == "ANSWER") {
				ASSERT_EQ(c["value"], "42");
				ASSERT_EQ(c["value-type"]["type-kind"], "prim");
				ASSERT_EQ(c["value-type"]["type-name"], "int32");
				found_answer = true;
			}
		}
	}
	ASSERT_TRUE(found_answer);
}
