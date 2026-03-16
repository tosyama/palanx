#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <boost/assert.hpp>
#include "PlnSemanticAnalyzer.h"

using namespace std;

static json promoteType(const json& a, const json& b) {
	static const map<string,int> rank = {
		{"int8",1},{"int16",2},{"int32",3},{"int64",4},
		{"uint8",1},{"uint16",2},{"uint32",3},{"uint64",4}
	};
	if (a["type-kind"] != "prim") return b;
	if (b["type-kind"] != "prim") return a;
	auto ia = rank.find(a["type-name"].get<string>());
	auto ib = rank.find(b["type-name"].get<string>());
	if (ia == rank.end()) return b;
	if (ib == rank.end()) return a;
	return ia->second >= ib->second ? a : b;
}

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
	sa["str-literals"] = json::array();
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
		} else if (stmt_type == "var-decl") {
			sa_var_decl(stmt);
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
	string expr_type = expr["expr-type"];

	if (expr_type == "lit-int") {
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "int64"}};

	} else if (expr_type == "lit-str") {
		string value = expr["value"];
		if (!strLiteralLabels.count(value)) {
			string label = ".str" + to_string(strLiteralLabels.size());
			strLiteralLabels[value] = label;
			sa["str-literals"].push_back({{"label", label}, {"value", value}});
		}
		sa_expr["label"] = strLiteralLabels[value];
		sa_expr.erase("value");
		sa_expr["value-type"] = {{"type-kind", "pntr"}, {"base-type", {{"type-kind", "prim"}, {"type-name", "uint8"}}}};

	} else if (expr_type == "id") {
		auto it = varSymbolTable.find(expr["name"].get<string>());
		if (it != varSymbolTable.end()) {
			sa_expr["var-type"]   = it->second;
			sa_expr["value-type"] = it->second;
		}

	} else if (expr_type == "add") {
		sa_expr["left"]  = sa_expression(expr["left"]);
		sa_expr["right"] = sa_expression(expr["right"]);
		sa_expr["value-type"] = promoteType(
			sa_expr["left"]["value-type"].get<json>(),
			sa_expr["right"]["value-type"].get<json>());

	} else if (expr_type == "call") {
		if (findCFunction(expr["name"])) {
			sa_expr["func-type"] = "c";
		}
		if (expr.contains("args")) {
			sa_expr["args"] = json::array();
			for (auto& arg : expr["args"]) {
				sa_expr["args"].push_back(sa_expression(arg));
			}
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

void PlnSemanticAnalyzer::sa_var_decl(const json &stmt)
{
	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt["vars"]) {
		string name = var["var-name"];
		varSymbolTable[name] = var["var-type"];

		json sa_var = var;
		if (var.contains("init")) {
			sa_var["init"] = sa_expression(var["init"]);
		}
		sa_stmt["vars"].push_back(sa_var);
	}
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

