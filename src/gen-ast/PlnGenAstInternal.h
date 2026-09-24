/// Inline helpers shared across PlnParser.yy grammar actions.
///
/// @file PlnGenAstInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
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
		// store_loc's tapple/func_call alternatives ("not-impl") reach here e.g.
		// via '@' store_loc when the operand is a call expression (`@f()`); SA
		// rejects the resulting addr-of/not-impl object as not addressable.
		e = {{"expr-type", "not-impl"}};
	}
	if (loc.contains("loc")) e["loc"] = loc["loc"];
	return e;
}

// Build a pointer type node. `mut` distinguishes @T (false) from @!T (true).
inline json pntrTypeExpr(json base, bool mut)
{
	return {{"type-kind","pntr"},{"mutable",mut},{"base-type",std::move(base)}};
}

// The base-type node for @void/@!void -- C's void*, spelled directly instead
// of via a primitive pointee like @!int8 (pntr(void) is bidirectionally
// compatible with any pntr(T), see PlnType.cpp typeCompat).
inline json voidTypeExpr()
{
	return json{{"type-kind","prim"},{"type-name","void"}};
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
	if (btk == "arr" && bt.value("specifier", "") == "raw" && !bt["size-expr"].is_null()
		&& bt["base-type"].value("type-kind", "") == "prim")
		return true;  // fixed-size multidim array (also covers embedded-struct arrays)
	return false;
}

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
}
