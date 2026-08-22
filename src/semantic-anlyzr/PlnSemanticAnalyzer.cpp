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
		// LCOV_EXCL_EXCEPTION_BR_START
		json fj = {
			{"name",      f.name},
			{"type-kind", f.typeKind},
			{"type-name", f.typeName},
			{"offset",    f.offset},
			{"size",      f.size}
		};
		if (f.typeKind == "embed-arr" || f.typeKind == "arr-ptr" || f.typeKind == "embed-ptr-arr") {
			fj["count"]     = f.count;
			fj["elem-kind"] = f.elemKind;
			fj["mutable"]   = f.isMutable;
		}
		fields.push_back(move(fj));
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	json owned = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "struct-ptr") continue;
		const StructDef& sub = structDefs_[f.typeName];
		// LCOV_EXCL_EXCEPTION_BR_START
		owned.push_back({
			{"name",              f.name},
			{"offset",            f.offset},
			{"struct-name",       f.typeName},
			{"struct-total-size", sub.totalSize},
			{"needs-alloc",       sub.hasOwnedStructFields}
		});
		// LCOV_EXCL_EXCEPTION_BR_STOP
		recordAllocShape(f.typeName);
	}
	json ownedArr = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "arr-ptr") continue;
		// LCOV_EXCL_EXCEPTION_BR_START
		ownedArr.push_back({
			{"name",      f.name},
			{"offset",    f.offset},
			{"elem-kind", f.elemKind},
			{"leaf-name", f.typeName},
			{"count",     f.count}
		});
		// LCOV_EXCL_EXCEPTION_BR_STOP
		if (f.elemKind == "struct") {
			recordAllocShape(f.typeName);
			string shape_key = "arr_" + f.typeName;
			if (!allocShapeNames_.count(shape_key)) {
				allocShapeNames_.insert(shape_key);
				// LCOV_EXCL_EXCEPTION_BR_START
				sa["alloc-shapes"].push_back({
					{"shape-kind",   "arr-struct"},
					{"shape-key",    shape_key},
					{"struct-name",  f.typeName}
				});
				// LCOV_EXCL_EXCEPTION_BR_STOP
			}
		}
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	sa["alloc-shapes"].push_back({
		{"shape-kind",         "struct"},
		{"shape-name",         name},
		{"total-size",         def.totalSize},
		{"fields",             move(fields)},
		{"owned-fields",       move(owned)},
		{"owned-array-fields", move(ownedArr)}
	});
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

bool PlnSemanticAnalyzer::isStructType(const json& type) const
{
	// LCOV_EXCL_EXCEPTION_BR_START
	return type.value("type-kind","") == "prim" &&
	       structDefs_.count(type.value("type-name",""));
	// LCOV_EXCL_EXCEPTION_BR_STOP
}

json PlnSemanticAnalyzer::toStructPntrType(const json& type) const
{
	if (!isStructType(type)) return type;
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = type["type-name"].get<string>();
	return {{"type-kind","pntr"},
	        {"base-type",{{"type-kind","struct"},{"type-name",name}}}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

bool PlnSemanticAnalyzer::isNamedReturnVar(const string& varName) const
{
	if (!currentFunc_ || !currentFunc_->contains("rets")) return false;
	for (auto& r : (*currentFunc_)["rets"]) {
		// LCOV_EXCL_EXCEPTION_BR_START
		if (r["name"].get<string>() != varName) continue;
		// LCOV_EXCL_EXCEPTION_BR_STOP
		const auto& vt = r["var-type"];
		if (isStructType(vt)) return true;
		// Also accept the normalized pntr(struct(Name)) form produced by normalizeStructSig
		// LCOV_EXCL_EXCEPTION_BR_START
		if (vt.value("type-kind","") == "pntr" && vt.contains("base-type") &&
		    vt["base-type"].value("type-kind","") == "struct")
			return true;
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	return false;
}

json PlnSemanticAnalyzer::deepNormalizePrimToStruct(const json& type) const
{
	// Recursively convert prim(Name) → struct(Name) inside pntr chains.
	// Needed when struct types appear nested in pointer-of-pointer signatures like []@!T.
	if (type.value("type-kind","") == "pntr") {
		json t = type;
		t["base-type"] = deepNormalizePrimToStruct(type["base-type"]);
		return t;
	}
	if (isStructType(type))
		return {{"type-kind","struct"},{"type-name",type["type-name"]}};
	return type;
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::normalizeStructSig(json& funcDef)
{
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"]) {
			p["var-type"] = resolveTypeAlias(p["var-type"]);
			if (isStructType(p["var-type"]))
				p["var-type"] = toStructPntrType(p["var-type"]);
			else
				p["var-type"] = deepNormalizePrimToStruct(p["var-type"]);
		}
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"]) {
			r["var-type"] = resolveTypeAlias(r["var-type"]);
			if (isStructType(r["var-type"]))
				r["var-type"] = toStructPntrType(r["var-type"]);
			else
				r["var-type"] = deepNormalizePrimToStruct(r["var-type"]);
		}
	if (funcDef.contains("ret-type")) {
		funcDef["ret-type"] = resolveTypeAlias(funcDef["ret-type"]);
		if (isStructType(funcDef["ret-type"]))
			funcDef["ret-type"] = toStructPntrType(funcDef["ret-type"]);
		else
			funcDef["ret-type"] = deepNormalizePrimToStruct(funcDef["ret-type"]);
	}
}

// Resolve alias type-names in a not-yet-registered function signature so
// call-site type checks done while pre-registering (before the function's
// own body/normalizeStructSig pass runs) see the underlying primitive type.
void PlnSemanticAnalyzer::resolveFuncSigTypeAliases(json& funcEntry) const
{
	if (funcEntry.contains("parameters"))
		for (auto& p : funcEntry["parameters"])
			p["var-type"] = resolveTypeAlias(p["var-type"]);
	if (funcEntry.contains("rets"))
		for (auto& r : funcEntry["rets"])
			r["var-type"] = resolveTypeAlias(r["var-type"]);
	if (funcEntry.contains("ret-type"))
		funcEntry["ret-type"] = resolveTypeAlias(funcEntry["ret-type"]);
}

void PlnSemanticAnalyzer::analysis(const json &ast)
{
	this->inputFilePath = ast["original"];
	sa["original"]      = ast["original"];
	sa["str-literals"]  = json::array();
	sa["functions"]     = json::array();
	sa["alloc-shapes"]  = json::array();
	enterScope();
	// 0. Pre-scan top-level type-alias declarations so function signatures
	//    pre-registered in step 1 (and calls resolved during step 2) see
	//    fully-resolved primitive types instead of alias names.
	if (ast["ast"].contains("statements"))
		for (auto& stmt : ast["ast"]["statements"])
			if (stmt.value("stmt-type", "") == "type-alias")
				sa_type_alias(stmt);
	// 1. Pre-register Palan functions so calls can resolve them
	if (ast["ast"].contains("functions"))
		for (auto& f : ast["ast"]["functions"]) {
			// Single named return: synthesize ret-type for call resolution
			json funcEntry = f;
			normalizeUnsizedArrSig(funcEntry);
			validateEmbeddedParams(funcEntry);
			if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
				funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
			resolveFuncSigTypeAliases(funcEntry);
			registerPlnFunc(funcEntry["name"], funcEntry);
		}
	// 2. Process top-level statements (cinclude/import registered here,
	//    visible in Palan function bodies processed next)
	sa["statements"] = sa_statements(ast["ast"]["statements"]);
	// 2.5. Re-normalize pre-registered Palan function signatures now that struct types are known.
	//      Step 1 ran before type declarations were processed, so struct-typed params/rets
	//      were left as prim(Name). Normalize them here so call resolution in step 3 is correct.
	for (auto& scope : plnFuncScopes)
		for (auto& [_, entry] : scope)
			normalizeStructSig(entry);
	// 3. Process each function body
	if (ast["ast"].contains("functions"))
		sa_functions(ast["ast"]["functions"]);
	leaveScope();
}

const json& PlnSemanticAnalyzer::result()
{
	return sa;
}

// Recursively find "typedef-name" hints (added by c2ast for scalar typedefs
// like size_t) inside a type node, register the underlying primitive into
// typeAliases_ so Palan code can reference the typedef name via IT-2602's
// alias mechanism, and strip the hint so it doesn't leak into sa.json (e.g.
// via sa_expr_call copying a C function's ret-type into a call's value-type).
void PlnSemanticAnalyzer::registerTypedefAliasInType(json& vtype)
{
	string tk = vtype.value("type-kind", "");
	if (tk == "prim" && vtype.contains("typedef-name")) {
		string aliasName = vtype["typedef-name"].get<string>();
		json resolved = {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}};
		auto it = typeAliases_.find(aliasName);
		if (it == typeAliases_.end()) {
			typeAliases_[aliasName] = resolved;
		} else if (it->second != resolved) {
			cerr << PlnSaMessage::getMessage(E_ConflictingTypedef, aliasName) << endl;
			exit(1);
		}
		vtype.erase("typedef-name");
	} else if (tk == "pntr" && vtype.contains("base-type")) {
		registerTypedefAliasInType(vtype["base-type"]);
	} else if (tk == "func") {
		if (vtype.contains("ret-type"))
			registerTypedefAliasInType(vtype["ret-type"]);
		if (vtype.contains("parameters"))
			for (auto& p : vtype["parameters"])
				if (p.contains("var-type"))
					registerTypedefAliasInType(p["var-type"]);
	}
}

void PlnSemanticAnalyzer::registerCFuncTypedefAliases(json& funcEntry)
{
	if (funcEntry.contains("ret-type"))
		registerTypedefAliasInType(funcEntry["ret-type"]);
	if (funcEntry.contains("parameters"))
		for (auto& p : funcEntry["parameters"])
			if (p.contains("var-type"))
				registerTypedefAliasInType(p["var-type"]);
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
			registerCFuncTypedefAliases(entry);
			entry["_c-func"] = true;
			currentScope[alias][fname] = entry;
		}
	} else {
		for (auto& f : stmt["functions"]) {
			json entry = f;
			registerCFuncTypedefAliases(entry);
			registerCFunc(entry["name"].get<string>(), entry);
		}
	}

	if (stmt.contains("constants")) {
		for (auto& c : stmt["constants"]) {
			string name = c["name"].get<string>();
			if (constDecls_.count(name)) continue;  // first header wins on duplicate macro names
			constDecls_[name] = {{"value", {{"expr-type","lit-int"},{"value",c["value"]},{"value-type",c["value-type"]}}},
			                     {"value-type", c["value-type"]}};
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
