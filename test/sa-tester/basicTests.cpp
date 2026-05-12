#include <gtest/gtest.h>
#include <fstream>
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
