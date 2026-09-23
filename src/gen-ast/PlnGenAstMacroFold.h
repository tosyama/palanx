/// cinclude macro-constant table and reference folding pass.
///
/// @file PlnGenAstMacroFold.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using std::string;
using std::map;

// One registered `#define NAME value` macro constant: its already-typed
// literal value/value-type (as c2ast reported them) and the `loc` of the
// cinclude statement that registered it, used to decide whether a given
// reference appears textually after this registration.
struct MacroEntry {
	json value;
	json valueType;
	json loc;
};

using MacroTable = map<string, MacroEntry>;

// Register `constants` (c2ast's per-cinclude array of {name, value, value-type})
// into `macros`, tagging each entry with `cincludeLoc`. First registration for
// a given name wins -- a later cinclude with the same macro name is ignored.
void registerMacroConstants(MacroTable& macros, const json& constants, const json& cincludeLoc);

// Recursively replace `id` expression nodes in `node` whose name matches a
// registered macro constant, and whose reference `loc` is textually after
// that macro's cinclude `loc`, with a typed `lit-int` node carrying the
// macro's value/value-type. Does not descend into `cinclude` statement
// subtrees (their `functions`/`structs`/etc. are C declarations, not Palan
// references). Leaves earlier (pre-cinclude) references as `id` untouched.
void foldMacroConstants(json& node, const MacroTable& macros);
