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
	sa["original"]      = ast["original"];
	sa["str-literals"]  = json::array();
	sa["functions"]     = json::array();
	pushScope();
	// 1. Pre-register Palan functions so calls can resolve them
	if (ast["ast"].contains("functions"))
		for (auto& f : ast["ast"]["functions"])
			plnFuncTable_[f["name"]] = f;
	// 2. Process each function body
	if (ast["ast"].contains("functions"))
		sa_functions(ast["ast"]["functions"]);
	// 3. Process top-level statements
	sa["statements"] = sa_statements(ast["ast"]["statements"]);
	popScope();
}

const json& PlnSemanticAnalyzer::result()
{
	return sa;
}

json PlnSemanticAnalyzer::sa_statements(const json& stmts)
{
	json result = json::array();
	for (auto& stmt : stmts) {
		string t = stmt["stmt-type"];
		if      (t == "import")   sa_import(stmt);
		else if (t == "cinclude") sa_cinclude(stmt);
		else if (t == "expr")     result.push_back(sa_expression_stmt(stmt));
		else if (t == "var-decl") result.push_back(sa_var_decl(stmt));
		else if (t == "assign")   result.push_back(sa_assign_stmt(stmt));
		else if (t == "return")   result.push_back(sa_return_stmt(stmt));
		else if (t == "not-impl") result.push_back(stmt);
		else BOOST_ASSERT(false);
	}
	return result;
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

	} else if (expr_type == "cast") {
		const PlnType* target  = registry_.fromJson(expr["target-type"]);
		json src               = sa_expression(expr["src"]);
		const PlnType* srcType = registry_.fromJson(src["value-type"]);
		TypeCompat compat      = typeCompat(srcType, target, registry_);
		if (compat == TypeCompat::Identical) {
			return src;
		} else if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast) {
			return wrapConvert(src, registry_.toJson(target));
		} else {
			BOOST_ASSERT(false);  // Incompatible
		}

	} else if (expr_type == "call") {
		const json* funcParams = nullptr;
		bool isVariadic = false;

		const json* cfunc = findCFunction(expr["name"]);
		if (cfunc) {
			sa_expr["func-type"] = "c";
			if (cfunc->contains("ret-type"))
				sa_expr["value-type"] = (*cfunc)["ret-type"];
			if (cfunc->contains("parameters")) {
				funcParams = &(*cfunc)["parameters"];
				for (auto& p : *funcParams)
					if (p.value("name", "") == "...") { isVariadic = true; break; }
			}
		} else {
			auto pit = plnFuncTable_.find(expr["name"].get<string>());
			if (pit != plnFuncTable_.end()) {
				const json& pFunc = pit->second;
				sa_expr["func-type"] = "palan";
				if (pFunc.contains("ret-type"))
					sa_expr["value-type"] = pFunc["ret-type"];
				if (pFunc.contains("parameters"))
					funcParams = &pFunc["parameters"];
			}
		}

		if (expr.contains("args")) {
			sa_expr["args"] = json::array();
			size_t fixedCount = funcParams ? (funcParams->size() - (isVariadic ? 1 : 0)) : 0;
			size_t argIdx = 0;
			for (auto& arg : expr["args"]) {
				json saArg = sa_expression(arg);
				if (saArg.contains("value-type")) {
					const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
					if (funcParams && argIdx < fixedCount) {
						const PlnType* toType = registry_.fromJson((*funcParams)[argIdx]["var-type"]);
						if (typeCompat(fromType, toType, registry_) == TypeCompat::ImplicitWiden)
							saArg = wrapConvert(saArg, registry_.toJson(toType));
					} else if (isVariadic) {
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

json PlnSemanticAnalyzer::sa_expression_stmt(const json& stmt)
{
	return {
		{"stmt-type", "expr"},
		{"body", sa_expression(stmt["body"])}
	};
}

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt["vars"]) {
		string name = var["name"];
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
	return sa_stmt;
}

void PlnSemanticAnalyzer::sa_functions(const json& funcs)
{
	for (auto& f : funcs) sa_function(f);
}

void PlnSemanticAnalyzer::sa_function(const json& funcDef)
{
	// Save top-level var table and use function scope instead
	auto savedVars = varSymbolTable;
	varSymbolTable.clear();
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			varSymbolTable[p["name"]] = p["var-type"];
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
			varSymbolTable[r["name"]] = r["var-type"];

	currentFunc_ = &plnFuncTable_[funcDef["name"]];

	json saFunc    = funcDef;
	saFunc["body"] = sa_statements(funcDef["body"]);

	varSymbolTable = savedVars;
	currentFunc_   = nullptr;

	sa["functions"].push_back(saFunc);
}

json PlnSemanticAnalyzer::sa_assign_stmt(const json& stmt)
{
	string name = stmt["name"];
	auto it = varSymbolTable.find(name);
	BOOST_ASSERT(it != varSymbolTable.end());
	const PlnType* toType = registry_.fromJson(it->second);
	json value = sa_expression(stmt["value"], toType);
	if (value.contains("value-type")) {
		const PlnType* fromType = registry_.fromJson(value["value-type"]);
		if (typeCompat(fromType, toType, registry_) == TypeCompat::ImplicitWiden)
			value = wrapConvert(value, registry_.toJson(toType));
	}
	return {{"stmt-type", "assign"}, {"name", name}, {"value", value}};
}

json PlnSemanticAnalyzer::sa_return_stmt(const json& stmt)
{
	BOOST_ASSERT(currentFunc_ != nullptr);
	bool hasRets   = currentFunc_->contains("rets");
	bool hasRetType = currentFunc_->contains("ret-type");
	bool hasValues = stmt.contains("values");

	if (hasRets) {
		// Multiple return values: bare return only
		BOOST_ASSERT(!hasValues);
		return {{"stmt-type", "return"}};
	}
	if (hasRetType) {
		// Single return value: exactly one expression required
		BOOST_ASSERT(hasValues && stmt["values"].size() == 1);
		const PlnType* toType = registry_.fromJson((*currentFunc_)["ret-type"]);
		json value = sa_expression(stmt["values"][0], toType);
		if (value.contains("value-type")) {
			const PlnType* fromType = registry_.fromJson(value["value-type"]);
			if (typeCompat(fromType, toType, registry_) == TypeCompat::ImplicitWiden)
				value = wrapConvert(value, registry_.toJson(toType));
		}
		return {{"stmt-type", "return"}, {"values", json::array({value})}};
	}
	// Void function: bare return only
	BOOST_ASSERT(!hasValues);
	return {{"stmt-type", "return"}};
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

