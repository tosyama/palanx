/// Deserialize sa.json into C++ AST nodes.
///
/// @file deserialize.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include "plnnode.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

Module deserialize(const json& sa);
