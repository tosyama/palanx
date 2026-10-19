/// Inline helpers shared across PlnParser.yy grammar actions.
///
/// @file PlnGenAstInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <filesystem>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using std::string;

// Convert a `kind`-tagged store_loc intermediate node into an `expr-type`-tagged
// expression node, recursively (needed since a store_loc base can itself be a
// field access, e.g. `s.f[0]`).
inline json storeLocToExpr(const json& loc)
{
	string kind = loc.value("kind", "");
	json e;
	if (kind == "var") {
		e = {{"expr-type", "id"}, {"name", loc["name"]}};
	} else if (kind == "arr-index") {
		e = {{"expr-type", "arr-index"},
		     {"array", loc["array"]}, {"index", loc["index"]}};
	} else if (kind == "field") {
		e = {{"expr-type", "field-access"},
		     {"object", storeLocToExpr(loc["base"])}, {"field", loc["field"]}};
	} else {
		// A call is only a whole '@' operand (addrOfObject); as the base of an
		// index or field, or as a store target, it stays unsupported.
		e = {{"expr-type", "not-impl"}};
	}
	if (loc.contains("loc")) e["loc"] = loc["loc"];
	return e;
} // LCOV_EXCL_EXCEPTION_BR_LINE

inline json addrOfObject(const json& loc)
{
	return loc.value("kind", "") == "call" ? loc["call"] : storeLocToExpr(loc);
}

inline bool isAssignExpr(const json& e)
{
	string et = e.value("expr-type", "");
	return et == "assign-expr" || et == "arr-assign-expr"
	    || et == "field-assign-expr" || et == "tapple-assign-expr";
}

inline json assignExprToStmt(const json& e)
{
	string et = e["expr-type"];
	json s;
	if (et == "assign-expr") {
		s = {{"stmt-type", "assign"}, {"name", e["name"]}, {"value", e["value"]}};
	} else if (et == "arr-assign-expr") {
		s = {{"stmt-type", "arr-assign"}, {"target", e["target"]}, {"value", e["value"]}};
	} else if (et == "field-assign-expr") {
		s = {{"stmt-type", "field-assign"},
		     {"object", storeLocToExpr(e["base"])}, {"field", e["field"]}, {"value", e["value"]}};
	} else {
		s = {{"stmt-type", "tapple-assign"}, {"targets", e["targets"]}, {"value", e["value"]}};
	}
	if (e.value("ownership-transfer", false)) s["ownership-transfer"] = true;
	if (e.contains("loc")) s["loc"] = e["loc"];
	return s;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The target an assignment expression stored, read back as an expression.
inline json assignTargetExpr(const json& v)
{
	string vt = v["expr-type"];
	json readBack;
	if (vt == "assign-expr")
		readBack = {{"expr-type", "id"}, {"name", v["name"]}};
	else if (vt == "arr-assign-expr")
		readBack = v["target"];
	else if (vt == "field-assign-expr")
		readBack = {{"expr-type", "field-access"}, {"object", storeLocToExpr(v["base"])}, {"field", v["field"]}};
	else
		readBack = {{"expr-type", "not-impl"}};
	if (!readBack.contains("loc") && v.contains("loc")) readBack["loc"] = v["loc"];
	return readBack;
} // LCOV_EXCL_EXCEPTION_BR_LINE

inline std::vector<json> lowerAssignChain(const json& e);

// `x -> v -> f()` is `x -> v; v -> f()`: an assignment feeding a chained call
// is moved out in front as statements, and the call reads its target back.
inline void hoistChainAssigns(json& e, std::vector<json>& stmts)
{
	if (e.value("expr-type", "") != "chain-call")
		return;
	json& v = e["value"];
	if (v.value("expr-type", "") == "chain-call") {
		hoistChainAssigns(v, stmts);
	} else if (isAssignExpr(v)) {
		for (auto& s : lowerAssignChain(v))
			stmts.push_back(s);
		v = assignTargetExpr(v);
	}
}

// `v -> t1 -> t2` is `v -> t1; t1 -> t2;`: each later step reads back the
// target the previous step stored, so SA only ever sees plain assignments.
inline std::vector<json> lowerAssignChain(const json& e)
{
	std::vector<json> stmts;
	json step = e;
	hoistChainAssigns(step["value"], stmts);
	const json& v = step["value"];
	if (!isAssignExpr(v)) {
		stmts.push_back(assignExprToStmt(step));
		return stmts;
	}

	json notImpl = {{"stmt-type", "not-impl"}};
	if (e.contains("loc")) notImpl["loc"] = e["loc"];
	if (v["expr-type"] == "tapple-assign-expr")
		return {notImpl};
	for (auto& s : lowerAssignChain(v))
		stmts.push_back(s);
	if (stmts.back()["stmt-type"] == "not-impl")
		return {notImpl};

	step["value"] = assignTargetExpr(v);
	stmts.push_back(assignExprToStmt(step));
	return stmts;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The statements an assignment or a chained call written as a statement
// expands to.
inline std::vector<json> lowerExprStmt(const json& e)
{
	if (isAssignExpr(e))
		return lowerAssignChain(e);
	std::vector<json> stmts;
	json call = e;
	hoistChainAssigns(call, stmts);
	json stmt = {{"stmt-type", "expr"}, {"body", std::move(call)}};
	if (e.contains("loc")) stmt["loc"] = e["loc"];
	stmts.push_back(std::move(stmt));
	return stmts;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Build a pointer type node. `mut` distinguishes @T (false) from @!T (true).
inline json pntrTypeExpr(json base, bool mut)
{
	return {{"type-kind","pntr"},{"mutable",mut},{"base-type",std::move(base)}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The base-type node for @void/@!void -- C's void*, spelled directly instead
// of via a primitive pointee like @!int8 (pntr(void) is bidirectionally
// compatible with any pntr(T), see PlnType.cpp typeCompat).
inline json voidTypeExpr()
{
	return json{{"type-kind","prim"},{"type-name","void"}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The pointee of @func(...): the same shape c2ast gives a C function type, so
// a native function pointer and a cinclude'd one reach SA identically.
inline json funcTypeExpr(std::vector<json> params, json ret)
{
	return {{"type-kind","func"},{"parameters",std::move(params)},{"ret-type",std::move(ret)}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Whether a type_expr result has no array level other than a plain '[n]'/'[]'
// one ('[#n]'/'[+n]' are not implemented).
inline bool isImplementedTypeExpr(const json& t)
{
	if (t.value("type-kind", "") == "arr" && t.value("specifier", "") != "raw")
		return false;
	return !t.contains("base-type") || isImplementedTypeExpr(t["base-type"]);
}

// Whether a type_expr result is one gen-ast currently knows how to carry as
// a declared variable's type (a bare, uninitialized var_declaration). Shared
// by var_declaration and return_def (ARROW type_expr) so a bare-return-type
// function stays representable in exactly the cases a same-typed local
// variable already is -- a named return (ARROW var_declarations) gets this
// for free by going through var_declaration itself.
inline bool isDeclarableVarType(const json& t)
{
	string tk = t.value("type-kind", "");
	if (tk == "prim" || tk == "pntr")
		return true;
	if (tk != "arr" || t.value("specifier", "") != "raw")
		return false;
	if (t["size-expr"].is_null())
		return true;  // unsized raw array
	const json& bt = t["base-type"];
	string btk = bt.value("type-kind", "");
	if (btk == "prim")
		return true;  // fixed-size array of prim
	if (btk == "pntr" && bt.value("mutable", false) == true && bt.contains("base-type")) {
		const json& ibt = bt["base-type"];
		if (ibt.value("type-kind", "") == "arr" && ibt.value("specifier", "") == "raw"
			&& ibt.contains("size-expr") && ibt["size-expr"].is_null())
			return true;  // array of @! slots into an unsized array (owning-slot array)
	}
	if (btk == "pntr" && bt["base-type"].value("type-kind", "") == "prim")
		return true;  // array of struct-record pointers (@T elements)
	if (btk == "pntr" && bt["base-type"].value("type-kind", "") == "func")
		return true;  // array of function pointers
	if (btk == "arr" && bt.value("specifier", "") == "raw" && !bt["size-expr"].is_null()
		&& bt["base-type"].value("type-kind", "") == "prim")
		return true;  // fixed-size multidim array (also covers embedded-struct arrays)
	return false;
}

// A typed lit-int, like a macro-folded constant: SA keeps its bool type
// instead of retyping it from context, so no separate literal kind is needed.
inline json boolLiteral(const char* value)
{
	return {{"expr-type", "lit-int"}, {"value", value},
	        {"value-type", {{"type-kind", "prim"}, {"type-name", "bool"}}}}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Unary minus on an untyped integer literal becomes one negative literal, so
// SA's range check sees `-128` as a single value (an int8 `neg(128)` operand
// would be out of range). A typed lit-int (macro-folded) keeps its `neg`.
inline json negateExpr(json operand)
{
	if (operand.value("expr-type", "") == "lit-int" && !operand.contains("value-type")) {
		string v = operand["value"];
		operand["value"] = (v[0] == '-') ? v.substr(1) : "-" + v;
		return operand;
	}
	return {{"expr-type", "neg"}, {"operand", std::move(operand)}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A `link` clause entry ending in ".o" names an object file rather than a -l
// library. Resolved here, against the source file's directory as a local
// cinclude header is, so everything downstream sees only absolute paths.
inline bool isLinkObjName(const string& name)
{
	return name.size() > 2 && name.compare(name.size() - 2, 2, ".o") == 0;
}

inline string resolveLinkObjPath(const string& name, const string& inputFile)
{
	std::filesystem::path p = name;
	if (!p.is_absolute())
		p = std::filesystem::absolute(std::filesystem::path(inputFile).parent_path() / p);
	return p.lexically_normal().string();
}
