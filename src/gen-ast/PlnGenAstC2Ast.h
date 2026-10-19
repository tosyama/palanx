/// palan-c2ast invocation for cinclude'd C headers.
///
/// @file PlnGenAstC2Ast.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

/// Run palan-c2ast on one cinclude'd header and return its AST JSON.
///
/// path is raw token text with only its delimiters stripped (PlnLexer.ll's
/// PATH/INCLUDE_FILE patterns), so it must never be handed to a shell.
///
/// macro_state is the previous cinclude's "macro-state" output, so this
/// header is preprocessed with the macros in effect at its cinclude, as in
/// one C translation unit; empty for the first cinclude.
///
/// Throws std::runtime_error (E_C2AstFailed) if c2ast cannot be started,
/// exits non-zero, or emits output that is not valid JSON.
nlohmann::json execute_c2ast(const std::string& path_type,
                             const std::string& path,
                             const std::string& base_dir,
                             const std::string& macro_state);
