/// Palan Semantic Analyzer
///
/// @file analysis.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using namespace std;

using json = nlohmann::json;

class PlnSemanticAnalyzer {
	string basePath;
	string c2astPath;
	json sa;
	
	void sa_statements(const json &stmts);
	void sa_import(const json &stmt);

public:
	PlnSemanticAnalyzer(string base_path, string c2ast_path);
	void analysis(const json &ast);
	json& result();
};
