/// Palan Semantic Analyzer
///
/// @file PlnSemanticAnalyzer.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <string>
#include <map>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"

using namespace std;

using json = nlohmann::json;

class PlnSemanticAnalyzer {
	string basePath;
	string astFileName;
	string c2astPath;
	string inputFilePath;
	json sa;
	PlnTypeRegistry registry_;
	vector<map<string, json>> cFunctionScopes;
	map<string, string> strLiteralLabels;  // value -> label
	map<string, json> varSymbolTable;      // var name -> var-type
	map<string, json> plnFuncTable_;       // Palan function table (name -> AST func node)
	const json*       currentFunc_ = nullptr;  // null = _start level

	void pushScope();
	void popScope();
	const json* findCFunction(const string& name) const;

	json sa_statements(const json& stmts);
	void sa_import(const json &stmt);
	void sa_cinclude(const json &stmt);
	json sa_expression(const json &expr, const PlnType* expectedType = nullptr);
	json sa_expression_stmt(const json& stmt);
	json sa_var_decl(const json& stmt);
	void sa_functions(const json& funcs);
	void sa_function(const json& funcDef);
	json sa_assign_stmt(const json& stmt);
	json sa_return_stmt(const json& stmt);

public:
	PlnSemanticAnalyzer(string base_path, string ast_filename, string c2ast_path);
	void analysis(const json &ast);
	const json& result();
};
