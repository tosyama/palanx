/// Palan Semantic Analyzer
///
/// @file analysis.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <string>
#include <map>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using namespace std;

using json = nlohmann::json;

class PlnSemanticAnalyzer {
	string basePath;
	string astFileName;
	string c2astPath;
	string inputFilePath;
	json sa;
	vector<map<string, json>> cFunctionScopes;
	map<string, string> strLiteralLabels;  // value -> label

	void pushScope();
	void popScope();
	const json* findCFunction(const string& name) const;

	void sa_statements(const json &stmts);
	void sa_import(const json &stmt);
	void sa_cinclude(const json &stmt);
	json sa_expression(const json &expr);
	void sa_expression_stmt(const json &stmt);

public:
	PlnSemanticAnalyzer(string base_path, string ast_filename, string c2ast_path);
	void analysis(const json &ast);
	const json& result();
};
