#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"

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

void PlnSemanticAnalyzer::enterScope()
{
	varScopes.push_back({});
	cFuncScopes.push_back({});
	plnFuncScopes.push_back({});
	importScopes.push_back({});
}

void PlnSemanticAnalyzer::leaveScope()
{
	varScopes.pop_back();
	cFuncScopes.pop_back();
	plnFuncScopes.pop_back();
	importScopes.pop_back();
}

void PlnSemanticAnalyzer::declareVar(const string& name, const json& type)
{
	for (auto& scope : varScopes)
		if (scope.count(name)) {
			cerr << PlnSaMessage::getMessage(E_DuplicateVarDecl, name) << endl;
			exit(1);
		}
	varScopes.back()[name] = type;
}

const json* PlnSemanticAnalyzer::findVar(const string& name) const
{
	for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

void PlnSemanticAnalyzer::registerCFunc(const string& name, const json& def)
{
	cFuncScopes.back()[name] = def;  // shadow allowed
}

const json* PlnSemanticAnalyzer::findCFunc(const string& name) const
{
	for (auto it = cFuncScopes.rbegin(); it != cFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

void PlnSemanticAnalyzer::registerPlnFunc(const string& name, const json& def)
{
	for (auto& scope : plnFuncScopes)
		if (scope.count(name)) {
			cerr << PlnSaMessage::getMessage(E_DuplicateFuncDef, name) << endl;
			exit(1);
		}
	plnFuncScopes.back()[name] = def;
}

const json* PlnSemanticAnalyzer::findPlnFunc(const string& name) const
{
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) {
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
	enterScope();
	// 1. Pre-register Palan functions so calls can resolve them
	if (ast["ast"].contains("functions"))
		for (auto& f : ast["ast"]["functions"]) {
			// Single named return: synthesize ret-type for call resolution
			json funcEntry = f;
			if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
				funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
			registerPlnFunc(funcEntry["name"], funcEntry);
		}
	// 2. Process top-level statements (cinclude/import registered here,
	//    visible in Palan function bodies processed next)
	sa["statements"] = sa_statements(ast["ast"]["statements"]);
	// 3. Process each function body
	if (ast["ast"].contains("functions"))
		sa_functions(ast["ast"]["functions"]);
	leaveScope();
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
		else if (t == "return")      result.push_back(sa_return_stmt(stmt));
		else if (t == "tapple-decl") result.push_back(sa_tapple_decl(stmt));
		else if (t == "not-impl")    result.push_back(stmt);
		else if (t == "func-def") {
			cerr << PlnSaMessage::getMessage(E_InternalError, "1") << endl;
			exit(1);
		}
		else if (t == "block")    result.push_back(sa_block(stmt));
		else {
			cerr << PlnSaMessage::getMessage(E_InternalError, "2") << endl;
			exit(1);
		}
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
		const json* varType = findVar(expr["name"].get<string>());
		if (varType != nullptr) {
			sa_expr["var-type"]   = *varType;
			sa_expr["value-type"] = *varType;
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
			cerr << PlnSaMessage::getMessage(E_IncompatibleTypeCast,
				src["value-type"]["type-name"].get<string>(),
				expr["target-type"]["type-name"].get<string>()) << endl;
			exit(1);
		}

	} else if (expr_type == "call") {
		const json* funcParams = nullptr;
		bool isVariadic = false;

		const json* cfunc = findCFunc(expr["name"]);
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
			const json* pFunc = findPlnFunc(expr["name"].get<string>());
			if (pFunc != nullptr) {
				sa_expr["func-type"] = "palan";
				if (pFunc->contains("ret-type"))
					sa_expr["value-type"] = (*pFunc)["ret-type"];
				if (pFunc->contains("parameters"))
					funcParams = &(*pFunc)["parameters"];
			} else {
				cerr << PlnSaMessage::getMessage(E_UndefinedFunction,
					expr["name"].get<string>()) << endl;
				exit(1);
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
		declareVar(name, var["var-type"]);

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
					if (et != "lit-int" && et != "lit-uint") {
						cerr << PlnSaMessage::getMessage(E_InvalidNarrowingInit) << endl;
						exit(1);
					}
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

json PlnSemanticAnalyzer::sa_block(const json& stmt)
{
	enterScope();

	// pre-register block-local func-defs (forward reference support)
	for (auto& f : stmt.value("functions", json::array())) {
		if (f.value("export", false)) {
			cerr << PlnSaMessage::getMessage(E_ExportInBlock, f["name"].get<string>()) << endl;
			exit(1);
		}
		json funcEntry = f;
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry);
	}

	// analyze block-local func bodies -> appended to sa["functions"]
	for (auto& f : stmt.value("functions", json::array()))
		sa_function(f);

	// analyze body statements (no func-defs)
	json body = sa_statements(stmt["body"]);

	leaveScope();
	return {{"stmt-type", "block"}, {"body", move(body)}};
}

void PlnSemanticAnalyzer::sa_function(const json& funcDef)
{
	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes = varScopes;
	auto savedCurrentFunc = currentFunc_;
	varScopes = {{}};
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			declareVar(p["name"], p["var-type"]);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
			declareVar(r["name"], r["var-type"]);

	currentFunc_ = findPlnFunc(funcDef["name"]);
	enterScope();  // push cFuncScopes/plnFuncScopes/importScopes frame

	const json& blk = funcDef["block"];

	// pre-register inner func-defs (visible only within this function)
	for (auto& f : blk.value("functions", json::array())) {
		if (f.value("export", false)) {
			cerr << PlnSaMessage::getMessage(E_ExportInFunction, f["name"].get<string>()) << endl;
			exit(1);
		}
		json funcEntry = f;
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = funcDef;
	// Single named return: add ret-type so codegen knows the return type
	if (!saFunc.contains("ret-type") && saFunc.contains("rets") && saFunc["rets"].size() == 1)
		saFunc["ret-type"] = saFunc["rets"][0]["var-type"];
	saFunc["body"] = sa_statements(blk["body"]);
	saFunc.erase("block");

	leaveScope();
	varScopes    = savedVarScopes;
	currentFunc_ = savedCurrentFunc;

	sa["functions"].push_back(saFunc);
}

json PlnSemanticAnalyzer::sa_assign_stmt(const json& stmt)
{
	string name = stmt["name"];
	const json* varType = findVar(name);
	if (varType == nullptr) {
		cerr << PlnSaMessage::getMessage(E_UndefinedVariable, name) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(*varType);
	json value = sa_expression(stmt["value"], toType);
	if (value.contains("value-type")) {
		const PlnType* fromType = registry_.fromJson(value["value-type"]);
		TypeCompat compat = typeCompat(fromType, toType, registry_);
		if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast)
			value = wrapConvert(value, registry_.toJson(toType));
	}
	return {{"stmt-type", "assign"}, {"name", name}, {"value", value}};
}

json PlnSemanticAnalyzer::sa_return_stmt(const json& stmt)
{
	if (currentFunc_ == nullptr) {
		cerr << PlnSaMessage::getMessage(E_ReturnOutsideFunction) << endl;
		exit(1);
	}
	bool hasRets   = currentFunc_->contains("rets");
	bool hasRetType = currentFunc_->contains("ret-type");
	bool hasValues = stmt.contains("values");

	if (hasRets) {
		// Multiple return values: bare return only
		if (hasValues) {
			cerr << PlnSaMessage::getMessage(E_MultiRetBareReturn) << endl;
			exit(1);
		}
		return {{"stmt-type", "return"}};
	}
	if (hasRetType) {
		// Single return value: exactly one expression required
		if (!hasValues || stmt["values"].size() != 1) {
			cerr << PlnSaMessage::getMessage(E_SingleRetOneExpr) << endl;
			exit(1);
		}
		const PlnType* toType = registry_.fromJson((*currentFunc_)["ret-type"]);
		json value = sa_expression(stmt["values"][0], toType);
		if (value.contains("value-type")) {
			const PlnType* fromType = registry_.fromJson(value["value-type"]);
			TypeCompat compat = typeCompat(fromType, toType, registry_);
			if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast)
				value = wrapConvert(value, registry_.toJson(toType));
		}
		return {{"stmt-type", "return"}, {"values", json::array({value})}};
	}
	// Void function: bare return only
	if (hasValues) {
		cerr << PlnSaMessage::getMessage(E_VoidBareReturn) << endl;
		exit(1);
	}
	return {{"stmt-type", "return"}};
}

json PlnSemanticAnalyzer::sa_tapple_decl(const json& stmt)
{
	const json& callExpr = stmt["value"];
	const string& fname = callExpr["name"].get<string>();

	// Resolve function — fall back to not-impl if not a known multi-return Palan func
	const json* pFunc = findPlnFunc(fname);
	if (pFunc == nullptr) {
		cerr << PlnSaMessage::getMessage(E_TupleUndefinedFunction, fname) << endl;
		exit(1);
	}
	if (!pFunc->contains("rets") || (*pFunc)["rets"].size() < 2) {
		cerr << PlnSaMessage::getMessage(E_TupleNeedsMultiRet, fname) << endl;
		exit(1);
	}
	if (stmt["vars"].size() != (*pFunc)["rets"].size()) {
		cerr << PlnSaMessage::getMessage(E_TupleVarCountMismatch, fname) << endl;
		exit(1);
	}

	// Process the call expression via sa_expression (resolves func-type, annotates args)
	json saCall = sa_expression(callExpr);

	// Add multi-return value-types from the function's rets
	json valueTypes = json::array();
	for (auto& r : (*pFunc)["rets"])
		valueTypes.push_back(r["var-type"]);
	saCall["value-types"] = valueTypes;

	// Register declared variables in the symbol table
	for (size_t i = 0; i < stmt["vars"].size(); i++)
		declareVar(stmt["vars"][i]["var-name"].get<string>(), (*pFunc)["rets"][i]["var-type"]);

	return {{"stmt-type", "tapple-decl"}, {"vars", stmt["vars"]}, {"value", saCall}};
}

void PlnSemanticAnalyzer::sa_cinclude(const json &stmt)
{
	if (stmt.contains("functions")) {
		for (auto& f : stmt["functions"]) {
			registerCFunc(f["name"], f);
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

void PlnSemanticAnalyzer::sa_import(const json& stmt)
{
	filesystem::path imp_path = stmt["path"];
	if (stmt["path-type"] == "src" && !is_absolute(imp_path))
		imp_path = basePath + '/' + imp_path.string();

	if (!imp_path.string().ends_with(".pa")) return;
	imp_path += ".ast.json";

	ifstream astfile(imp_path.string());
	if (!astfile.is_open()) {
		cerr << PlnSaMessage::getMessage(E_ImportFileNotFound, imp_path.string()) << endl;
		exit(1);
	}
	json imp_ast = json::parse(astfile);

	if (!imp_ast.contains("export")) return;
	for (auto& f : imp_ast["export"]) {
		if (findPlnFunc(f["name"].get<string>())) continue;
		json funcEntry = f;
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets")
				&& funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry);
	}
}

