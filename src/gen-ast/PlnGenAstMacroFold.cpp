/// cinclude macro-constant table and reference folding pass.
///
/// @file PlnGenAstMacroFold.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnGenAstMacroFold.h"

void registerMacroConstants(MacroTable& macros, const json& constants, const json& cincludeLoc)
{
	for (auto& c : constants) {
		string name = c["name"].get<string>();
		if (macros.count(name)) continue;  // first cinclude wins on duplicate macro names
		macros[name] = MacroEntry{ c["value"], c.value("value-type", json()), cincludeLoc };
	}
}

// True if `loc` (this id reference) is textually at or after `macroLoc`
// (the registering cinclude statement), comparing by begin (line, column).
static bool isAfterMacroLoc(const json& loc, const json& macroLoc)
{
	int refLine = loc[0].get<int>(), refCol = loc[1].get<int>();
	int macroLine = macroLoc[0].get<int>(), macroCol = macroLoc[1].get<int>();
	if (refLine != macroLine) return refLine > macroLine;
	return refCol > macroCol;
}

void foldMacroConstants(json& node, const MacroTable& macros)
{
	if (node.is_array()) {
		for (auto& e : node) foldMacroConstants(e, macros);
		return;
	}
	if (!node.is_object()) return;

	if (node.value("stmt-type", "") == "cinclude") return;

	if (node.value("expr-type", "") == "id") {
		auto it = macros.find(node["name"].get<string>());
		if (it != macros.end() && isAfterMacroLoc(node["loc"], it->second.loc)) {
			json loc = node["loc"];
			node = { {"expr-type", "lit-int"}, {"value", it->second.value}, {"loc", loc} };
			if (!it->second.valueType.is_null()) node["value-type"] = it->second.valueType;
		} // LCOV_EXCL_EXCEPTION_BR_LINE
		return;
	}

	for (auto& [key, val] : node.items()) {
		foldMacroConstants(val, macros);
	}
}
