/// palan-c2ast invocation for cinclude'd C headers.
///
/// @file		PlnGenAstC2Ast.h
/// @copyright	2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

/// Run palan-c2ast on one cinclude'd header and return its AST JSON.
///
/// path is raw token text with only its delimiters stripped (PlnLexer.ll's
/// PATH/INCLUDE_FILE patterns), so it must never be handed to a shell.
///
/// Throws std::runtime_error (E_C2AstFailed) if c2ast cannot be started,
/// exits non-zero, or emits output that is not valid JSON.
nlohmann::json execute_c2ast(const std::string& path_type,
                             const std::string& path,
                             const std::string& base_dir);
