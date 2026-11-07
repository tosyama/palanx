/// Writes a module's declarations as an interface (.pai).
///
/// @file PlnInterface.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

// ast: a module's AST, with macro constants folded. Throws std::runtime_error
// on a declaration Palan source cannot state, such as an unimplemented parameter.
std::string writeInterface(const nlohmann::json& ast);
