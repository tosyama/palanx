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
	if (kind == "call") return loc["call"];
	if (kind == "expr") return loc["expr"];
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
		e = {{"expr-type", "not-impl"}};
	}
	if (loc.contains("loc")) e["loc"] = loc["loc"];
	return e;
} // LCOV_EXCL_EXCEPTION_BR_LINE

inline bool isStoreStmt(const json& e)
{
	return e.contains("stmt-type");
}

// Multiple values stored by a tapple have no single target to read back.
inline bool chainsThroughTapple(const json& value)
{
	string t = value.value("stmt-type", "");
	return t == "tapple-assign" || t == "not-impl";
}

// `value -> loc` (`->>` when transfer) as its statement. In a chain the value
// is itself a store statement, whose target SA reads back.
inline json storeStmt(json loc, json value, bool transfer)
{
	if (chainsThroughTapple(value))
		return {{"stmt-type", "not-impl"}};
	string kind = loc["kind"];
	json s;
	if (kind == "var") {
		s = {{"stmt-type", "assign"}, {"name", move(loc["name"])}};
	} else if (kind == "arr-index") {
		json target = {{"expr-type", "arr-index"}, {"array", move(loc["array"])}, {"index", move(loc["index"])}};
		target["loc"] = loc["loc"];
		s = {{"stmt-type", "arr-assign"}, {"target", move(target)}};
	} else if (kind == "field") {
		s = {{"stmt-type", "field-assign"}, {"object", storeLocToExpr(loc["base"])}, {"field", move(loc["field"])}};
	} else {
		return {{"expr-type", "not-impl"}};
	}
	s["value"] = move(value);
	if (transfer) s["ownership-transfer"] = true;
	return s;
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

// A variable-length array, which no function signature takes yet.
inline bool isVarrType(const json& t)
{
	return t.value("type-kind", "") == "arr" && t.value("specifier", "") == "variable";
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
	if (isVarrType(t))
		return t["base-type"].value("type-kind", "") == "prim";
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

// `$T v;` / `$[n]T a;` put a local's storage in the stack frame. Whether the
// base is a struct or an array is only known in SA, so any declarable base is
// carried here and SA rejects the rest with a real message.
inline bool isFrameVarType(const json& t)
{
	return t.value("type-kind", "") == "embed" && isDeclarableVarType(t["base-type"]);
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
