#include <gtest/gtest.h>
#include <fstream>
#include <algorithm>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

static json run_sa(const string& pa_file)
{
	string ast_out = "out/test.ast.json";
	string sa_out = "out/test.sa.json";

	string gen = execTestCommand("bin/palan-gen-ast " + pa_file + " -o " + ast_out);
	if (gen != "") return json{};

	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o " + sa_out);
	if (sa != "") return json{};

	ifstream f(sa_out);
	if (!f.is_open()) return json{};
	return json::parse(f);
}

TEST(sa, helloworld_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/001_helloworld.pa");

	ASSERT_TRUE(jout.is_object());

	// printf call is annotated as C function
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] == "call" && expr["name"] == "printf") {
			ASSERT_EQ(expr["func-type"], "c");
			found = true;
			break;
		}
	}
	ASSERT_TRUE(found);

	// string literals are collected
	ASSERT_TRUE(jout.contains("str-literals"));
	auto& lits = jout["str-literals"];
	ASSERT_EQ(lits.size(), 1u);
	ASSERT_EQ(lits[0]["value"], "Hello World!\n");
	ASSERT_EQ(lits[0]["label"], ".str0");

	// cinclude nodes are not emitted to SA output
	for (auto& stmt : jout["statements"])
		ASSERT_NE(stmt["stmt-type"], "cinclude");
}

TEST(sa, var_decl_emitted) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/002_var_decl.pa");

	ASSERT_TRUE(jout.is_object());
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "x" && v["var-type"]["type-name"] == "int64") {
				ASSERT_EQ(v["init"]["expr-type"], "lit-int");
				ASSERT_EQ(v["init"]["value"],     "10");
				found = true;
			}
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, addition_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/003_addition.pa");

	ASSERT_TRUE(jout.is_object());

	bool found_add = false;
	bool found_sub = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] == "add") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					ASSERT_EQ(arg["value-type"]["type-name"],          "int64");
					ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
					ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
					found_add = true;
				}
				if (arg["expr-type"] == "sub") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					ASSERT_EQ(arg["value-type"]["type-name"],          "int64");
					ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
					ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
					found_sub = true;
				}
			}
		}
	}
	ASSERT_TRUE(found_add);
	ASSERT_TRUE(found_sub);
}

TEST(sa, comparison_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/005_comparison.pa");

	ASSERT_TRUE(jout.is_object());

	bool found_lt = false;
	bool found_eq = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] != "cmp") continue;
				ASSERT_EQ(arg["value-type"]["type-name"], "int32");
				ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
				ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
				if (arg["op"] == "<")  found_lt = true;
				if (arg["op"] == "==") found_eq = true;
			}
		}
	}
	ASSERT_TRUE(found_lt);
	ASSERT_TRUE(found_eq);
}

TEST(sa, convert_widening_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/001_convert_widening.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] != "y") continue;
			auto& init = v["init"];
			// convert_node_widening: convert node structure
			ASSERT_EQ(init["expr-type"],               "convert");
			ASSERT_EQ(init["from-type"]["type-name"],  "int32");
			ASSERT_EQ(init["value-type"]["type-name"], "int64");
			ASSERT_EQ(init["src"]["expr-type"],        "id");
			ASSERT_EQ(init["src"]["name"],             "x");
			// int32_id_value_type: src id carries int32 value-type
			ASSERT_EQ(init["src"]["value-type"]["type-kind"], "prim");
			ASSERT_EQ(init["src"]["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, add_mixed_types_wraps_narrower_in_convert) {
	cleanTestEnv();
	// int32 a + int64 b → the int32 operand must be wrapped in a convert node
	json jout = run_sa("../test/testdata/sa/002_add_mixed_types.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		for (auto& arg : body["args"]) {
			if (arg["expr-type"] != "add") continue;
			// left operand (int32 a) must be wrapped in convert → int64
			ASSERT_EQ(arg["left"]["expr-type"],               "convert");
			ASSERT_EQ(arg["left"]["from-type"]["type-name"],  "int32");
			ASSERT_EQ(arg["left"]["value-type"]["type-name"], "int64");
			ASSERT_EQ(arg["left"]["src"]["expr-type"],        "id");
			ASSERT_EQ(arg["left"]["src"]["name"],             "a");
			// right operand (int64 b) needs no convert
			ASSERT_EQ(arg["right"]["expr-type"],              "id");
			ASSERT_EQ(arg["right"]["value-type"]["type-name"],"int64");
			ASSERT_EQ(arg["value-type"]["type-name"],         "int64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, lit_int_default_value_type) {
	cleanTestEnv();
	// lit-int used directly as a function arg (no expected type context) → value-type: int64
	json jout = run_sa("../test/testdata/sa/003_lit_int_printf.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		for (auto& arg : body["args"]) {
			if (arg["expr-type"] != "lit-int") continue;
			ASSERT_EQ(arg["value-type"]["type-kind"], "prim");
			ASSERT_EQ(arg["value-type"]["type-name"], "int64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, variadic_arg_int16_promoted_to_int32) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/004_variadic_promotion.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		// args[1] should be a convert node wrapping the id
		auto& arg1 = call["args"][1];
		ASSERT_EQ(arg1["expr-type"], "convert");
		ASSERT_EQ(arg1["value-type"]["type-name"], "int32");
		ASSERT_EQ(arg1["src"]["expr-type"], "id");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, c_func_return_type_as_value_type) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/005_c_func_return_type.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& var : stmt["vars"]) {
			if (var["name"] != "x") continue;
			// init should be a convert node (int32 -> int64) wrapping the call
			auto& init = var["init"];
			ASSERT_EQ(init["expr-type"], "convert");
			ASSERT_EQ(init["value-type"]["type-name"], "int64");
			ASSERT_EQ(init["src"]["expr-type"], "call");
			ASSERT_EQ(init["src"]["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_explicit_emits_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/006_cast_explicit.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		// args[1] is int32(x) → convert node
		auto& arg = call["args"][1];
		ASSERT_EQ(arg["expr-type"],              "convert");
		ASSERT_EQ(arg["from-type"]["type-name"],  "int64");
		ASSERT_EQ(arg["value-type"]["type-name"], "int32");
		ASSERT_EQ(arg["src"]["expr-type"],        "id");
		ASSERT_EQ(arg["src"]["name"],             "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_identical_returns_src) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/007_cast_identical.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		// cast disappears; id node comes through directly
		ASSERT_EQ(expr["expr-type"], "id");
		ASSERT_EQ(expr["name"],      "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_signed_to_unsigned_emits_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/008_cast_signed_unsigned.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		auto& arg = call["args"][1];
		ASSERT_EQ(arg["expr-type"],              "convert");
		ASSERT_EQ(arg["from-type"]["type-name"],  "int32");
		ASSERT_EQ(arg["value-type"]["type-name"], "uint32");
		ASSERT_EQ(arg["src"]["expr-type"],        "id");
		ASSERT_EQ(arg["src"]["name"],             "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, func_defs) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/101_func_def.pa");
	ASSERT_TRUE(jout.is_object());

	auto findFunc = [&](const string& name) -> const json* {
		for (auto& f : jout["functions"])
			if (f["name"] == name) return &f;
		return nullptr;
	};

	// func_registered_in_sa (009): add function signature
	const json* addFunc = findFunc("add");
	ASSERT_NE(addFunc, nullptr);
	ASSERT_EQ((*addFunc)["parameters"].size(), 2u);
	ASSERT_EQ((*addFunc)["parameters"][0]["name"], "a");
	ASSERT_EQ((*addFunc)["parameters"][0]["var-type"]["type-name"], "int32");
	ASSERT_EQ((*addFunc)["ret-type"]["type-name"], "int32");

	// func_params_accessible (010): identity returns its int64 param x
	const json* identFunc = findFunc("identity");
	ASSERT_NE(identFunc, nullptr);
	auto& identRet = (*identFunc)["body"][0];
	ASSERT_EQ(identRet["stmt-type"], "return");
	ASSERT_EQ(identRet["values"][0]["expr-type"], "id");
	ASSERT_EQ(identRet["values"][0]["name"], "x");
	ASSERT_EQ(identRet["values"][0]["value-type"]["type-name"], "int64");

	// func_assign_emitted (011): divmod contains assign stmt for q
	const json* divmodFunc = findFunc("divmod");
	ASSERT_NE(divmodFunc, nullptr);
	bool found_assign = false;
	for (auto& stmt : (*divmodFunc)["body"]) {
		if (stmt["stmt-type"] != "assign") continue;
		ASSERT_EQ(stmt["name"], "q");
		ASSERT_EQ(stmt["value"]["expr-type"], "id");
		ASSERT_EQ(stmt["value"]["name"], "a");
		found_assign = true;
	}
	ASSERT_TRUE(found_assign);

	// func_return_widening (012): widen returns int32 x as int64 via convert
	const json* widenFunc = findFunc("widen");
	ASSERT_NE(widenFunc, nullptr);
	auto& widenRet = (*widenFunc)["body"][0];
	ASSERT_EQ(widenRet["stmt-type"], "return");
	auto& widenVal = widenRet["values"][0];
	ASSERT_EQ(widenVal["expr-type"],               "convert");
	ASSERT_EQ(widenVal["from-type"]["type-name"],  "int32");
	ASSERT_EQ(widenVal["value-type"]["type-name"], "int64");
	ASSERT_EQ(widenVal["src"]["name"],             "x");

	// func_multiret_tapple (014): tapple-decl statement for sumsOf
	bool found_tapple = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "tapple-decl") continue;
		auto& val = stmt["value"];
		ASSERT_EQ(val["func-type"], "palan");
		ASSERT_EQ(val["value-types"].size(), 2u);
		ASSERT_EQ(stmt["vars"].size(), 2u);
		found_tapple = true;
	}
	ASSERT_TRUE(found_tapple);

	// func_recursive (015): recurse calls itself (palan call)
	const json* recurseFunc = findFunc("recurse");
	ASSERT_NE(recurseFunc, nullptr);
	bool found_recurse = false;
	for (auto& stmt : (*recurseFunc)["body"]) {
		if (stmt["stmt-type"] != "return") continue;
		auto& val = stmt["values"][0];
		ASSERT_EQ(val["expr-type"], "call");
		ASSERT_EQ(val["name"],      "recurse");
		ASSERT_EQ(val["func-type"], "palan");
		found_recurse = true;
	}
	ASSERT_TRUE(found_recurse);
}

TEST(sa, palan_call_resolved) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/013_palan_call.pa");

	ASSERT_TRUE(jout.is_object());

	// add(1,2) is passed as an arg to printf — look inside printf's args
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& outer = stmt["body"];
		if (!outer.contains("args")) continue;
		for (auto& arg : outer["args"]) {
			if (arg["expr-type"] != "call" || arg["name"] != "add") continue;
			ASSERT_EQ(arg["func-type"], "palan");
			ASSERT_EQ(arg["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, block_scope) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/016_block_scope.pa");

	// block statement emitted in sa["statements"]
	bool found = false;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "block") { found = true; break; }
	ASSERT_TRUE(found);

	// block body has var-decl for y and expr for printf
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		bool has_decl = false, has_expr = false;
		for (auto& bs : s["body"]) {
			if (bs["stmt-type"] == "var-decl") has_decl = true;
			if (bs["stmt-type"] == "expr")     has_expr = true;
		}
		ASSERT_TRUE(has_decl);
		ASSERT_TRUE(has_expr);
	}
}

TEST(sa, block_cinclude_scope) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/020_block_cinclude_scope.pa");
	ASSERT_TRUE(jout.is_object());
	// block is emitted with body
	ASSERT_EQ(jout["statements"].size(), 1u);
	ASSERT_EQ(jout["statements"][0]["stmt-type"], "block");
	// printf inside block resolved as C function
	ASSERT_EQ(jout["statements"][0]["body"][0]["stmt-type"], "expr");
	ASSERT_EQ(jout["statements"][0]["body"][0]["body"]["func-type"], "c");
}

TEST(sa, if_stmt_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/036_if_stmt.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_if = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "if") continue;
		found_if = true;
		// cond is a cmp expression with value-type int32
		ASSERT_EQ(stmt["cond"]["expr-type"], "cmp");
		ASSERT_EQ(stmt["cond"]["value-type"]["type-name"], "int32");
		// then block contains a printf call
		ASSERT_EQ(stmt["then"]["stmt-type"], "block");
		ASSERT_FALSE(stmt.contains("else"));
	}
	ASSERT_TRUE(found_if);
}

TEST(sa, if_else_stmt_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/037_if_else_stmt.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_if = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "if") continue;
		found_if = true;
		ASSERT_EQ(stmt["then"]["stmt-type"], "block");
		ASSERT_TRUE(stmt.contains("else"));
		ASSERT_EQ(stmt["else"]["stmt-type"], "block");
	}
	ASSERT_TRUE(found_if);
}

TEST(sa, block_func_def) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/017_block_func_def.pa");

	// double_val is in sa["functions"]
	bool found_func = false;
	for (auto& f : jout["functions"])
		if (f["name"] == "double_val") { found_func = true; break; }
	ASSERT_TRUE(found_func);

	// block body does NOT contain func-def (it's been processed out)
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		for (auto& bs : s["body"])
			ASSERT_NE(bs["stmt-type"], "func-def");
	}

	// block body contains the printf call as expr
	bool found_call = false;
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		for (auto& bs : s["body"])
			if (bs["stmt-type"] == "expr") { found_call = true; break; }
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, unary_minus) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/038_unary_minus.pa");

	// Second var-decl: y = -x  should have neg init with value-type int64
	bool found_neg = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (!v.contains("init")) continue;
			auto& init = v["init"];
			if (init["expr-type"] == "neg") {
				ASSERT_TRUE(init.contains("operand"));
				ASSERT_EQ(init["value-type"]["type-name"], "int64");
				ASSERT_EQ(init["operand"]["expr-type"], "id");
				found_neg = true;
			}
		}
	}
	ASSERT_TRUE(found_neg);
}

TEST(sa, while_stmt) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/039_while_loop.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_while = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "while") continue;
		found_while = true;
		// cond is a cmp expression with value-type int32
		ASSERT_EQ(stmt["cond"]["expr-type"], "cmp");
		ASSERT_EQ(stmt["cond"]["value-type"]["type-name"], "int32");
		ASSERT_EQ(stmt["cond"]["op"], "<");
		// body is a raw array containing an assign statement
		ASSERT_TRUE(stmt.contains("body"));
		ASSERT_EQ(stmt["body"].size(), 1u);
		ASSERT_EQ(stmt["body"][0]["stmt-type"], "assign");
	}
	ASSERT_TRUE(found_while);
}

TEST(sa, void_func) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/040_void_func.pa");

	ASSERT_TRUE(jout.is_object());

	// greet function: no ret-type, body has bare return
	ASSERT_EQ(jout["functions"].size(), 1u);
	const json& greet = jout["functions"][0];
	ASSERT_EQ(greet["name"], "greet");
	ASSERT_FALSE(greet.contains("ret-type"));
	ASSERT_FALSE(greet.contains("rets"));
	ASSERT_EQ(greet["body"][0]["stmt-type"], "return");
	ASSERT_FALSE(greet["body"][0].contains("values"));

	// greet() called as expr stmt: no value-type on the call
	bool found_call = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "greet") continue;
		ASSERT_EQ(body["func-type"], "palan");
		ASSERT_FALSE(body.contains("value-type"));
		found_call = true;
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, float_var_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/041_float_var_decl.pa");

	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 3u);

	// flo64 x = 3.14 → lit-flo adopts flo64 type
	const auto& x = jout["statements"][0]["vars"][0];
	ASSERT_EQ(x["name"], "x");
	ASSERT_EQ(x["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "flo64");

	// flo32 y = 1.5 → lit-flo adopts flo32 type
	const auto& y = jout["statements"][1]["vars"][0];
	ASSERT_EQ(y["name"], "y");
	ASSERT_EQ(y["var-type"]["type-name"], "flo32");
	ASSERT_EQ(y["init"]["value-type"]["type-name"], "flo32");

	// flo64 z (no init)
	const auto& z = jout["statements"][2]["vars"][0];
	ASSERT_EQ(z["name"], "z");
	ASSERT_EQ(z["var-type"]["type-name"], "flo64");
	ASSERT_FALSE(z.contains("init"));
}

TEST(sa, int_to_float_implicit) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/042_int_to_float_implicit.pa");

	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 2u);

	// int64 n = 3 → plain var-decl, no convert
	const auto& n = jout["statements"][0]["vars"][0];
	ASSERT_EQ(n["var-type"]["type-name"], "int64");

	// flo64 x = n → init wrapped in convert(int64 → flo64)
	const auto& x = jout["statements"][1]["vars"][0];
	ASSERT_EQ(x["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["expr-type"], "convert");
	ASSERT_EQ(x["init"]["from-type"]["type-name"], "int64");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["src"]["expr-type"], "id");
	ASSERT_EQ(x["init"]["src"]["name"], "n");
}

TEST(sa, array_top_level_malloc) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/043_array_top_level.pa");
	ASSERT_TRUE(jout.is_object());

	bool found_buf = false, found_arr = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "buf") {
				// arr → pntr/uint8
				ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "uint8");
				// init = malloc(64)
				ASSERT_EQ(v["init"]["expr-type"], "call");
				ASSERT_EQ(v["init"]["name"], "malloc");
				ASSERT_EQ(v["init"]["func-type"], "c");
				ASSERT_EQ(v["init"]["args"][0]["value"], "64");
				found_buf = true;
			}
			if (v["name"] == "arr") {
				// arr → pntr/int64
				ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int64");
				// init = malloc(n * 8)
				ASSERT_EQ(v["init"]["expr-type"], "call");
				ASSERT_EQ(v["init"]["name"], "malloc");
				ASSERT_EQ(v["init"]["args"][0]["expr-type"], "mul");
				ASSERT_EQ(v["init"]["args"][0]["left"]["name"], "n");
				ASSERT_EQ(v["init"]["args"][0]["right"]["value"], "8");
				found_arr = true;
			}
		}
	}
	ASSERT_TRUE(found_buf);
	ASSERT_TRUE(found_arr);
	// Top-level: no free() stmt in top-level statements
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		ASSERT_NE(stmt["body"].value("name", ""), "free");
	}
}

TEST(sa, array_func_scope_free) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/044_array_scope_free.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_FALSE(jout["functions"].empty());

	auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);
	// body[0]: var-decl with pntr type and malloc init
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");
	ASSERT_EQ(body[0]["vars"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[0]["vars"][0]["init"]["name"], "malloc");
	// body.back(): free(buf)
	auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "buf");
}

TEST(sa, array_while_break_free) {
	// while loop with array + break: SA must insert free(buf) before break
	// and at end of while body.  Covers collectFreeStmts path for break.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/046_array_while_break.pa");
	ASSERT_TRUE(jout.is_object());
	// Find the while statement
	json* while_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "while") { while_stmt = &s; break; }
	ASSERT_NE(while_stmt, nullptr);

	auto& body = (*while_stmt)["body"];
	// body[0]: var-decl buf = malloc(64)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");
	ASSERT_EQ(body[0]["vars"][0]["init"]["name"], "malloc");

	// body.back(): free(buf) at end of while body (normal iteration exit)
	ASSERT_EQ(body.back()["stmt-type"], "expr");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], "buf");

	// Find if stmt and verify free(buf) is inserted before break in then block
	json* if_stmt = nullptr;
	for (auto& s : body)
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);
	auto& then_body = (*if_stmt)["then"]["body"];
	ASSERT_GE(then_body.size(), 2u);
	ASSERT_EQ(then_body[0]["stmt-type"], "expr");
	ASSERT_EQ(then_body[0]["body"]["name"], "free");
	ASSERT_EQ(then_body[1]["stmt-type"], "break");
}

TEST(sa, array_while_continue_free) {
	// while loop with array + continue: SA must insert free(buf) before continue
	// and at end of while body.  Covers collectFreeStmts path for continue.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/047_array_while_continue.pa");
	ASSERT_TRUE(jout.is_object());
	json* while_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "while") { while_stmt = &s; break; }
	ASSERT_NE(while_stmt, nullptr);

	auto& body = (*while_stmt)["body"];
	// body[0]: var-decl buf = malloc(64)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");

	// body.back(): free(buf) at end of while body
	ASSERT_EQ(body.back()["stmt-type"], "expr");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], "buf");

	// Find if stmt and verify free(buf) is inserted before continue in then block
	json* if_stmt = nullptr;
	for (auto& s : body)
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);
	auto& then_body = (*if_stmt)["then"]["body"];
	ASSERT_GE(then_body.size(), 2u);
	ASSERT_EQ(then_body[0]["stmt-type"], "expr");
	ASSERT_EQ(then_body[0]["body"]["name"], "free");
	ASSERT_EQ(then_body[1]["stmt-type"], "continue");
}

TEST(sa, arr_index_elem_type) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/048_arr_index_elem_type.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0] var-decl a (malloc), [1] var-decl i, [2] expr(a[i])
	ASSERT_GE(jout["statements"].size(), 3u);
	const auto& expr_stmt = jout["statements"][2];
	ASSERT_EQ(expr_stmt["stmt-type"], "expr");

	const auto& body = expr_stmt["body"];
	ASSERT_EQ(body["expr-type"],                    "arr-index");
	ASSERT_EQ(body["value-type"]["type-name"],       "int32");
	ASSERT_EQ(body["elem-size"]["expr-type"],        "lit-uint");
	ASSERT_EQ(body["elem-size"]["value"],            "4");
	ASSERT_EQ(body["array"]["name"],                 "a");
	ASSERT_EQ(body["array"]["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(body["index"]["name"],                 "i");
}

TEST(sa, arr_assign_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/049_arr_assign_convert.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0] var-decl a, [1] var-decl x, [2] var-decl i, [3] arr-assign
	ASSERT_GE(jout["statements"].size(), 4u);
	const auto& stmt = jout["statements"][3];
	ASSERT_EQ(stmt["stmt-type"], "arr-assign");

	// target is arr-index with value-type int64
	ASSERT_EQ(stmt["target"]["expr-type"],           "arr-index");
	ASSERT_EQ(stmt["target"]["value-type"]["type-name"], "int64");

	// value is wrapped in convert: int32 -> int64
	ASSERT_EQ(stmt["value"]["expr-type"],              "convert");
	ASSERT_EQ(stmt["value"]["from-type"]["type-name"], "int32");
	ASSERT_EQ(stmt["value"]["value-type"]["type-name"], "int64");
	ASSERT_EQ(stmt["value"]["src"]["name"],            "x");
}

TEST(sa, array_multi_var_shared_size) {
	// [4]int32 a, b; — two vars share one size temp var (__arr_sz_0)
	// Covers: elem_size > 1 (mul) path and vars.size() > 1 path.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/045_array_multi_var.pa");
	ASSERT_TRUE(jout.is_object());
	auto& stmts = jout["statements"];
	// Expect two var-decl statements: [0] temp size var, [1] a and b
	ASSERT_GE(stmts.size(), 2u);

	// stmts[0]: __arr_sz_0 = 4 * 4 (mul of count and elem_size)
	ASSERT_EQ(stmts[0]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[0]["vars"].size(), 1u);
	auto& sz_var = stmts[0]["vars"][0];
	ASSERT_EQ(sz_var["name"], "__arr_sz_0");
	ASSERT_EQ(sz_var["var-type"]["type-name"], "uint64");
	ASSERT_EQ(sz_var["init"]["expr-type"], "mul");
	ASSERT_EQ(sz_var["init"]["left"]["value"], "4");   // count
	ASSERT_EQ(sz_var["init"]["right"]["value"], "4");  // elem_size of int32

	// stmts[1]: a and b, each malloc(__arr_sz_0)
	ASSERT_EQ(stmts[1]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[1]["vars"].size(), 2u);
	for (auto& v : stmts[1]["vars"]) {
		ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
		ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int32");
		ASSERT_EQ(v["init"]["expr-type"], "call");
		ASSERT_EQ(v["init"]["name"], "malloc");
		ASSERT_EQ(v["init"]["args"][0]["expr-type"], "id");
		ASSERT_EQ(v["init"]["args"][0]["name"], "__arr_sz_0");
	}
}

TEST(sa, unsized_arr_sig) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/050_unsized_arr_sig.pa");
	ASSERT_TRUE(jout.is_object());

	auto findFunc = [&](const string& name) -> const json* {
		for (auto& f : jout["functions"])
			if (f["name"] == name) return &f;
		return nullptr;
	};

	// getArr: []int32 return type → pntr(int32)
	const json* getArr = findFunc("getArr");
	ASSERT_NE(getArr, nullptr);
	ASSERT_EQ((*getArr)["ret-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getArr)["ret-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ((*getArr)["ret-type"]["base-type"]["type-name"], "int32");

	// sumArr: []int32 parameter → pntr(int32)
	const json* sumArr = findFunc("sumArr");
	ASSERT_NE(sumArr, nullptr);
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["base-type"]["type-name"], "int32");

	// getNestedArr: [][]int32 return type → pntr(pntr(int32))
	const json* getNestedArr = findFunc("getNestedArr");
	ASSERT_NE(getNestedArr, nullptr);
	ASSERT_EQ((*getNestedArr)["ret-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getNestedArr)["ret-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getNestedArr)["ret-type"]["base-type"]["base-type"]["type-name"], "int32");
}

TEST(sa, pntr_arr_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/051_pntr_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];

	// body[0]: var-decl ptrs → pntr(pntr(int32)), malloc(5 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	const auto& var = body[0]["vars"][0];
	ASSERT_EQ(var["name"], "ptrs");
	ASSERT_EQ(var["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(var["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(var["var-type"]["base-type"]["base-type"]["type-name"], "int32");

	// malloc(5 * 8)
	ASSERT_EQ(var["init"]["name"], "malloc");
	ASSERT_EQ(var["init"]["args"][0]["expr-type"], "mul");
	ASSERT_EQ(var["init"]["args"][0]["left"]["value"], "5");
	ASSERT_EQ(var["init"]["args"][0]["right"]["value"], "8");

	// body[1]: expr ptrs[i] — arr-index with elem-size=8, value-type=pntr(int32)
	ASSERT_EQ(body[1]["stmt-type"], "expr");
	const auto& idx = body[1]["body"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["elem-size"]["value"], "8");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "int32");

	// body.back(): free(ptrs) at scope end
	ASSERT_EQ(body.back()["stmt-type"], "expr");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], "ptrs");
}

TEST(sa, ownership_transfer) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/052_ownership_transfer.pa");
	ASSERT_TRUE(jout.is_object());

	// functions[0]: testOwnedReturn() -> []int32
	{
		const auto& body = jout["functions"][0]["body"];
		// body[0]: var-decl arr
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		ASSERT_EQ(body[0]["vars"][0]["name"], "arr");
		// body[1]: return arr — no free(arr) before return
		ASSERT_EQ(body[1]["stmt-type"], "return");
		ASSERT_EQ(body[1]["values"][0]["name"], "arr");
		ASSERT_EQ(body.size(), 2u);
	}

	// functions[1]: testTransfer(int64 i)
	{
		const auto& body = jout["functions"][1]["body"];
		// body[0]: var-decl inner — pntr(int32)
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		ASSERT_EQ(body[0]["vars"][0]["name"], "inner");
		// body[1]: var-decl outer — pntr(pntr(int32))
		ASSERT_EQ(body[1]["stmt-type"], "var-decl");
		ASSERT_EQ(body[1]["vars"][0]["name"], "outer");
		// body[2]: arr-assign with ownership-transfer: true
		ASSERT_EQ(body[2]["stmt-type"], "arr-assign");
		ASSERT_EQ(body[2].value("ownership-transfer", false), true);
		// body[3]: null-assign for inner (inner = 0)
		ASSERT_EQ(body[3]["stmt-type"], "assign");
		ASSERT_EQ(body[3]["name"], "inner");
		ASSERT_EQ(body[3]["value"]["expr-type"], "lit-int");
		ASSERT_EQ(body[3]["value"]["value"], "0");
		ASSERT_EQ(body[3]["value"]["value-type"]["type-kind"], "pntr");
		// body[4]: testOwnedReturn() call with category: "expiring"
		ASSERT_EQ(body[4]["stmt-type"], "expr");
		ASSERT_EQ(body[4]["body"]["name"], "testOwnedReturn");
		ASSERT_EQ(body[4]["body"]["category"], "expiring");
		// body[5..6]: free(outer), free(inner) before return
		ASSERT_EQ(body[5]["stmt-type"], "expr");
		ASSERT_EQ(body[5]["body"]["name"], "free");
		ASSERT_EQ(body[6]["stmt-type"], "expr");
		ASSERT_EQ(body[6]["body"]["name"], "free");
		// body[7]: return
		ASSERT_EQ(body[7]["stmt-type"], "return");
	}

	// functions[2]: testFreePtr(int64 i)
	{
		const auto& body = jout["functions"][2]["body"];
		// body[0]: var-decl outer
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		// body[1]: user-written free(outer[i]) — arg is arr-index
		ASSERT_EQ(body[1]["stmt-type"], "expr");
		ASSERT_EQ(body[1]["body"]["name"], "free");
		ASSERT_EQ(body[1]["body"]["args"][0]["expr-type"], "arr-index");
		// body[2]: SA-generated free(outer) at return
		ASSERT_EQ(body[2]["stmt-type"], "expr");
		ASSERT_EQ(body[2]["body"]["name"], "free");
		ASSERT_EQ(body[2]["body"]["args"][0]["name"], "outer");
		// body[3]: return
		ASSERT_EQ(body[3]["stmt-type"], "return");
	}
}

TEST(sa, 2d_arr_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/053_2d_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes: one entry for arr_arr_int32
	ASSERT_TRUE(jout.contains("alloc-shapes"));
	ASSERT_EQ(jout["alloc-shapes"].size(), 1u);
	auto& shape = jout["alloc-shapes"][0];
	ASSERT_EQ(shape["shape-key"], "arr_arr_int32");
	ASSERT_EQ(shape["leaf-type"], "int32");
	ASSERT_EQ(shape["depth"], 2);

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[0]: rows var-decl, body[1]: cols var-decl
	// body[2]: __mat_d0 = rows (uint64 temp for outer dimension)
	ASSERT_EQ(body[2]["stmt-type"], "var-decl");
	ASSERT_EQ(body[2]["vars"][0]["name"], "__mat_d0");
	ASSERT_EQ(body[2]["vars"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(body[2]["vars"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ(body[2]["vars"][0]["init"]["name"], "rows");

	// body[3]: mat = __pln_alloc_arr_arr_int32(__mat_d0, cols)
	ASSERT_EQ(body[3]["stmt-type"], "var-decl");
	ASSERT_EQ(body[3]["vars"][0]["name"], "mat");
	ASSERT_EQ(body[3]["vars"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[3]["vars"][0]["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[3]["vars"][0]["var-type"]["base-type"]["base-type"]["type-name"], "int32");
	auto& init = body[3]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["name"], "__pln_alloc_arr_arr_int32");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["args"][0]["name"], "__mat_d0");
	ASSERT_EQ(init["args"][1]["name"], "cols");

	// body.back(): __pln_free_arr_arr_int32(mat, __mat_d0) at scope exit
	auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "__pln_free_arr_arr_int32");
	ASSERT_EQ(last["body"]["func-type"], "palan");
	ASSERT_EQ(last["body"]["args"][0]["name"], "mat");
	ASSERT_EQ(last["body"]["args"][1]["name"], "__mat_d0");
}

TEST(sa, logical_ops) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/054_logical_ops.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// [0] int64 a, [1] int64 b, [2] int32 c, [3] a&&b, [4] a||b, [5] !a, [6] a&&b||!a, [7] a&&int32(c)
	ASSERT_GE(stmts.size(), 8u);

	// a && b  →  value-type int32
	const auto& land = stmts[3]["body"];
	ASSERT_EQ(land["expr-type"],               "logical-and");
	ASSERT_EQ(land["value-type"]["type-name"], "int32");
	ASSERT_EQ(land["left"]["name"],            "a");
	ASSERT_EQ(land["right"]["name"],           "b");

	// a || b  →  value-type int32
	const auto& lor = stmts[4]["body"];
	ASSERT_EQ(lor["expr-type"],               "logical-or");
	ASSERT_EQ(lor["value-type"]["type-name"], "int32");

	// !a  →  value-type int32
	const auto& lnot = stmts[5]["body"];
	ASSERT_EQ(lnot["expr-type"],               "logical-not");
	ASSERT_EQ(lnot["value-type"]["type-name"], "int32");
	ASSERT_EQ(lnot["operand"]["name"],         "a");

	// a && b || !a  →  (a&&b) || (!a)
	const auto& mixed = stmts[6]["body"];
	ASSERT_EQ(mixed["expr-type"],                  "logical-or");
	ASSERT_EQ(mixed["value-type"]["type-name"],    "int32");
	ASSERT_EQ(mixed["left"]["expr-type"],          "logical-and");
	ASSERT_EQ(mixed["right"]["expr-type"],         "logical-not");

	// a && int32(c)  →  convert wraps c, value-type int32
	const auto& with_cast = stmts[7]["body"];
	ASSERT_EQ(with_cast["expr-type"],               "logical-and");
	ASSERT_EQ(with_cast["value-type"]["type-name"], "int32");
}

TEST(sa, embed_arr_decl_const_inner) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/055_embed_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes must be empty (embedded arrays use stdlib malloc/free)
	ASSERT_TRUE(jout["alloc-shapes"].empty());

	// func f: single body stmt + free at scope end
	ASSERT_FALSE(jout["functions"].empty());
	const auto& body_f = jout["functions"][0]["body"];

	// body[0]: mat = malloc(rows * 16)
	ASSERT_EQ(body_f[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body_f[0]["vars"][0]["name"], "mat");
	const auto& mat_vtype = body_f[0]["vars"][0]["var-type"];
	ASSERT_EQ(mat_vtype["type-kind"],   "pntr");
	ASSERT_TRUE(mat_vtype.value("embedded", false));
	ASSERT_EQ(mat_vtype["inner-size"],  4);
	ASSERT_EQ(mat_vtype["base-type"]["type-name"], "int32");

	const auto& init = body_f[0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["name"],      "malloc");
	ASSERT_EQ(init["args"][0]["expr-type"],  "mul");
	ASSERT_EQ(init["args"][0]["right"]["value"], "16");

	// body[1]: return; body[2]: free(mat)  (or body.back() == free)
	const auto& last_f = body_f.back();
	ASSERT_EQ(last_f["stmt-type"], "expr");
	ASSERT_EQ(last_f["body"]["name"], "free");
	ASSERT_EQ(last_f["body"]["args"][0]["name"], "mat");
}

TEST(sa, embed_arr_decl_variable_inner) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/055_embed_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_GE(jout["functions"].size(), 2u);

	const auto& body_g = jout["functions"][1]["body"];

	// body[0]: __mat_d1 = cols (uint64 hidden var for inner dimension)
	ASSERT_EQ(body_g[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body_g[0]["vars"][0]["name"], "__mat_d1");
	ASSERT_EQ(body_g[0]["vars"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(body_g[0]["vars"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ(body_g[0]["vars"][0]["init"]["name"], "cols");

	// body[1]: mat = malloc(rows * (__mat_d1 * 4))  — var-type has no inner-size
	ASSERT_EQ(body_g[1]["stmt-type"], "var-decl");
	ASSERT_EQ(body_g[1]["vars"][0]["name"], "mat");
	const auto& mat_vtype_g = body_g[1]["vars"][0]["var-type"];
	ASSERT_EQ(mat_vtype_g["type-kind"], "pntr");
	ASSERT_TRUE(mat_vtype_g.value("embedded", false));
	ASSERT_FALSE(mat_vtype_g.contains("inner-size"));
	ASSERT_EQ(mat_vtype_g["base-type"]["type-name"], "int32");

	const auto& init_g = body_g[1]["vars"][0]["init"];
	ASSERT_EQ(init_g["expr-type"], "call");
	ASSERT_EQ(init_g["name"],      "malloc");
	// args[0]: mul(rows, mul(__mat_d1, lit-uint(4)))
	ASSERT_EQ(init_g["args"][0]["expr-type"], "mul");
	ASSERT_EQ(init_g["args"][0]["right"]["expr-type"], "mul");
	ASSERT_EQ(init_g["args"][0]["right"]["left"]["name"], "__mat_d1");
	ASSERT_EQ(init_g["args"][0]["right"]["right"]["value"], "4");

	// body.back(): free(mat)
	const auto& last_g = body_g.back();
	ASSERT_EQ(last_g["stmt-type"], "expr");
	ASSERT_EQ(last_g["body"]["name"], "free");
	ASSERT_EQ(last_g["body"]["args"][0]["name"], "mat");
}

TEST(sa, embed_arr_func_param) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/056_embed_arr_func_param.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_GE(jout["functions"].size(), 2u);

	// func process: first param is pntr, embedded:true, inner-size:4
	const auto& params = jout["functions"][0]["parameters"];
	ASSERT_EQ(params[0]["name"], "mat");
	const auto& mat_vt = params[0]["var-type"];
	ASSERT_EQ(mat_vt["type-kind"], "pntr");
	ASSERT_TRUE(mat_vt.value("embedded", false));
	ASSERT_EQ(mat_vt["inner-size"], 4);
	ASSERT_EQ(mat_vt["base-type"]["type-name"], "int32");

	// return mat[0][0]: nested arr-index, outer elem-size=16, inner elem-size=4
	const auto& ret = jout["functions"][0]["body"][0];
	ASSERT_EQ(ret["stmt-type"], "return");
	const auto& outer_idx = ret["values"][0];
	ASSERT_EQ(outer_idx["expr-type"], "arr-index");
	ASSERT_EQ(outer_idx["elem-size"]["value"], "4");
	ASSERT_EQ(outer_idx["array"]["expr-type"], "arr-index");
	ASSERT_EQ(outer_idx["array"]["elem-size"]["value"], "16");

	// func caller: call to process succeeds (no error)
	const auto& caller_body = jout["functions"][1]["body"];
	bool found_call = false;
	for (const auto& stmt : caller_body) {
		if (stmt["stmt-type"] == "expr" && stmt["body"]["expr-type"] == "call"
				&& stmt["body"]["name"] == "process") {
			found_call = true;
			break;
		}
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, embed_arr_var_row_access) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/057_embed_arr_var_row.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];

	// body[0]: hidden var __mat_d1 = cols (inner dimension)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "__mat_d1");
	ASSERT_EQ(body[0]["vars"][0]["init"]["name"], "cols");

	// body[1]: mat = malloc(n * (__mat_d1 * 4)), no inner-size field
	ASSERT_EQ(body[1]["vars"][0]["name"], "mat");
	ASSERT_FALSE(body[1]["vars"][0]["var-type"].contains("inner-size"));

	// body[3]: return mat[0][0] — inner arr-index (row access) has mul elem-size
	const auto& ret_val = body[3]["values"][0];
	ASSERT_EQ(ret_val["expr-type"], "arr-index");
	const auto& row_idx = ret_val["array"];
	ASSERT_EQ(row_idx["expr-type"], "arr-index");
	ASSERT_EQ(row_idx["elem-size"]["expr-type"],    "mul");
	ASSERT_EQ(row_idx["elem-size"]["left"]["name"], "__mat_d1");
	ASSERT_EQ(row_idx["elem-size"]["right"]["value"], "4");
}

static void genLibSaImport()
{
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
}

TEST(sa, import_unqualified)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/058_import_unqualified.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["name"], "square");
}

TEST(sa, import_block_scope)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/059_import_block_scope.pa");
	ASSERT_TRUE(jout.is_object());
	auto& block = jout["statements"][0];
	ASSERT_EQ(block["stmt-type"], "block");
	const auto& inner = block["body"][0];
	ASSERT_EQ(inner["stmt-type"], "var-decl");
	ASSERT_EQ(inner["vars"][0]["init"]["func-type"], "palan");
}

TEST(sa, import_selective)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/060_import_selective.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["name"], "square");
}

TEST(sa, import_alias)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/061_import_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& a_init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(a_init["expr-type"], "call");
	ASSERT_EQ(a_init["func-type"], "palan");
	ASSERT_EQ(a_init["name"], "square");
	const auto& b_init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(b_init["func-type"], "palan");
	ASSERT_EQ(b_init["name"], "cube");
}

TEST(sa, import_alias_tapple)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/062_import_alias_tapple.pa");
	ASSERT_TRUE(jout.is_object());
	auto& tapple = jout["statements"][0];
	ASSERT_EQ(tapple["stmt-type"], "tapple-decl");
	ASSERT_EQ(tapple["value"]["expr-type"], "call");
	ASSERT_EQ(tapple["value"]["func-type"], "palan");
	ASSERT_EQ(tapple["value"]["name"], "divmod");
	ASSERT_EQ(tapple["value"]["value-types"].size(), 2u);
}

TEST(sa, cinclude_alias)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/063_cinclude_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmt = jout["statements"][0];
	ASSERT_EQ(stmt["stmt-type"], "expr");
	const auto& call = stmt["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["func-type"], "c");
	ASSERT_EQ(call["name"], "printf");
}

TEST(sa, lit_uint_typed)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/064_lit_uint_typed.pa");
	ASSERT_TRUE(jout.is_object());

	// uint32 x = 5u → lit-uint adopts uint32 from expectedType
	const auto& x = jout["statements"][0]["vars"][0];
	ASSERT_EQ(x["name"], "x");
	ASSERT_EQ(x["init"]["expr-type"], "lit-uint");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "uint32");

	// uint8 y = 3u → lit-uint adopts uint8 from expectedType
	const auto& y = jout["statements"][1]["vars"][0];
	ASSERT_EQ(y["name"], "y");
	ASSERT_EQ(y["init"]["value-type"]["type-name"], "uint8");
}

TEST(sa, lit_flo_variadic_arg)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/065_lit_flo_variadic.pa");
	ASSERT_TRUE(jout.is_object());

	// 3.14 passed to printf (variadic, no expectedType) → value-type defaults to flo64
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		const auto& call = stmt["body"];
		if (call["name"] != "printf") continue;
		for (const auto& arg : call["args"]) {
			if (arg["expr-type"] != "lit-flo") continue;
			ASSERT_EQ(arg["value-type"]["type-name"], "flo64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, cmp_mixed_int_widening)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/066_cmp_mixed_int.pa");
	ASSERT_TRUE(jout.is_object());

	// int32 x = a(int32) < b(int64) → left(a) wrapped in convert
	const auto& x_cmp = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(x_cmp["expr-type"], "cmp");
	ASSERT_EQ(x_cmp["left"]["expr-type"],              "convert");
	ASSERT_EQ(x_cmp["left"]["value-type"]["type-name"], "int64");

	// int32 y = b(int64) < a(int32) → right(a) wrapped in convert
	const auto& y_cmp = jout["statements"][3]["vars"][0]["init"];
	ASSERT_EQ(y_cmp["expr-type"], "cmp");
	ASSERT_EQ(y_cmp["right"]["expr-type"],              "convert");
	ASSERT_EQ(y_cmp["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, arith_right_widen)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/067_arith_right_widen.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 c = a(int64) + b(int32) → right(b) wrapped in convert to int64
	const auto& add = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(add["expr-type"], "add");
	ASSERT_EQ(add["left"]["expr-type"],               "id");
	ASSERT_EQ(add["left"]["value-type"]["type-name"],  "int64");
	ASSERT_EQ(add["right"]["expr-type"],               "convert");
	ASSERT_EQ(add["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, arith_lit_retypes_to_right)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/068_arith_lit_retypes.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 c = 4 + b(int64) → lit-int on left retypes to int64 via second pass
	const auto& add = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(add["expr-type"], "add");
	ASSERT_EQ(add["left"]["expr-type"],               "lit-int");
	ASSERT_EQ(add["left"]["value-type"]["type-name"],  "int64");
	ASSERT_EQ(add["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, struct_def)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/069_struct_def.pa");
	ASSERT_TRUE(jout.is_object());

	// struct-def is consumed; first stmt is var-decl for p
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	// init: calloc(1, 16)
	ASSERT_EQ(v["init"]["expr-type"], "call");
	ASSERT_EQ(v["init"]["name"],      "calloc");
	ASSERT_EQ(v["init"]["func-type"], "c");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "16");
}

TEST(sa, type_alias_var)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/120_type_alias_var.pa");
	ASSERT_TRUE(jout.is_object());

	// type-alias is consumed; first stmt is var-decl for x, resolved to int64
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "x");
	ASSERT_EQ(v["var-type"]["type-kind"], "prim");
	ASSERT_EQ(v["var-type"]["type-name"], "int64");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "int64");
}

TEST(sa, type_alias_func)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/121_type_alias_func.pa");
	ASSERT_TRUE(jout.is_object());

	const json* doubleFunc = nullptr;
	for (auto& f : jout["functions"])
		if (f["name"] == "double") doubleFunc = &f;
	ASSERT_NE(doubleFunc, nullptr);

	// Count param/ret resolved to uint64, not left as the alias name
	ASSERT_EQ((*doubleFunc)["parameters"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ((*doubleFunc)["parameters"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ((*doubleFunc)["ret-type"]["type-kind"], "prim");
	ASSERT_EQ((*doubleFunc)["ret-type"]["type-name"], "uint64");

	// body: n * 2 -> r; assign stmt with mul expression, no crash resolving Count
	bool found_assign = false;
	for (auto& stmt : (*doubleFunc)["body"]) {
		if (stmt["stmt-type"] != "assign") continue;
		ASSERT_EQ(stmt["name"], "r");
		ASSERT_EQ(stmt["value"]["value-type"]["type-name"], "uint64");
		found_assign = true;
	}
	ASSERT_TRUE(found_assign);
}

TEST(sa, cinclude_typedef_size_t)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/122_cinclude_typedef_size_t.pa");
	ASSERT_TRUE(jout.is_object());

	// size_t resolved via IT-2604's cinclude typedef bridge to a clean uint64,
	// with no leftover "typedef-name" bookkeeping key.
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["var-type"]["type-kind"], "prim");
	ASSERT_EQ(v["var-type"]["type-name"], "uint64");
	ASSERT_FALSE(v["var-type"].contains("typedef-name"));

	// strlen() call's value-type is likewise clean.
	ASSERT_EQ(v["init"]["expr-type"], "call");
	ASSERT_EQ(v["init"]["name"], "strlen");
	ASSERT_EQ(v["init"]["func-type"], "c");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "uint64");
	ASSERT_FALSE(v["init"]["value-type"].contains("typedef-name"));
}

TEST(sa, field_assign)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/070_field_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[1]: 10 -> p.x
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["stmt-type"],                  "field-assign");
	ASSERT_EQ(fa1["var"],                         "p");
	ASSERT_EQ(fa1["offset"],                      0);
	ASSERT_EQ(fa1["value-type"]["type-name"],     "int64");
	ASSERT_EQ(fa1["value"]["value"],              "10");

	// statements[2]: 20 -> p.y
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["stmt-type"],                  "field-assign");
	ASSERT_EQ(fa2["var"],                         "p");
	ASSERT_EQ(fa2["offset"],                      8);
	ASSERT_EQ(fa2["value-type"]["type-name"],     "int64");
	ASSERT_EQ(fa2["value"]["value"],              "20");
}

TEST(sa, field_access)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/071_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[2]: int64 v = p.x
	const auto& fa = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(fa["expr-type"],              "field-access");
	ASSERT_EQ(fa["var"],                     "p");
	ASSERT_EQ(fa["offset"],                  0);
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
}

TEST(sa, struct_c_abi)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/072_struct_c_abi.pa");
	ASSERT_TRUE(jout.is_object());

	// var-decl for m: calloc(1, 16) — int32 a@0(4B) + pad(4B) + int64 b@8(8B) = 16B
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["args"][1]["value"], "16");

	// 10 -> m.a: offset=0, int32
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["offset"],                  0);
	ASSERT_EQ(fa1["value-type"]["type-name"], "int32");

	// 20 -> m.b: offset=8, int64
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["offset"],                  8);
	ASSERT_EQ(fa2["value-type"]["type-name"], "int64");
}

TEST(sa, field_assign_widen)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/073_field_assign_widen.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[2]: v -> b.val  (int32 widened to int64)
	// SA wraps the value in a convert expression
	const auto& fa = jout["statements"][2];
	ASSERT_EQ(fa["stmt-type"],              "field-assign");
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
	ASSERT_EQ(fa["value"]["expr-type"],      "convert");
	ASSERT_EQ(fa["value"]["value-type"]["type-name"], "int64");
}

TEST(sa, embed_struct_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/074_embed_struct_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Line { $Point a; $Point b; } — totalSize = 16+16 = 32
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "l");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][1]["value"], "32");
}

TEST(sa, owned_struct_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/075_owned_struct_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Rect { Point tl; Point br; } — hasOwnedStructFields → __pln_alloc_Rect
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["init"]["name"], "__pln_alloc_Rect");
	ASSERT_EQ(v["init"]["func-type"], "pln");
	ASSERT_TRUE(v["init"]["args"].empty());
}

TEST(sa, raw_ptr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/076_raw_ptr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Node { int64 val; @Node next; } — int64(8) + ptr(8) = 16
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][1]["value"], "16");
}

TEST(sa, alloc_shape_owned)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/077_alloc_shape_owned.pa");
	ASSERT_TRUE(jout.is_object());

	// Rect r; — init uses __pln_alloc_Rect (pln func, no args)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["init"]["name"], "__pln_alloc_Rect");
	ASSERT_EQ(v["init"]["func-type"], "pln");
	ASSERT_TRUE(v["init"]["args"].empty());

	// alloc-shapes: Point first (leaf), then Rect (root)
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 2u);
	ASSERT_EQ(shapes[0]["shape-name"], "Point");
	ASSERT_EQ(shapes[0]["shape-kind"], "struct");
	ASSERT_EQ(shapes[0]["total-size"], 16);
	ASSERT_EQ(shapes[0]["owned-fields"].size(), 0u);
	ASSERT_EQ(shapes[1]["shape-name"], "Rect");
	ASSERT_EQ(shapes[1]["total-size"], 16);
	ASSERT_EQ(shapes[1]["owned-fields"].size(), 2u);
	ASSERT_EQ(shapes[1]["owned-fields"][0]["struct-name"], "Point");
	ASSERT_EQ(shapes[1]["owned-fields"][0]["needs-alloc"], false);
	ASSERT_EQ(shapes[1]["owned-fields"][1]["struct-name"], "Point");
}

TEST(sa, calloc_no_owned)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/078_calloc_no_owned.pa");
	ASSERT_TRUE(jout.is_object());

	// Point p; — no owned struct fields → calloc
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][1]["value"], "16");

	// No struct entries added to alloc-shapes
	ASSERT_TRUE(jout["alloc-shapes"].empty());
}

TEST(sa, alloc_func_no_recurse)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/079_alloc_func_no_recurse.pa");
	ASSERT_TRUE(jout.is_object());

	// __pln_alloc_Rect() -> Rect ret { Rect ret; }
	// inAllocFunc_=true → Rect ret uses calloc(1,16), not __pln_alloc_Rect
	const auto& f = jout["functions"][0];
	ASSERT_EQ(f["name"], "__pln_alloc_Rect");

	// ret-type normalized to pntr(struct(Rect))
	ASSERT_EQ(f["ret-type"]["type-kind"], "pntr");
	ASSERT_EQ(f["ret-type"]["base-type"]["type-name"], "Rect");

	// body[0] = var-decl for ret with calloc (named return → no free appended)
	ASSERT_EQ(f["body"].size(), 1u);
	const auto& var_decl = f["body"][0];
	ASSERT_EQ(var_decl["stmt-type"], "var-decl");
	ASSERT_EQ(var_decl["vars"][0]["name"], "ret");
	ASSERT_EQ(var_decl["vars"][0]["init"]["name"], "calloc");
	ASSERT_EQ(var_decl["vars"][0]["init"]["func-type"], "c");
}

TEST(sa, struct_param)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/080_struct_param.pa");
	ASSERT_TRUE(jout.is_object());

	// func __pln_free_Point(Point p) { free(p); }
	// parameters[0]: Point p → pntr(struct(Point))
	const auto& f = jout["functions"][0];
	ASSERT_EQ(f["name"], "__pln_free_Point");

	const auto& param = f["parameters"][0];
	ASSERT_EQ(param["name"], "p");
	ASSERT_EQ(param["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(param["var-type"]["base-type"]["type-name"], "Point");
}

TEST(sa, embed_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/081_embed_field_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// 10 -> l.a.x: $Point a@0, int64 x@0 → flatten: var="l", offset=0
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["stmt-type"], "field-assign");
	ASSERT_EQ(fa1["var"],        "l");
	ASSERT_EQ(fa1["offset"],     0);

	// 20 -> l.b.y: $Point b@16, int64 y@8 → flatten: var="l", offset=24
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["stmt-type"], "field-assign");
	ASSERT_EQ(fa2["var"],        "l");
	ASSERT_EQ(fa2["offset"],     24);

	// int64 v = l.a.x: field-access: var="l", offset=0
	const auto& rd = jout["statements"][3]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"], "field-access");
	ASSERT_EQ(rd["var"],        "l");
	ASSERT_EQ(rd["offset"],     0);
}

TEST(sa, owned_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/082_owned_field_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// 10 -> r.tl.x: Point tl@0, int64 x@0 → ptr-expr={var:"r",offset:0}, offset:0
	const auto& fa = jout["statements"][1];
	ASSERT_EQ(fa["stmt-type"],              "field-assign");
	ASSERT_EQ(fa["offset"],                 0);
	ASSERT_EQ(fa["ptr-expr"]["var"],         "r");
	ASSERT_EQ(fa["ptr-expr"]["offset"],      0);
	ASSERT_EQ(fa["ptr-expr"]["value-type"]["base-type"]["type-name"], "Point");

	// int64 v = r.tl.x: field-access with ptr-expr
	const auto& rd = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],         "field-access");
	ASSERT_EQ(rd["offset"],             0);
	ASSERT_EQ(rd["ptr-expr"]["var"],    "r");
	ASSERT_EQ(rd["ptr-expr"]["offset"], 0);
}

TEST(sa, ptr_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/083_ptr_field_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 v = n.next.val: @Node next@8, int64 val@0
	// ptr-expr={var:"n",offset:8,mutable:false}, outer offset:0
	const auto& rd = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],                "field-access");
	ASSERT_EQ(rd["offset"],                   0);
	ASSERT_EQ(rd["ptr-expr"]["var"],           "n");
	ASSERT_EQ(rd["ptr-expr"]["offset"],        8);
	ASSERT_EQ(rd["ptr-expr"]["value-type"]["mutable"], false);
}

TEST(sa, mixed_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/084_mixed_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 v = outer.sub.inner.x
	// Sub sub@0 (struct-ptr) → ptr-expr={var:"outer",offset:0,pntr(struct(Sub))}
	// $Inner inner@0 (embed, flatten) → outer offset stays 0
	// int64 x@0 → final offset: 0
	const auto& rd = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],          "field-access");
	ASSERT_EQ(rd["offset"],             0);
	ASSERT_EQ(rd["ptr-expr"]["var"],    "outer");
	ASSERT_EQ(rd["ptr-expr"]["offset"], 0);
	ASSERT_EQ(rd["ptr-expr"]["value-type"]["base-type"]["type-name"], "Sub");
}

TEST(sa, flo32_to_flo64_assign)
{
	// flo32 x; 0.5->x; flo64 y; x->y;
	// x->y: typeCompat(flo32,flo64)=ImplicitWiden → sa_assign_stmt wraps in convert
	// Covers: sa_assign_stmt wrapConvert branch, primRank Float32+Float64
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/085_flo32_to_flo64_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0]=decl x, [1]=assign x=0.5, [2]=decl y, [3]=assign y=x
	const auto& assign_stmt = jout["statements"][3];
	ASSERT_EQ(assign_stmt["stmt-type"], "assign");
	ASSERT_EQ(assign_stmt["name"],      "y");
	const auto& val = assign_stmt["value"];
	ASSERT_EQ(val["expr-type"],                  "convert");
	ASSERT_EQ(val["from-type"]["type-name"],     "flo32");
	ASSERT_EQ(val["value-type"]["type-name"],    "flo64");
}

TEST(sa, int8_to_int16_assign)
{
	// int8 a; 10->a; int16 b; a->b;
	// a->b: typeCompat(int8,int16)=ImplicitWiden → convert wrapper
	// Covers: primRank Int16
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/086_int8_to_int16_assign.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& assign_stmt = jout["statements"][3];
	ASSERT_EQ(assign_stmt["stmt-type"], "assign");
	ASSERT_EQ(assign_stmt["name"],      "b");
	const auto& val = assign_stmt["value"];
	ASSERT_EQ(val["expr-type"],               "convert");
	ASSERT_EQ(val["from-type"]["type-name"],  "int8");
	ASSERT_EQ(val["value-type"]["type-name"], "int16");
}

TEST(sa, if_else_if_chain)
{
	// if x==1 { ... } else if x==2 { ... }
	// Covers: sa_if_stmt else-if branch (els["stmt-type"] == "if")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/087_if_else_if.pa");
	ASSERT_TRUE(jout.is_object());

	// Find the if statement
	json* if_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);

	// else clause must be an if statement directly (not a block)
	ASSERT_TRUE((*if_stmt).contains("else"));
	ASSERT_EQ((*if_stmt)["else"]["stmt-type"], "if");
	ASSERT_EQ((*if_stmt)["else"]["cond"]["op"], "==");
}

TEST(sa, struct_ptr_field_assign)
{
	// type A { int64 v; }; type B { A inner; }; B b; A a; a -> b.inner;
	// field-assign for struct-ptr field → fieldValueType returns pntr type
	// Covers: fieldValueType struct-ptr branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/088_struct_ptr_field_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// Find the field-assign statement
	json* fa = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "field-assign") { fa = &s; break; }
	ASSERT_NE(fa, nullptr);

	// field "inner" is a struct-ptr → value-type must be pntr(struct(A))
	ASSERT_EQ((*fa)["value-type"]["type-kind"],                  "pntr");
	ASSERT_EQ((*fa)["value-type"]["base-type"]["type-kind"],     "struct");
	ASSERT_EQ((*fa)["value-type"]["base-type"]["type-name"],     "A");
	// var-based access (b is direct variable)
	ASSERT_EQ((*fa)["var"],                                      "b");
	ASSERT_EQ((*fa)["offset"],                                   0);
}

TEST(sa, raw_ptr_mutable_field_assign)
{
	// type Node { int64 val; @!Node next; }; Node n1; Node n2; n2 -> n1.next;
	// field-assign for raw-ptr mutable field → fieldValueType sets mutable:true
	// Covers: fieldValueType raw-ptr branch (pntr["mutable"] = isMutable)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/089_raw_ptr_mutable_field_assign.pa");
	ASSERT_TRUE(jout.is_object());

	json* fa = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "field-assign") { fa = &s; break; }
	ASSERT_NE(fa, nullptr);

	// field "next" is raw-ptr mutable → value-type must be pntr with mutable:true
	ASSERT_EQ((*fa)["value-type"]["type-kind"],              "pntr");
	ASSERT_EQ((*fa)["value-type"]["mutable"],                true);
	ASSERT_EQ((*fa)["value-type"]["base-type"]["type-name"], "Node");
}

TEST(sa, deep_ptr_chain_access)
{
	// type A{int64 v}; type B{A a}; type C{B b}; C c;
	// int64 x = c.b.a.v;  — two levels of struct-ptr traversal
	// Covers: resolveObjectChain when base.isPointerBased=true (nested ptr-expr)
	// Also: resolveStoreLocChain ptr-based for "10 -> c.b.a.v"
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/090_deep_ptr_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// Find the var-decl for x (read chain)
	const json* x_decl = nullptr;
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "var-decl") continue;
		for (auto& v : s["vars"])
			if (v["name"] == "x") { x_decl = &v; break; }
		if (x_decl) break;
	}
	ASSERT_NE(x_decl, nullptr);

	// init = field-access with nested ptr-expr (c.b.a.v)
	const auto& init = (*x_decl)["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["offset"],    0);
	// ptr-expr points into B.a (second struct-ptr level)
	const auto& pe1 = init["ptr-expr"];
	ASSERT_EQ(pe1["expr-type"], "field-access");
	// pe1 itself has a ptr-expr pointing to C.b (first struct-ptr level)
	ASSERT_TRUE(pe1.contains("ptr-expr"));
	const auto& pe2 = pe1["ptr-expr"];
	ASSERT_EQ(pe2["expr-type"],  "field-access");
	ASSERT_EQ(pe2["var"],        "c");

	// Find the field-assign "10 -> c.b.a.v" (write chain)
	json* fa = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "field-assign") { fa = &s; break; }
	ASSERT_NE(fa, nullptr);
	// ptr-expr chain must also be nested
	ASSERT_TRUE((*fa).contains("ptr-expr"));
	ASSERT_TRUE((*fa)["ptr-expr"].contains("ptr-expr"));
}

TEST(sa, import_non_pa_ignored)
{
	// import "somelib.txt" — non-.pa extension → SA returns early, no error
	// Covers: imp_path.ends_with(".pa") == false → early return branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/091_import_non_pa.pa");
	ASSERT_TRUE(jout.is_object());
	// SA must succeed and produce output (import was silently ignored)
	ASSERT_TRUE(jout.contains("statements"));
}

static void genLibNoExport()
{
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_no_export.pa -o out/lib_no_export.pa.ast.json");
}

TEST(sa, import_no_export_ignored)
{
	// lib_no_export.pa has no "export" section → SA returns early after parsing
	// Covers: imp_ast.contains("export") == false → early return branch
	cleanTestEnv();
	genLibNoExport();
	json jout = run_sa("../test/testdata/sa/092_import_no_export.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_TRUE(jout.contains("statements"));
}

static void genLibSaNamedRet()
{
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_named_ret.pa -o out/lib_sa_named_ret.pa.ast.json");
}

TEST(sa, import_single_named_ret)
{
	// lib_sa_named_ret.pa: export func getVal() -> int64 ret { ... }
	// Single named return in export → SA synthesizes ret-type in import
	// Covers: funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"] in sa_import
	cleanTestEnv();
	genLibSaNamedRet();
	json jout = run_sa("../test/testdata/sa/093_import_named_ret.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 x = getVal() → call annotated as palan with ret-type int64
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"],  "call");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["name"],       "getVal");
	ASSERT_EQ(init["value-type"]["type-name"], "int64");
}

TEST(sa, variadic_flo32_promote)
{
	// printf("%f\n", x) where x is flo32 → variadicPromote(flo32) = flo64
	// Covers: variadicPromote Float32 branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/094_variadic_flo32.pa");
	ASSERT_TRUE(jout.is_object());

	// Find printf call
	json* call = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ call = &s; break; }
	ASSERT_NE(call, nullptr);

	// arg[1] (the flo32 var) should be wrapped in convert to flo64
	const auto& arg = (*call)["body"]["args"][1];
	ASSERT_EQ(arg["expr-type"],                  "convert");
	ASSERT_EQ(arg["from-type"]["type-name"],     "flo32");
	ASSERT_EQ(arg["value-type"]["type-name"],    "flo64");
}

TEST(sa, variadic_uint8_uint16_promote)
{
	// printf with uint8 and uint16 args → variadicPromote to uint32
	// Covers: variadicPromote Uint8+Uint16 branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/095_variadic_uint8_uint16.pa");
	ASSERT_TRUE(jout.is_object());

	json* call = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ call = &s; break; }
	ASSERT_NE(call, nullptr);

	// arg[1] = uint8 a → convert to uint32
	const auto& arg1 = (*call)["body"]["args"][1];
	ASSERT_EQ(arg1["expr-type"],               "convert");
	ASSERT_EQ(arg1["from-type"]["type-name"],  "uint8");
	ASSERT_EQ(arg1["value-type"]["type-name"], "uint32");

	// arg[2] = uint16 b → convert to uint32
	const auto& arg2 = (*call)["body"]["args"][2];
	ASSERT_EQ(arg2["expr-type"],               "convert");
	ASSERT_EQ(arg2["from-type"]["type-name"],  "uint16");
	ASSERT_EQ(arg2["value-type"]["type-name"], "uint32");
}

TEST(sa, array_in_if_block)
{
	// Array declared inside an if block: free stmt appended to block body
	// Covers: sa_block frees loop (PlnSaStmt.cpp line 99)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/096_array_in_block.pa");
	ASSERT_TRUE(jout.is_object());

	// Find test() function
	json* func = nullptr;
	for (auto& f : jout["functions"])
		if (f["name"] == "test") { func = &f; break; }
	ASSERT_NE(func, nullptr);

	// The if stmt is the second statement in the function body
	const auto& stmts = (*func)["body"];
	json* ifStmt = nullptr;
	for (auto& s : stmts)
		if (s["stmt-type"] == "if") { ifStmt = &const_cast<json&>(s); break; }
	ASSERT_NE(ifStmt, nullptr);

	// The then block body should end with a free() call for arr
	const auto& thenBody = (*ifStmt)["then"]["body"];
	ASSERT_GE(thenBody.size(), 2u);
	const auto& last = thenBody[thenBody.size() - 1];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
}

TEST(sa, import_absolute_path)
{
	// import "/nonexistent_lib.so" — absolute path, non-.pa extension
	// Covers: is_absolute() returning true (PlnSemanticAnalyzer.cpp line 308 branch)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/097_import_absolute_path.pa");
	ASSERT_TRUE(jout.is_object());

	// Import should be silently ignored; printf call should be present
	bool found = false;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ found = true; break; }
	ASSERT_TRUE(found);
}

TEST(sa, embed_struct_arr_decl)
{
	// [4]$Point pts — contiguous 1D struct array: malloc(4 * 16), free at scope exit
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/098_embed_struct_arr.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: pts = malloc(4 * 16)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "pts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	ASSERT_TRUE(vt.value("embedded", false));
	ASSERT_EQ(vt["stride"], 16);
	ASSERT_EQ(vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(vt["base-type"]["type-name"], "Point");

	const auto& init = body[0]["vars"][0]["init"];
	ASSERT_EQ(init["name"],      "malloc");
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["args"][0]["expr-type"],          "mul");
	ASSERT_EQ(init["args"][0]["right"]["expr-type"], "lit-uint");
	ASSERT_EQ(init["args"][0]["right"]["value"],     "16");

	// body[1]: expr pts[0] — arr-index, value-type pntr(struct(Point)), elem-size 16
	const auto& idx = body[1]["body"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(idx["value-type"].contains("mutable"));
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "16");

	// body.back(): free(pts)
	const auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "pts");
}

TEST(sa, owned_struct_arr_decl)
{
	// [2]Point pts — owned pointer array: alloc/free via palan functions
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/099_owned_struct_arr.pa");
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes: struct shape for Point, then arr-struct shape for arr_Point
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_GE(shapes.size(), 2u);
	auto arr_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "arr-struct";
	});
	ASSERT_NE(arr_it, shapes.end());
	ASSERT_EQ((*arr_it)["shape-key"],   "arr_Point");
	ASSERT_EQ((*arr_it)["struct-name"], "Point");

	auto struct_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "struct";
	});
	ASSERT_NE(struct_it, shapes.end());
	ASSERT_EQ((*struct_it)["shape-name"], "Point");

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: __pts_n = 2  (uint64 temp for count)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "__pts_n");
	ASSERT_EQ(body[0]["vars"][0]["var-type"]["type-name"], "uint64");

	// body[1]: pts = __pln_alloc_arr_Point(__pts_n)
	ASSERT_EQ(body[1]["stmt-type"], "var-decl");
	ASSERT_EQ(body[1]["vars"][0]["name"], "pts");
	const auto& vt = body[1]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
	const auto& init = body[1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["name"],      "__pln_alloc_arr_Point");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["args"][0]["name"], "__pts_n");

	// body[2]: expr pts[0] — arr-index, value-type pntr(struct(Point)), elem-size 8
	const auto& idx = body[2]["body"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(idx["value-type"].contains("mutable"));
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// body.back(): __pln_free_arr_Point(pts, __pts_n)  at scope exit
	const auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"],      "__pln_free_arr_Point");
	ASSERT_EQ(last["body"]["func-type"], "palan");
	ASSERT_EQ(last["body"]["args"][0]["name"], "pts");
	ASSERT_EQ(last["body"]["args"][1]["name"], "__pts_n");
}

TEST(sa, at_struct_arr_decl)
{
	// [4]@Point rpts — non-owning read-only pointer array: malloc(4 * 8), free at scope exit
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/100_at_struct_arr.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: rpts = malloc(4 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "rpts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	const auto& elem_vt = vt["base-type"];
	ASSERT_EQ(elem_vt["type-kind"], "pntr");
	ASSERT_EQ(elem_vt["mutable"], false);
	ASSERT_EQ(elem_vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(elem_vt["base-type"]["type-name"], "Point");

	const auto& init = body[0]["vars"][0]["init"];
	ASSERT_EQ(init["name"],      "malloc");
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["args"][0]["expr-type"],          "mul");
	ASSERT_EQ(init["args"][0]["right"]["expr-type"], "lit-uint");
	ASSERT_EQ(init["args"][0]["right"]["value"],     "8");

	// body[1]: expr rpts[0] — arr-index, value-type pntr(struct(Point), mutable:false), elem-size 8
	const auto& idx = body[1]["body"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(idx["value-type"]["mutable"], false);
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// body.back(): free(rpts)
	const auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "rpts");
}

TEST(sa, at_bang_struct_arr_decl)
{
	// [4]@!Point wpts — non-owning mutable write-through pointer array
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/102_at_bang_struct_arr.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: wpts = malloc(4 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "wpts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	const auto& elem_vt = vt["base-type"];
	ASSERT_EQ(elem_vt["type-kind"], "pntr");
	ASSERT_EQ(elem_vt["mutable"], true);
	ASSERT_EQ(elem_vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(elem_vt["base-type"]["type-name"], "Point");

	// body[1]: expr wpts[0] — arr-index, value-type pntr(struct(Point), mutable:true), elem-size 8
	const auto& idx = body[1]["body"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(idx["value-type"]["mutable"], true);
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// body.back(): free(wpts)
	const auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "wpts");
}

TEST(sa, embed_struct_arr_field_access)
{
	// [4]$Point pts; 10 -> pts[0].x; 20 -> pts[0].y; printf("%ld %ld\n", pts[0].x, pts[0].y);
	// Covers: resolveObjectChain / resolveStoreLocChain arr-index base case (embedded struct array)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/103_embed_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[1]: 10 -> pts[0].x — field-assign, ptr-expr = pts[0] (arr-index), offset 0
	const auto& fa_x = body[1];
	ASSERT_EQ(fa_x["stmt-type"], "field-assign");
	ASSERT_EQ(fa_x["offset"], 0);
	ASSERT_EQ(fa_x["value-type"]["type-name"], "int64");
	ASSERT_EQ(fa_x["value"]["value"], "10");
	const auto& pe_x = fa_x["ptr-expr"];
	ASSERT_EQ(pe_x["expr-type"], "arr-index");
	ASSERT_EQ(pe_x["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(pe_x["value-type"].contains("mutable"));
	ASSERT_EQ(pe_x["elem-size"]["value"], "16");

	// body[2]: 20 -> pts[0].y — field-assign, offset 8
	const auto& fa_y = body[2];
	ASSERT_EQ(fa_y["stmt-type"], "field-assign");
	ASSERT_EQ(fa_y["offset"], 8);
	ASSERT_EQ(fa_y["value"]["value"], "20");

	// body[3]: printf("%ld %ld\n", pts[0].x, pts[0].y) — field-access args, ptr-expr = arr-index
	const auto& args = body[3]["body"]["args"];
	ASSERT_EQ(args[1]["expr-type"], "field-access");
	ASSERT_EQ(args[1]["offset"], 0);
	ASSERT_EQ(args[1]["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(args[2]["expr-type"], "field-access");
	ASSERT_EQ(args[2]["offset"], 8);
	ASSERT_EQ(args[2]["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, owned_struct_arr_field_access)
{
	// [2]Point pts; 5 -> pts[0].x; printf("%ld\n", pts[0].x);
	// Covers: resolveObjectChain / resolveStoreLocChain arr-index base case (owned pointer array)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/104_owned_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[2]: 5 -> pts[0].x — field-assign, ptr-expr = pts[0] (arr-index), elem-size 8
	const auto& fa = body[2];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(pe["value-type"].contains("mutable"));
	ASSERT_EQ(pe["elem-size"]["value"], "8");

	// body[3]: printf("%ld\n", pts[0].x) — field-access, ptr-expr = arr-index
	const auto& fa_read = body[3]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, at_bang_struct_arr_field_write)
{
	// Point p; [4]@!Point wpts; p -> wpts[0]; 42 -> wpts[0].x; printf("%ld\n", p.x);
	// Covers: resolveStoreLocChain arr-index base case, mutable:true (write-through allowed)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/105_at_bang_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: 42 -> wpts[0].x — field-assign, ptr-expr = wpts[0] (arr-index, mutable:true)
	const auto& fa = body[3];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	ASSERT_EQ(fa["value"]["value"], "42");
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["value-type"]["mutable"], true);
}

TEST(sa, at_struct_arr_field_read)
{
	// Point p; [4]@Point rpts; p -> rpts[0]; printf("%ld\n", rpts[0].x);
	// Covers: resolveObjectChain arr-index base case, mutable:false (read-only, read allowed)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/106_at_struct_arr_field_read.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: printf("%ld\n", rpts[0].x) — field-access, ptr-expr = rpts[0] (arr-index, mutable:false)
	const auto& fa = body[3]["body"]["args"][1];
	ASSERT_EQ(fa["expr-type"], "field-access");
	ASSERT_EQ(fa["offset"], 0);
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["value-type"]["mutable"], false);
}

TEST(sa, embed_prim_arr_field)
{
	// type Buf { [4]$int64 data; }; Buf buf;
	// Covers: buildStructDef "arr" branch, embedded primitive-leaf case (embed-arr typeKind)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/107_embed_prim_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Buf buf; -> calloc(1, 32): confirms totalSize == 4*8 == 32 and useSimpleCalloc path
	// (hasOwnedStructFields stays false for embed-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "buf");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "32");
}

TEST(sa, embed_struct_arr_field)
{
	// type Point { int64 x; int64 y; }; type Polygon { [4]$Point pts; }; Polygon poly;
	// Covers: buildStructDef "arr" branch, embedded struct-leaf case (embed-arr typeKind, elemKind=="struct")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/108_embed_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Polygon poly; -> calloc(1, 64): confirms totalSize == 4*Point.totalSize(16) == 64
	// and useSimpleCalloc path (hasOwnedStructFields stays false for embed-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "poly");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "64");
}

TEST(sa, embed_ptr_arr_field)
{
	// type Point { int64 x; int64 y; }; type Ring { [4]@!Point nodes; }; Ring r;
	// Covers: buildStructDef "arr" branch, non-embedded pntr-wrapped base-type case
	// (embed-ptr-arr typeKind, isMutable==true, elemKind=="struct")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/109_embed_ptr_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Ring r; -> calloc(1, 32): confirms totalSize == 4*8 == 32 (4 pointer slots)
	// and useSimpleCalloc path (hasOwnedStructFields stays false for embed-ptr-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "32");
}

TEST(sa, embed_ptr_arr_field_readonly)
{
	// type Point { int64 x; int64 y; }; type Watch { [3]@Point observed; }; Watch w;
	// Covers: buildStructDef "arr" branch, embed-ptr-arr typeKind, isMutable==false
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/110_embed_ptr_arr_field_readonly.pa");
	ASSERT_TRUE(jout.is_object());

	// Watch w; -> calloc(1, 24): confirms totalSize == 3*8 == 24 (3 pointer slots)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "w");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "24");
}

TEST(sa, embed_ptr_arr_field_prim_leaf)
{
	// type Counter { [2]@!int64 slots; }; Counter c;
	// Covers: buildStructDef "arr" branch, embed-ptr-arr typeKind, elemKind=="prim"
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/111_embed_ptr_arr_field_prim_leaf.pa");
	ASSERT_TRUE(jout.is_object());

	// Counter c; -> calloc(1, 16): confirms totalSize == 2*8 == 16 (2 pointer slots)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "c");
	ASSERT_EQ(v["init"]["name"], "calloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "1");
	ASSERT_EQ(v["init"]["args"][1]["value"], "16");
}

TEST(sa, owned_prim_arr_field)
{
	// type Bucket { [3]int64 vals; }; Bucket b;
	// Covers: buildStructDef "arr" branch, non-embedded non-pntr-wrapped primitive-leaf
	// case (arr-ptr typeKind, hasOwnedArrayFields), useSimpleCalloc==false path,
	// recordAllocShape "owned-array-fields" output.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/112_owned_prim_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Bucket b; -> __pln_alloc_Bucket() (not calloc): hasOwnedArrayFields forces
	// the struct off the simple-calloc path even with no owned-struct fields.
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "b");
	ASSERT_EQ(v["init"]["name"], "__pln_alloc_Bucket");
	ASSERT_EQ(v["init"]["func-type"], "pln");
	ASSERT_TRUE(v["init"]["args"].empty());

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 1u);
	ASSERT_EQ(shapes[0]["shape-name"], "Bucket");
	ASSERT_EQ(shapes[0]["shape-kind"], "struct");
	ASSERT_EQ(shapes[0]["total-size"], 8);
	ASSERT_EQ(shapes[0]["owned-fields"].size(), 0u);

	const auto& fields = shapes[0]["fields"];
	ASSERT_EQ(fields[0]["name"], "vals");
	ASSERT_EQ(fields[0]["type-kind"], "arr-ptr");
	ASSERT_EQ(fields[0]["type-name"], "int64");
	ASSERT_EQ(fields[0]["count"], 3);
	ASSERT_EQ(fields[0]["elem-kind"], "prim");

	const auto& ownedArr = shapes[0]["owned-array-fields"];
	ASSERT_EQ(ownedArr.size(), 1u);
	ASSERT_EQ(ownedArr[0]["name"], "vals");
	ASSERT_EQ(ownedArr[0]["offset"], 0);
	ASSERT_EQ(ownedArr[0]["elem-kind"], "prim");
	ASSERT_EQ(ownedArr[0]["leaf-name"], "int64");
	ASSERT_EQ(ownedArr[0]["count"], 3);
}

TEST(sa, owned_struct_arr_field)
{
	// type Point { int64 x; int64 y; }; type Cluster { [4]Point pts; }; Cluster c;
	// Covers: buildStructDef "arr" branch, non-embedded case, struct-leaf owned
	// pointer array field (arr-ptr typeKind, elemKind=="struct"), recordAllocShape
	// registering an "arr-struct" shape (arr_Point) alongside the "struct" shapes.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/113_owned_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Cluster c; -> __pln_alloc_Cluster() (not calloc)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "c");
	ASSERT_EQ(v["init"]["name"], "__pln_alloc_Cluster");
	ASSERT_EQ(v["init"]["func-type"], "pln");
	ASSERT_TRUE(v["init"]["args"].empty());

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_GE(shapes.size(), 3u);

	auto point_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "struct" && s.value("shape-name","") == "Point";
	});
	ASSERT_NE(point_it, shapes.end());

	auto arr_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "arr-struct";
	});
	ASSERT_NE(arr_it, shapes.end());
	ASSERT_EQ((*arr_it)["shape-key"],   "arr_Point");
	ASSERT_EQ((*arr_it)["struct-name"], "Point");

	auto cluster_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "struct" && s.value("shape-name","") == "Cluster";
	});
	ASSERT_NE(cluster_it, shapes.end());
	ASSERT_EQ((*cluster_it)["total-size"], 8);
	ASSERT_EQ((*cluster_it)["owned-fields"].size(), 0u);

	const auto& fields = (*cluster_it)["fields"];
	ASSERT_EQ(fields[0]["name"], "pts");
	ASSERT_EQ(fields[0]["type-kind"], "arr-ptr");
	ASSERT_EQ(fields[0]["type-name"], "Point");
	ASSERT_EQ(fields[0]["count"], 4);
	ASSERT_EQ(fields[0]["elem-kind"], "struct");

	const auto& ownedArr = (*cluster_it)["owned-array-fields"];
	ASSERT_EQ(ownedArr.size(), 1u);
	ASSERT_EQ(ownedArr[0]["name"], "pts");
	ASSERT_EQ(ownedArr[0]["offset"], 0);
	ASSERT_EQ(ownedArr[0]["elem-kind"], "struct");
	ASSERT_EQ(ownedArr[0]["leaf-name"], "Point");
	ASSERT_EQ(ownedArr[0]["count"], 4);
}

TEST(sa, embed_prim_arr_struct_field_access)
{
	// type Buf { [4]$int64 data; }; Buf buf; 10->buf.data[0]; printf(buf.data[0],buf.data[1]);
	// Covers: sa_expr_arr_index new 1D primitive embedded array field branch (IT-2507).
	// The "array" (buf.data) must be addr-only (FieldAccessExpr computes ptr+offset,
	// not a load), and the arr-index result itself must be a plain scalar (not pntr)
	// so the element is actually loaded (DerefLoadIdx), not just addressed.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/114_embed_prim_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& target = body[1]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "prim");
	ASSERT_EQ(target["value-type"]["type-name"], "int64");
	ASSERT_EQ(target["elem-size"]["value"], "8");

	const auto& arr = target["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["embedded"], true);
	ASSERT_EQ(arr["value-type"]["stride"], 8);
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-name"], "int64");

	// printf args: buf.data[0], buf.data[1] — both scalar arr-index reads
	const auto& args = body[3]["body"]["args"];
	ASSERT_EQ(args[1]["value-type"]["type-kind"], "prim");
	ASSERT_EQ(args[2]["value-type"]["type-kind"], "prim");
}

TEST(sa, embed_struct_arr_struct_field_access)
{
	// type Point{...}; type Polygon { [4]$Point pts; }; Polygon poly;
	// 10->poly.pts[0].x; 20->poly.pts[0].y; printf(poly.pts[0].x, poly.pts[0].y);
	// Covers: FieldAccessExpr addr-only fix for embed-arr struct leaf (previously
	// segfaulted at runtime — DerefLoad was reading raw struct bytes as a pointer
	// instead of computing poly_ptr+offset). Field-assign/field-access chain through
	// the arr-index base (poly.pts[0]) must resolve the same way the existing
	// variable-level embedded struct array (embed_struct_arr_field_access) does.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/115_embed_struct_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& fa_x = body[1];
	ASSERT_EQ(fa_x["stmt-type"], "field-assign");
	ASSERT_EQ(fa_x["offset"], 0);
	const auto& pe_x = fa_x["ptr-expr"];
	ASSERT_EQ(pe_x["expr-type"], "arr-index");
	ASSERT_EQ(pe_x["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe_x["elem-size"]["value"], "16");

	const auto& arr = pe_x["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	ASSERT_EQ(arr["value-type"]["embedded"], true);
	ASSERT_EQ(arr["value-type"]["stride"], 16);
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "struct");

	const auto& fa_y = body[2];
	ASSERT_EQ(fa_y["offset"], 8);
}

TEST(sa, owned_prim_arr_struct_field_access)
{
	// type Bucket { [3]int64 vals; }; Bucket b; 1->b.vals[0]; printf(b.vals[0]);
	// Covers: fieldValueType arr-ptr primitive-leaf fix — base-type must be
	// {"type-kind":"prim","type-name":"int64"}, not "struct" (the previous default
	// branch tagged every arr-ptr leaf as a struct, which happened to be harmless
	// for codegen by luck but was schema-incorrect).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/116_owned_prim_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& target = body[1]["target"];
	ASSERT_EQ(target["value-type"]["type-kind"], "prim");
	ASSERT_EQ(target["value-type"]["type-name"], "int64");

	const auto& arr = target["array"];
	ASSERT_EQ(arr["addr-only"], false);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, owned_struct_arr_struct_field_access)
{
	// type Point{...}; type Cluster { [4]Point pts; }; Cluster c;
	// 5->c.pts[0].x; printf(c.pts[0].x);
	// Covers: fieldValueType arr-ptr struct-leaf fix — field must be double-wrapped
	// pntr(pntr(struct)) matching the variable-level owned struct array shape
	// (sa_owned_struct_arr_var_decl), so pts[i] yields pntr(struct) (a real stored
	// pointer, loaded via DerefLoadIdx) rather than the bare struct type the old
	// default branch produced (which broke resolveObjectChain's arr-index case).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/117_owned_struct_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& fa = body[1];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["elem-size"]["value"], "8");

	const auto& arr = pe["array"];
	ASSERT_EQ(arr["addr-only"], false);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["base-type"]["type-name"], "Point");
}

TEST(sa, embed_ptr_arr_struct_field_access)
{
	// type Point{...}; type Ring { [4]@!Point nodes; }; Point p; 99->p.x; Ring r;
	// p->r.nodes[0]; printf(r.nodes[0].x);
	// Covers: FieldAccessExpr addr-only fix for embed-ptr-arr (previously segfaulted
	// on the write `p -> r.nodes[0];` — DerefLoad on the freshly-calloc'd "nodes"
	// field read back 0 instead of computing r_ptr+offset, so the store target
	// address collapsed to NULL). Also covers write-through then read-back of the
	// stored pointer's field.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/118_embed_ptr_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: p -> r.nodes[0]; — arr-assign, target.array is addr-only (embed-ptr-arr)
	const auto& target = body[3]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(target["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(target["value-type"]["mutable"], true);

	const auto& arr = target["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["mutable"], true);
	ASSERT_EQ(arr["value-type"]["base-type"]["base-type"]["type-name"], "Point");

	// body[4]: printf("%ld\n", r.nodes[0].x) — field-access on the arr-index result
	const auto& fa_read = body[4]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, field_arr_readonly_ptr_slot)
{
	// type Point{...}; type Watch { [3]@Point observed; }; Point p; 99->p.x; Watch w;
	// p->w.observed[0]; printf(w.observed[0].x);
	// Covers: IT-2508 — same embed-ptr-arr shape as embed_ptr_arr_struct_field_access
	// (118) but with the non-mutable `@T` slot instead of `@!T`, confirming that
	// assignment into the pointer slot itself is unaffected by the mutable flag
	// (only write-through to the pointee's fields is restricted; see
	// write_readonly_arr_field_elem for the rejected case).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/119_field_arr_readonly_ptr_slot.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: p -> w.observed[0]; — arr-assign is allowed regardless of mutable flag
	const auto& target = body[3]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(target["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(target["value-type"]["mutable"], false);

	// body[4]: printf("%ld\n", w.observed[0].x) — read-through is allowed too
	const auto& fa_read = body[4]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(fa_read["ptr-expr"]["value-type"]["mutable"], false);
}

TEST(sa, void_ptr_compat)
{
	// IT-2605: pntr(void) parameters (e.g. memcpy's void* dest/src) must resolve
	// and accept pntr(T) arguments without SA throwing on the unknown "void" prim.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/123_void_ptr_compat.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][2]["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["name"], "memcpy");
	ASSERT_EQ(call["func-type"], "c");
}

TEST(sa, void_ptr_cmp)
{
	// IT-2605: comparing two pntr(void) results (e.g. memchr() == memchr()) goes
	// through the "cmp" expr's unguarded fromJson() calls, which previously threw.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/124_void_ptr_cmp.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& decl = jout["statements"][2];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["var-type"]["type-name"], "int32");
	ASSERT_EQ(v["init"]["expr-type"], "cmp");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "int32");
}

TEST(sa, const_decl_basic)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/125_const_decl_basic.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 1);

	// const-decl is consumed; first stmt is var-decl for x, init inlined to lit-int 100
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "x");
	ASSERT_EQ(v["init"]["expr-type"], "lit-int");
	ASSERT_EQ(v["init"]["value"], "100");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "int64");
}

TEST(sa, const_decl_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/126_const_decl_chain.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 1);
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["value"], "10");
}
