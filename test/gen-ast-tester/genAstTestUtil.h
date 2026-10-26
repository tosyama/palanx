/// gen-ast test helpers
///
/// @file genAstTestUtil.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

bool checkerr(std::string &output);
