/// semantic analyzer test helpers
///
/// @file saTestUtil.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

json run_sa(const std::string& pa_file);
json written(const json& sa);
std::string funcName(const json& linkName);
const json& tempInit(const json& arg);
std::string tempRelease(const json& arg);
void genLibSaImport();
void genLibSaTypes();
