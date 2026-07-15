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
		// Unreachable: store_loc productions only ever yield var/arr-index/field.
		e = {{"expr-type", "not-impl"}}; // LCOV_EXCL_LINE
	}
	if (loc.contains("loc")) e["loc"] = loc["loc"];
	return e;
}
