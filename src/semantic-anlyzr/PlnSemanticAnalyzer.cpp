#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include <boost/assert.hpp>
#include "PlnSemanticAnalyzer.h"

using namespace std;

static json wrapConvert(const json& expr, const json& to_type) {
	return {
		{"expr-type",  "convert"},
		{"value-type", to_type},
		{"from-type",  expr["value-type"]},
		{"src",        expr}
	};
}

static const PlnType* variadicPromote(const PlnType* t, PlnTypeRegistry& reg)
{
	if (t->kind != PlnType::Kind::Prim) return t;
	const auto* p = static_cast<const PrimType*>(t);
	using N = PrimType::Name;
	switch (p->name) {
		case N::Int8:  case N::Int16:  return reg.prim(N::Int32);
		case N::Uint8: case N::Uint16: return reg.prim(N::Uint32);
		default: return t;
	}
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

json PlnSemanticAnalyzer::sa_expression(const json &expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	if (expr_type == "lit-int") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim)
			sa_expr["value-type"] = registry_.toJson(expectedType);
		else
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Int64));

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
		json left  = sa_expression(expr["left"]);
		json right = sa_expression(expr["right"]);
		const PlnType* leftType  = registry_.fromJson(left["value-type"]);
		const PlnType* rightType = registry_.fromJson(right["value-type"]);
		const PlnType* promoted;
		if (typeCompat(leftType, rightType, registry_) == TypeCompat::ImplicitWiden) {
			promoted = rightType;
			left = wrapConvert(left, registry_.toJson(promoted));
		} else if (typeCompat(rightType, leftType, registry_) == TypeCompat::ImplicitWiden) {
			promoted = leftType;
			right = wrapConvert(right, registry_.toJson(promoted));
		} else {
			promoted = leftType;
		}
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = registry_.toJson(promoted);

	} else if (expr_type == "call") {
		const json* cfunc = findCFunction(expr["name"]);
		if (cfunc) {
			sa_expr["func-type"] = "c";
			if (cfunc->contains("ret-type"))
				sa_expr["value-type"] = (*cfunc)["ret-type"];
		}
		if (expr.contains("args")) {
			sa_expr["args"] = json::array();

			// Determine fixed param count and variadic flag
			size_t fixedCount = 0;
			bool isVariadic = false;
			if (cfunc && cfunc->contains("parameters")) {
				for (auto& p : (*cfunc)["parameters"]) {
					if (p.contains("name") && p["name"] == "...") isVariadic = true;
					else fixedCount++;
				}
			}

			size_t argIdx = 0;
			for (auto& arg : expr["args"]) {
				json saArg = sa_expression(arg);
				if (saArg.contains("value-type")) {
					const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
					if (cfunc && cfunc->contains("parameters") && argIdx < fixedCount) {
						// Fixed parameter: type check
						const PlnType* toType = registry_.fromJson(
							(*cfunc)["parameters"][argIdx]["var-type"]);
						TypeCompat compat = typeCompat(fromType, toType, registry_);
						if (compat == TypeCompat::ImplicitWiden)
							saArg = wrapConvert(saArg, registry_.toJson(toType));
						// else: Identical or Incompatible (e.g. ptr types) — pass through as-is
					} else if (isVariadic) {
						// Variadic argument: caller promotion
						const PlnType* promoted = variadicPromote(fromType, registry_);
						if (promoted != fromType)
							saArg = wrapConvert(saArg, registry_.toJson(promoted));
					}
				}
				sa_expr["args"].push_back(saArg);
				argIdx++;
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
			const PlnType* toType = registry_.fromJson(var["var-type"]);
			json init = sa_expression(var["init"], toType);
			const json& var_type = var["var-type"];
			if (init.contains("value-type") && init["value-type"] != var_type) {
				const PlnType* fromType = registry_.fromJson(init["value-type"]);
				TypeCompat compat = typeCompat(fromType, toType, registry_);
				if (compat == TypeCompat::ImplicitWiden) {
					init = wrapConvert(init, var_type);
				} else if (compat == TypeCompat::ExplicitCast) {
					string et = init["expr-type"];
					BOOST_ASSERT(et == "lit-int" || et == "lit-uint");  // narrowing only from literal
				}
				// Incompatible: no action (ptr types pass through as-is)
			}
			sa_var["init"] = init;
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

