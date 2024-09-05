#include <iostream>
#include <fstream>
#include <filesystem>
#include <boost/assert.hpp>
#include "analysis.h"

using namespace std;

PlnSemanticAnalyzer::PlnSemanticAnalyzer(string base_path, string c2ast_path)
	: basePath(base_path), c2astPath(c2ast_path)
{
}

void PlnSemanticAnalyzer::analysis(const json &ast)
{
	sa_statements(ast["ast"]["statements"]);	
}

void PlnSemanticAnalyzer::sa_statements(const json &stmts)
{
	for (auto& stmt: stmts) {
		string stmt_type = stmt["stmt-type"];
		if (stmt_type == "import") {
			sa_import(stmt);
		} else if (stmt_type == "not-impl") {
		} else {
			BOOST_ASSERT(false);
		}
	}
}

bool is_absolute(filesystem::path &path)
{
	if (path.string()[0] == '/') {
		return true;
	}
	return false;
}

void PlnSemanticAnalyzer::sa_import(const json &stmt)
{
	filesystem::path imp_path = stmt["path"];	
	if (stmt["path-type"] == "src") {
		if (!is_absolute(imp_path)) {
			imp_path = basePath + '/' + imp_path.string();
		}
	}

	if (imp_path.string().ends_with(".pa")) {
		imp_path += ".ast.json";

		ifstream astfile(imp_path.string());
		json ast = json::parse(astfile);
		if (!ast["export"].is_null()) {
			BOOST_ASSERT(false);
		}

	} else if (imp_path.string().ends_with(".h")) {
		if (stmt["path-type"] == "src") {

		} else if (stmt["path-type"] == "inc") {
			string c2astcmd = c2astPath + " -s " + imp_path.string();
			int ret = system(c2astcmd.c_str());
			// TODO: Add include path?

		} else {
			BOOST_ASSERT(false);
		}

	}
}

