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

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] != "add") continue;
				// addition: id nodes present
				ASSERT_EQ(arg["left"]["expr-type"],  "id");
				ASSERT_EQ(arg["right"]["expr-type"], "id");
				// value_type_on_expressions: value-type annotated
				ASSERT_EQ(arg["value-type"]["type-name"],          "int64");
				ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
				ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
				found = true;
			}
		}
	}
	ASSERT_TRUE(found);
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
