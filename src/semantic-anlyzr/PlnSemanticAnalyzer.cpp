/// Palan Semantic Analyzer — core: scope management, registry, analysis entry
///
/// @file PlnSemanticAnalyzer.cpp
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

// Collect free() stmts for arrayScopeVars_[from_idx, to_idx) in reverse scope/decl order
json PlnSemanticAnalyzer::collectFreeStmts(size_t from_idx, size_t to_idx)
{
	json result = json::array();
	for (size_t i = to_idx; i > from_idx; --i) {
		auto& scope = arrayScopeVars_[i - 1];
		for (auto it = scope.rbegin(); it != scope.rend(); ++it)
			result.push_back(it->second);
	}
	return result;
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
	arrayScopeVars_.push_back({});
}

void PlnSemanticAnalyzer::leaveScope()
{
	varScopes.pop_back();
	cFuncScopes.pop_back();
	plnFuncScopes.pop_back();
	importScopes.pop_back();
	arrayScopeVars_.pop_back();
}

string PlnSemanticAnalyzer::locPrefix(const json& node) const
{
	if (node.contains("loc") && node["loc"].is_array() && node["loc"].size() >= 2)
		return inputFilePath + ":" + to_string(node["loc"][0].get<int>())
		       + ":" + to_string(node["loc"][1].get<int>()) + ": error: ";
	return "";
}

void PlnSemanticAnalyzer::declareVar(const string& name, const json& type, const json* loc_node)
{
	for (auto& scope : varScopes)
		if (scope.count(name)) {
			cerr << locPrefix(loc_node ? *loc_node : json{})
			     << PlnSaMessage::getMessage(E_DuplicateVarDecl, name) << endl;
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

bool PlnSemanticAnalyzer::isInArrayScope(const string& name) const
{
	for (auto& scope : arrayScopeVars_)
		for (auto& [n, _] : scope)
			if (n == name) return true;
	return false;
}

void PlnSemanticAnalyzer::removeFromArrayScope(const string& name)
{
	for (auto& scope : arrayScopeVars_) {
		auto it = find_if(scope.begin(), scope.end(),
			[&](const pair<string,json>& p){ return p.first == name; });
		if (it != scope.end()) { scope.erase(it); return; }
	}
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

void PlnSemanticAnalyzer::registerPlnFunc(const string& name, const json& def, const json* loc_node)
{
	for (auto& scope : plnFuncScopes)
		if (scope.count(name)) {
			cerr << locPrefix(loc_node ? *loc_node : json{})
			     << PlnSaMessage::getMessage(E_DuplicateFuncDef, name) << endl;
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

const json* PlnSemanticAnalyzer::findImportFunc(const string& fname) const
{
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find("");
		if (alias_it == it->end()) continue;
		auto f_it = alias_it->second.find(fname);
		if (f_it != alias_it->second.end())
			return &f_it->second;
	}
	return nullptr;
}

const json* PlnSemanticAnalyzer::findImportFuncByAlias(const string& alias, const string& fname) const
{
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find(alias);
		if (alias_it == it->end()) continue;
		auto f_it = alias_it->second.find(fname);
		if (f_it != alias_it->second.end())
			return &f_it->second;
	}
	return nullptr;
}

void PlnSemanticAnalyzer::validateEmbeddedParams(const json& funcDef)
{
	if (!funcDef.contains("parameters")) return;
	for (auto& p : funcDef["parameters"]) {
		if (!p.contains("var-type")) continue;
		const auto& vt = p["var-type"];
		if (vt.value("embedded", false) && !vt.contains("inner-size")) {
			cerr << locPrefix(funcDef)
			     << PlnSaMessage::getMessage(E_EmbeddedArrUnsizedInner) << endl;
			exit(1);
		}
	}
}

void PlnSemanticAnalyzer::recordAllocShape(const string& name)
{
	if (allocShapeNames_.count(name)) return;
	allocShapeNames_.insert(name);

	const StructDef& def = structDefs_[name];
	json fields = json::array();
	for (auto& f : def.fields) {
		fields.push_back({
			{"name",      f.name},
			{"type-kind", f.typeKind},
			{"type-name", f.typeName},
			{"offset",    f.offset},
			{"size",      f.size}
		});
	}
	json owned = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "struct-ptr") continue;
		const StructDef& sub = structDefs_[f.typeName];
		owned.push_back({
			{"name",              f.name},
			{"offset",            f.offset},
			{"struct-name",       f.typeName},
			{"struct-total-size", sub.totalSize},
			{"needs-alloc",       sub.hasOwnedStructFields}
		});
		recordAllocShape(f.typeName);
	}
	sa["alloc-shapes"].push_back({
		{"shape-kind",   "struct"},
		{"shape-name",   name},
		{"total-size",   def.totalSize},
		{"fields",       move(fields)},
		{"owned-fields", move(owned)}
	});
}

void PlnSemanticAnalyzer::analysis(const json &ast)
{
	this->inputFilePath = ast["original"];
	sa["original"]      = ast["original"];
	sa["str-literals"]  = json::array();
	sa["functions"]     = json::array();
	sa["alloc-shapes"]  = json::array();
	enterScope();
	// 1. Pre-register Palan functions so calls can resolve them
	if (ast["ast"].contains("functions"))
		for (auto& f : ast["ast"]["functions"]) {
			// Single named return: synthesize ret-type for call resolution
			json funcEntry = f;
			normalizeUnsizedArrSig(funcEntry);
			validateEmbeddedParams(funcEntry);
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

void PlnSemanticAnalyzer::sa_cinclude(const json &stmt)
{
	if (!stmt.contains("functions")) return;

	if (stmt.contains("alias")) {
		const string& alias = stmt["alias"].get<string>();
		auto& currentScope = importScopes.back();
		for (auto& f : stmt["functions"]) {
			string fname = f["name"].get<string>();
			json entry = f;
			entry["_c-func"] = true;
			currentScope[alias][fname] = entry;
		}
	} else {
		for (auto& f : stmt["functions"]) {
			registerCFunc(f["name"], f);
		}
	}
}

static bool is_absolute(filesystem::path &path)
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
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ImportFileNotFound, imp_path.string()) << endl;
		exit(1);
	}
	json imp_ast = json::parse(astfile);

	if (!imp_ast.contains("export")) return;

	string alias = stmt.value("alias", "");
	bool hasAlias = !alias.empty();

	set<string> targets;
	bool selective = stmt.contains("targets");
	if (selective)
		for (auto& t : stmt["targets"])
			targets.insert(t.get<string>());

	auto& currentScope = importScopes.back();

	for (auto& f : imp_ast["export"]) {
		string fname = f["name"].get<string>();
		if (selective && !targets.count(fname)) continue;

		json funcEntry = f;
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets")
				&& funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];

		if (hasAlias) {
			currentScope[alias][fname] = funcEntry;
		} else {
			if (currentScope[""].count(fname))
				currentScope[""][fname] = json{};  // ambiguous sentinel
			else
				currentScope[""][fname] = funcEntry;
		}
	}
}
