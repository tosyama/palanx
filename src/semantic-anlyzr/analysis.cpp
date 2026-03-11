#include <iostream>
#include <fstream>
#include <filesystem>
#include <boost/assert.hpp>
#include "analysis.h"

using namespace std;

PlnSemanticAnalyzer::PlnSemanticAnalyzer(string base_path, string ast_filename, string c2ast_path)
	: basePath(base_path), astFileName(ast_filename), c2astPath(c2ast_path), inputFilePath("")
{
}

void PlnSemanticAnalyzer::pushScope()
{
	cFunctionScopes.push_back({});
}

void PlnSemanticAnalyzer::popScope()
{
	cFunctionScopes.pop_back();
}

const json* PlnSemanticAnalyzer::findCFunction(const string& name) const
{
	for (auto it = cFunctionScopes.rbegin(); it != cFunctionScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

void PlnSemanticAnalyzer::analysis(const json &ast)
{
	this->inputFilePath = ast["original"];
	sa["original"] = ast["original"];
	pushScope();
	sa_statements(ast["ast"]["statements"]);
	popScope();
}

const json& PlnSemanticAnalyzer::result()
{
	return sa;
}

void PlnSemanticAnalyzer::sa_statements(const json &stmts)
{
	for (auto& stmt: stmts) {
		string stmt_type = stmt["stmt-type"];
		if (stmt_type == "import") {
			sa_import(stmt);
		} else if (stmt_type == "cinclude") {
			sa_cinclude(stmt);
		} else if (stmt_type == "expr") {
			sa_expression_stmt(stmt);
		} else if (stmt_type == "not-impl") {
			sa["statements"].push_back(stmt);
		} else {
			BOOST_ASSERT(false);
		}
	}
}

json PlnSemanticAnalyzer::sa_expression(const json &expr)
{
	json sa_expr = expr;
	if (expr["expr-type"] == "call") {
		string name = expr["name"];
		if (findCFunction(name)) {
			sa_expr["func-type"] = "c";
		}
	}
	return sa_expr;
}

void PlnSemanticAnalyzer::sa_expression_stmt(const json &stmt)
{
	json sa_stmt = {
		{"stmt-type", "expr"},
		{"body", sa_expression(stmt["body"])}
	};
	sa["statements"].push_back(sa_stmt);
}

void PlnSemanticAnalyzer::sa_cinclude(const json &stmt)
{
	if (stmt.contains("functions")) {
		for (auto& f : stmt["functions"]) {
			cFunctionScopes.back()[f["name"]] = f;
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
	}
}

