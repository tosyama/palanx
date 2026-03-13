/// Deserialize sa.json into C++ AST nodes.
///
/// @file PlnDeserialize.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include "PlnNode.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

Module deserialize(const json& sa);
