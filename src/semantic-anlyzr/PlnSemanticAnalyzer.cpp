/// Palan Semantic Analyzer — core: scope management, registry, analysis entry
///
/// @file PlnSemanticAnalyzer.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <fstream>
#include <filesystem>
#include <set>
#include <deque>
#include <stdexcept>
#include <boost/assert.hpp>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

struct PlnSemanticAnalyzer::TypeProgram {
	map<string, PlnSemanticAnalyzer*> modules;  // moduleId -> its type context, the root's included
	vector<unique_ptr<PlnSemanticAnalyzer>> imported;
	struct Resolving { PlnSemanticAnalyzer* module; string name; const json* stmt; };
	vector<Resolving> resolving;
	deque<function<void()>> settled;
};

// Collect release stmts for arrayScopeVars_[from_idx, to_idx) in reverse scope/decl order
json PlnSemanticAnalyzer::collectFreeStmts(size_t from_idx, size_t to_idx, const string& keep)
{
	json result = json::array();
	for (size_t i = to_idx; i > from_idx; --i) {
		auto& scope = arrayScopeVars_[i - 1];
		for (auto it = scope.rbegin(); it != scope.rend(); ++it)
			if (!it->release.is_null() && it->name != keep)
				result.push_back(it->release);
	}
	return result;
}

PlnSemanticAnalyzer::PlnSemanticAnalyzer(string base_path, string ast_filename, string c2ast_path)
	: basePath(base_path), astFileName(ast_filename), c2astPath(c2ast_path), inputFilePath(""),
	  ownedProgram_(make_unique<TypeProgram>()), program_(ownedProgram_.get())
{
}

PlnSemanticAnalyzer::~PlnSemanticAnalyzer() = default;

void PlnSemanticAnalyzer::enterScope()
{
	varScopes.push_back({});
	cFuncScopes.push_back({});
	cGlobalScopes.push_back({});
	plnFuncScopes.push_back({});
	importScopes.push_back({});
	arrayScopeVars_.push_back({});
}

void PlnSemanticAnalyzer::leaveScope()
{
	varScopes.pop_back();
	cFuncScopes.pop_back();
	cGlobalScopes.pop_back();
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
	bool taken = constDecls_.count(name) > 0;
	for (auto& scope : varScopes) taken = taken || scope.count(name);
	if (taken) {
		cerr << locPrefix(loc_node ? *loc_node : json{})
		     << PlnSaMessage::getMessage(E_DuplicateVarDecl, name) << endl;
		exit(1);
	}
	// A const may have read this type early, from the declaration's shape.
	if (varScopes.size() == 1 && loc_node) {
		auto v = topVars_.find(name);
		BOOST_ASSERT(v == topVars_.end() || v->second.type.is_null()
		             || (*v->second.stmt)["loc"] != loc_node->value("loc", json()) || v->second.type == type);
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

bool PlnSemanticAnalyzer::isLocalVar(const string& name) const
{
	for (size_t i = varScopes.size(); i > 0; --i)
		if (varScopes[i-1].count(name))
			return (i-1) >= funcBodyScopeIdx_;
	return false; // LCOV_EXCL_LINE -- callers only invoke this after findVar() succeeded
}

bool PlnSemanticAnalyzer::isOwnedVar(const string& name) const
{
	for (auto& scope : arrayScopeVars_)
		for (auto& v : scope)
			if (v.name == name) return v.owned;
	return false;
}

bool PlnSemanticAnalyzer::isFrameVar(const string& name) const
{
	for (auto& scope : arrayScopeVars_)
		for (auto& v : scope)
			if (v.name == name) return !v.owned;
	return false;
}

void PlnSemanticAnalyzer::removeFromArrayScope(const string& name)
{
	for (auto& scope : arrayScopeVars_) {
		auto it = find_if(scope.begin(), scope.end(),
			[&](const ScopeVar& v){ return v.name == name; });
		if (it != scope.end()) { scope.erase(it); return; }
	}
}

void PlnSemanticAnalyzer::addOwnedScopeVar(const string& name, json release)
{
	arrayScopeVars_.back().push_back({name, move(release), true});
}

// `@[+]@[]uint8` as its owner would hold it; `args` borrows it read-only.
static json argsStorageType()
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json elem = {{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","prim"},{"type-name","uint8"}}}};
	return varrPntrType(elem, false);
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Whether a top-level statement names `args`, which no declaration there can
// shadow. A function's body has a scope of its own, so it is not looked into.
static bool namesArgs(const json& node)
{
	if (node.is_object()) {
		if ((node.value("expr-type","") == "id" || node.value("stmt-type","") == "assign")
		    && node.value("name","") == "args")
			return true;
		for (auto& [k, v] : node.items())
			if (k != "functions" && namesArgs(v)) return true;
	} else if (node.is_array()) {
		for (auto& v : node)
			if (namesArgs(v)) return true;
	}
	return false;
}

// Its builtins and shape are registered only when the top level names it, so
// a module that does not leaves its output as it was. A call looks its
// callee up by name before reading its arguments, so this comes first.
void PlnSemanticAnalyzer::noteImplicitArgs(const json& stmts)
{
	if (!namesArgs(stmts)) return;
	argsUsed_ = true;
	recordVarrShape(argsStorageType());
}

// The header lives in the top-level function's frame and is never freed.
json PlnSemanticAnalyzer::implicitArgsDecl() const
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json& vt = *findVar("args");
	const json& params = sa["main"]["parameters"];
	auto param = [&](size_t i) {
		const json& t = params[i]["var-type"];
		return json{{"expr-type","id"},{"name",params[i]["name"]},{"var-type",t},{"value-type",t}};
	};
	json data = {{"type-kind","pntr"},{"base-type",vt["base-type"]["base-type"]}};
	auto store = [&](int offset, const json& type, json value) {
		return json{{"stmt-type","field-assign"},{"var","args"},{"offset",offset},
		            {"value-type",type},{"value",move(value)}};
	};
	json init = {{"expr-type","frame-alloc"},{"size",24},{"align",8},{"value-type",vt}};
	return json::array({
		{{"stmt-type","var-decl"},{"vars",json::array({{{"name","args"},{"var-type",vt},{"init",move(init)}}})}},
		store(0, params[0]["var-type"], param(0)),
		store(8, params[0]["var-type"], param(0)),
		store(16, data, param(1)),
	});
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::registerCFunc(const string& name, const json& def)
{
	cFuncScopes.back()[name] = def;  // shadow allowed
}

void PlnSemanticAnalyzer::registerCGlobal(const string& name, const json& def)
{
	cGlobalScopes.back()[name] = def;  // shadow allowed
}

const json* PlnSemanticAnalyzer::findCGlobal(const string& name) const
{
	for (auto it = cGlobalScopes.rbegin(); it != cGlobalScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

// gen-ast marks a reference to a cinclude'd macro c2ast could not export, so it
// is not reported as an undefined name the header plainly defines.
void PlnSemanticAnalyzer::undefinedNameError(const json& at, const json& ref, PlnSaMessageCode undefinedCode) const
{
	string macro = ref.value("c-macro", "");
	PlnSaMessageCode code = macro == "function-like" ? E_CMacroFuncLike
	                      : macro == "not-constant" ? E_CMacroNotConstant
	                      : undefinedCode;
	string name = ref["name"].get<string>();
	if (code == E_UndefinedVariable) {
		vector<string> spellings = enumeratorSpellings(name);
		if (!spellings.empty()) {
			string list;
			for (size_t i = 0; i < spellings.size(); i++)
				list += (i == 0 ? "" : i + 1 == spellings.size() ? " or " : ", ") + ("'" + spellings[i] + "'");
			cerr << locPrefix(at) << PlnSaMessage::getMessage(E_UnqualifiedEnumerator, name, list) << endl;
			exit(1);
		}
	}
	cerr << locPrefix(at) << PlnSaMessage::getMessage(code, name) << endl;
	exit(1);
}

void PlnSemanticAnalyzer::reportDiag(const json& locNode, const SaDiag& diag) const
{
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(diag.code, diag.arg1, diag.arg2) << endl;
	exit(1);
}

static bool sameParamTypes(const json& a, const json& b)
{
	const json& pa = a.value("parameters", json::array());
	const json& pb = b.value("parameters", json::array());
	if (pa.size() != pb.size()) return false;
	for (size_t i = 0; i < pa.size(); i++)
		if (pa[i]["var-type"] != pb[i]["var-type"]) return false;
	return true;
}

// The builtins exist for every T, whether or not a `[+]T` has made them
// candidates yet, so declaration order does not matter. A `len` of a borrowed
// constant-size array would never be called: such a len is folded first.
static bool definesBuiltin(const string& name, const json& def)
{
	const json& params = def.value("parameters", json::array());
	if (params.empty()) return false;
	const json& first = params[0]["var-type"];
	if (name == "len" && params.size() == 1 && first.contains("arr-size") && first.contains("mutable"))
		return true;
	if (!isVarrPntr(first)) return false;
	for (auto& builtin : varrBuiltinDefs(first["base-type"]))
		if (builtin["name"] == name && sameParamTypes(builtin, def)) return true;
	return false;
}

// Functions of one name are told apart by their parameter types alone, since
// a call chooses among them by its arguments.
void PlnSemanticAnalyzer::registerPlnFunc(const string& name, const json& def, const json& locNode)
{
	if (definesBuiltin(name, def)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateFuncDef, name) << endl;
		exit(1);
	}
	for (auto& scope : plnFuncScopes) {
		auto it = scope.find(name);
		if (it == scope.end()) continue;
		for (auto& other : it->second)
			if (sameParamTypes(other, def)) {
				cerr << locPrefix(locNode)
				     << PlnSaMessage::getMessage(E_DuplicateFuncDef, name) << endl;
				exit(1);
			}
	}
	plnFuncScopes.back()[name].push_back(def);
}

void PlnSemanticAnalyzer::validateEmbeddedParams(const json& funcDef)
{
	if (!funcDef.contains("parameters")) return;
	for (auto& p : funcDef["parameters"]) {
		if (!p.contains("var-type")) continue;
		const auto& vt = p["var-type"];
		if (vt.value("embedded", false) && !vt.contains("stride")) {
			cerr << locPrefix(funcDef)
			     << PlnSaMessage::getMessage(E_EmbeddedArrUnsizedInner) << endl;
			exit(1);
		}
	}
}

bool PlnSemanticAnalyzer::isStructType(const json& type) const
{
	// LCOV_EXCL_EXCEPTION_BR_START
	return type.value("type-kind","") == "prim" &&
	       structDefs_.count(type.value("type-name",""));
	// LCOV_EXCL_EXCEPTION_BR_STOP
}

bool PlnSemanticAnalyzer::isKnownTypeName(const string& name) const
{
	return structDefs_.count(name) || elemSizeBytes(name) >= 0;
}

bool PlnSemanticAnalyzer::isKnownPointeeTypeName(const string& name) const
{
	// `void` is pointee-only (PlnType.h PrimType::Name::Void): @void/@!void are
	// C's void*, while a standalone `void x;` stays rejected by isKnownTypeName.
	return isPrimPointeeName(name) || isKnownTypeName(name);
}

void PlnSemanticAnalyzer::requireKnownTypeNames(const json& locNode, const json& type) const
{
	const json* t = &type;
	bool isPointee = false;
	while (t->value("type-kind","") == "arr" || t->value("type-kind","") == "pntr") {
		isPointee = t->value("type-kind","") == "pntr";
		if (isFuncPtr(*t) && t->value("mutable", false)) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_FuncPtrMutable) << endl;
			exit(1);
		}
		if (t->value("embedded", false)) {
			const json& elem = (*t)["base-type"];
			string ek = elem.value("type-kind","");
			string ename = elem.value("type-name","");
			if (ek == "prim" && !isKnownTypeName(ename)) { // LCOV_EXCL_EXCEPTION_BR_LINE
				cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownType, ename) << endl;
				exit(1);
			}
			if ((ek == "prim" && !structDefs_.count(ename)) || (ek != "prim" && ek != "arr")) {
				cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedElemNotStruct, typeDisplayName(elem)) << endl;
				exit(1);
			}
		}
		t = &(*t)["base-type"];
	}
	if (t->value("type-kind","") == "func") {
		requireFuncSigTypes(locNode, *t);
		return;
	}
	if (t->value("type-kind","") != "prim") return;
	string tname = t->value("type-name","");
	if (isPointee ? isKnownPointeeTypeName(tname) : isKnownTypeName(tname)) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownType, tname) << endl;
	exit(1);
}

// `func` is a function type as written, its names already resolved.
void PlnSemanticAnalyzer::requireFuncSigTypes(const json& locNode, const json& func) const
{
	auto check = [&](const json& t, bool isRet) {
		string tk = t.value("type-kind","");
		bool ok = tk == "prim" ? (isRet || t.value("type-name","") != "void") && !isStructType(t)
		        : tk == "pntr" && t["base-type"].value("type-kind","") != "arr";
		if (!ok) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_FuncPtrSigType, typeDisplayName(t)) << endl;
			exit(1);
		}
		if (!(isRet && t.value("type-name","") == "void")) requireKnownTypeNames(locNode, t);
	};
	for (auto& p : func["parameters"]) check(p["var-type"], false);
	check(func["ret-type"], true);
}

json PlnSemanticAnalyzer::deepNormalizePrimToStruct(const json& type) const
{
	// Converts prim(Name) -> struct(Name) at every level of a pntr chain.
	json resolved = type;
	if (resolved.value("type-kind","") == "pntr") {
		json t = resolved;
		t["base-type"] = deepNormalizePrimToStruct(resolved["base-type"]);
		return t;
	}
	if (resolved.value("type-kind","") == "func") {
		json t = resolved;
		for (auto& p : t["parameters"]) p["var-type"] = deepNormalizePrimToStruct(p["var-type"]);
		t["ret-type"] = deepNormalizePrimToStruct(t["ret-type"]);
		return t;
	}
	if (isStructType(resolved))
		return {{"type-kind","struct"},{"type-name",resolved["type-name"]}};
	return resolved;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Structural-only counterpart to unrepresentableTypeName for native Palan
// signatures: names were already checked by preregisterFunc, so only a
// type-kind that can never build is reported: "arr", left only under a
// returned array borrow by sigType. A function type's own parameters were
// checked by requireFuncSigTypes.
static string unsupportedNativeSigTypeKind(const json& vt)
{
	string k = vt.value("type-kind", "");
	if (k == "pntr")
		return vt.contains("base-type") ? unsupportedNativeSigTypeKind(vt["base-type"]) : "";
	if (k == "prim" || k == "struct" || k == "func" || k == "varr" || k.empty())
		return "";
	return k == "arr" ? "array" : k;
}

void PlnSemanticAnalyzer::validateNativeSig(const json& funcDef)
{
	string bad;
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"]) {
			if (!p.contains("var-type")) continue;
			bad = unsupportedNativeSigTypeKind(p["var-type"]);
			if (!bad.empty()) break;
		}
	if (bad.empty() && funcDef.contains("ret-type"))
		bad = unsupportedNativeSigTypeKind(funcDef["ret-type"]);
	if (bad.empty() && funcDef.contains("rets"))
		for (auto& r : funcDef["rets"]) {
			if (!r.contains("var-type")) continue;
			bad = unsupportedNativeSigTypeKind(r["var-type"]);
			if (!bad.empty()) break;
		}
	if (!bad.empty()) {
		cerr << locPrefix(funcDef)
		     << PlnSaMessage::getMessage(E_UnsupportedParamType, funcDef["name"].get<string>(), bad) << endl;
		exit(1);
	}
}

// The header's data pointer: what `v[i]` indexes and a C function or a `[]T`
// receives. Through a read-only borrow the elements are read-only too.
json PlnSemanticAnalyzer::varrData(json varr)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json& vt = varr["value-type"];
	json elems = {{"type-kind","pntr"},{"base-type",vt["base-type"]["base-type"]}};
	if (isVarrEmbedded(vt)) {
		elems["embedded"] = true;
		elems["stride"] = inPlaceSize(vt["base-type"]["base-type"]);
	}
	// A row carries the permission on each of its levels, as in a borrowed `[m][n]T`.
	if (!isWritableThrough(vt) || (varr.value("expr-type","") == "arr-index" && !isWritableElem(varr))) {
		elems["mutable"] = false;
		elems["base-type"] = withArrPermission(elems["base-type"], false);
	}
	return {{"expr-type","field-access"},{"ptr-expr",move(varr)},{"offset",16},{"value-type",move(elems)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

optional<SaDiag> PlnSemanticAnalyzer::arrBorrowBindingMismatch(const json& srcAst,
		const json& saValue, const json& dstType)
{
	bool namesBorrow = saValue.value("expr-type","") == "id" && saValue.contains("var-type")
	                   && isArrBorrowVar(saValue["var-type"]);
	// Only an explicit cast converts into a borrowed array type.
	bool castBorrow = saValue.value("expr-type","") == "convert";
	if (srcAst.value("expr-type","") != "addr-of" && !namesBorrow && !castBorrow)
		return SaDiag{E_ArrBorrowNeedsAddrOf}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (!arrShapeMatch(saValue["value-type"], dstType)) {
		// Shown as the owned type, without the permission the borrow added.
		const json& srcType = saValue.contains("var-type") ? saValue["var-type"]
			: saValue.value("expr-type","") == "arr-index" ? saValue["array"]["value-type"]["base-type"]
			: saValue["value-type"];
		return SaDiag{E_ArrBorrowShapeMismatch, typeDisplayName(dstType), typeDisplayName(srcType)}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	return nullopt;
}

void PlnSemanticAnalyzer::checkArrBorrowBinding(const json& locNode, const json& srcAst,
		const json& saValue, const json& dstType)
{
	if (auto d = arrBorrowBindingMismatch(srcAst, saValue, dstType)) reportDiag(locNode, *d);
}

optional<SaDiag> PlnSemanticAnalyzer::elemShapeMismatch(const json& saValue, const json& dstType)
{
	if (!saValue.contains("value-type") || elemShapeMatch(saValue["value-type"], dstType)) return nullopt;
	json dstShape = dstType;
	dstShape.erase("mutable");  // a slot reads as the array it points to
	return SaDiag{E_ArrElemShapeMismatch, typeDisplayName(dstShape), typeDisplayName(saValue["value-type"])}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

void PlnSemanticAnalyzer::checkElemShape(const json& locNode, const json& saValue, const json& dstType)
{
	if (auto d = elemShapeMismatch(saValue, dstType)) reportDiag(locNode, *d);
}

// Diagnoses a "syscall" declaration against the Linux syscall ABI (<=6
// GP-register args, no float, single raw-rax return) and folds its number
// into a plain integer. The number must be a literal, not a symbolic
// constant, since a constant's value can't be resolved across an import
// boundary. Must run after normalizeSig.
void PlnSemanticAnalyzer::validateSyscallDecl(json& funcDef)
{
	string funcName = funcDef["name"].get<string>();
	const json& numNode = funcDef["syscall-number"];
	string et = numNode.value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint") {
		cerr << locPrefix(funcDef)
		     << PlnSaMessage::getMessage(E_SyscallNumberNotConstant, funcName) << endl;
		exit(1);
	}
	string numStr = numNode["value"].get<string>();
	uint64_t num = 0;
	bool outOfRange = false;
	try {
		num = stoull(numStr);
	} catch (const out_of_range&) {
		outOfRange = true;
	}
	if (outOfRange || num > UINT32_MAX) {
		cerr << locPrefix(funcDef)
		     << PlnSaMessage::getMessage(E_SyscallNumberOutOfRange, funcName, numStr) << endl;
		exit(1);
	}
	funcDef["syscall-number"] = num;

	if (funcDef.contains("parameters") && funcDef["parameters"].size() > 6) {
		cerr << locPrefix(funcDef)
		     << PlnSaMessage::getMessage(E_SyscallTooManyParams, funcName,
		                                  to_string(funcDef["parameters"].size())) << endl;
		exit(1);
	}

	// The Linux syscall ABI has no sub-word argument/return slot: every GP
	// register is read/written in full, so a narrower declared type would
	// leave stale high bits behind a partial-register move (e.g. movb into
	// %dil) instead of the zero/sign-extension the ABI assumes.
	auto isAbiUnrepresentable = [](const json& vt) {
		if (vt.value("type-kind", "") != "prim") return false;
		string tn = vt.value("type-name", "");
		return tn == "flo32" || tn == "flo64"
		       || tn == "int8" || tn == "int16" || tn == "uint8" || tn == "uint16"
		       || tn == "bool";
	};
	string badType;
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"]) {
			if (p.contains("var-type") && isAbiUnrepresentable(p["var-type"])) {
				badType = typeDisplayName(p["var-type"]);
				break;
			}
		}
	if (badType.empty() && funcDef.contains("ret-type") && isAbiUnrepresentable(funcDef["ret-type"]))
		badType = typeDisplayName(funcDef["ret-type"]);
	if (!badType.empty()) {
		cerr << locPrefix(funcDef)
		     << PlnSaMessage::getMessage(E_SyscallUnsupportedParamType, funcName, badType) << endl;
		exit(1);
	}
}

// gen-ast keeps a parameter or return it parses but does not implement in the
// signature (see ASTSpec.md); every Palan signature, local or imported, is
// checked here before anything reads its types.
void PlnSemanticAnalyzer::rejectNotImplSig(const json& f)
{
	for (auto& p : f["parameters"]) {
		const json& at = p.contains("loc") ? p : f;
		if (p.contains("init") || p.contains("untyped-var")) {
			string name = p.contains("untyped-var") ? p["untyped-var"] : p["name"];
			cerr << locPrefix(p.contains("init") ? p["init"] : at)
			     << PlnSaMessage::getMessage(E_ParamDefaultNotImpl, name) << endl;
			exit(1);
		}
		if (!p.contains("var-type")) {
			cerr << locPrefix(at) << PlnSaMessage::getMessage(E_ParamNotImplemented) << endl;
			exit(1);
		}
	}
	for (auto& r : f.value("rets", json::array()))
		if (!r.contains("var-type")) {
			cerr << locPrefix(r.contains("loc") ? r : f)
			     << PlnSaMessage::getMessage(E_RetNotImplemented) << endl;
			exit(1);
		}
	if (f.contains("ret-type") && f["ret-type"].contains("not-impl")) {
		cerr << locPrefix(f["ret-type"]) << PlnSaMessage::getMessage(E_RetNotImplemented) << endl;
		exit(1);
	}
}

json PlnSemanticAnalyzer::normalizeFuncSig(const json& f, const string& scope)
{
	rejectNotImplSig(f);
	auto ensureSigType = [&](const json& t) {
		ensureTypeRefs(t);
		rejectDeferredConstRefs(f, t);
	};
	for (auto& p : f.value("parameters", json::array()))
		ensureSigType(p["var-type"]);
	for (auto& r : f.value("rets", json::array()))
		ensureSigType(r["var-type"]);
	if (f.contains("ret-type")) ensureSigType(f["ret-type"]);

	json funcEntry = resolveSigTypes(f);
	normalizeSig(funcEntry);
	validateEmbeddedParams(funcEntry);
	if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
		funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
	validateNativeSig(funcEntry);
	if (funcEntry.value("func-type", "") == "syscall")
		validateSyscallDecl(funcEntry);
	else
		funcEntry["symbol"] = scope + ".."
			+ funcSymbol(funcEntry["name"].get<string>(), funcEntry.value("parameters", json::array()));
	return funcEntry;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

json PlnSemanticAnalyzer::resolveSigTypes(const json& f) const
{
	json sig = f;
	if (sig.contains("parameters"))
		for (auto& p : sig["parameters"])
			p["var-type"] = resolveKnownType(f, p["var-type"]);
	if (sig.contains("rets"))
		for (auto& r : sig["rets"])
			r["var-type"] = resolveKnownType(f, r["var-type"]);
	if (sig.contains("ret-type")) sig["ret-type"] = resolveKnownType(f, sig["ret-type"]);
	return sig;
}

void PlnSemanticAnalyzer::preregisterFunc(const json& f, const string& scope)
{
	json funcEntry = normalizeFuncSig(f, scope);
	registerPlnFunc(funcEntry["name"], funcEntry, f);
}

const json* PlnSemanticAnalyzer::firstNonConstName(const json& expr) const
{
	vector<const json*> ids;
	nonConstNames(expr, ids);
	return ids.empty() ? nullptr : ids[0];
}

void PlnSemanticAnalyzer::nonConstNames(const json& expr, vector<const json*>& ids) const
{
	if (expr.is_object()) {
		if (expr.value("expr-type","") == "id" && !constDecls_.count(expr.value("name",""))) {
			ids.push_back(&expr);
			return;
		}
		if (expr.value("expr-type","") == "field-access"
		    && !enumTypeNamed(typeNameWritten(expr["object"])).is_null())
			return;
		for (auto& [k, v] : expr.items()) nonConstNames(v, ids);
	} else if (expr.is_array()) {
		for (auto& v : expr) nonConstNames(v, ids);
	}
}

void PlnSemanticAnalyzer::beginModule(const json& ast)
{
	inputFilePath = ast["original"];
	moduleId_     = filesystem::weakly_canonical(inputFilePath).string();
	symbolScope_  = moduleSymbol(moduleId_);
	sa["original"]      = ast["original"];
	// LCOV_EXCL_EXCEPTION_BR_START
	json u8 = {{"type-kind","prim"},{"type-name","uint8"}};
	sa["main"] = {{"name", symbolScope_ + "..main"}, {"parameters", {
		{{"name","__argc"}, {"var-type", {{"type-kind","prim"},{"type-name","int64"}}}},
		{{"name","__argv"}, {"var-type", {{"type-kind","pntr"},{"base-type", {{"type-kind","pntr"},{"base-type",u8}}}}}},
	}}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
	sa["str-literals"]  = json::array();
	sa["functions"]     = json::array();
	sa["alloc-shapes"]  = json::array();
	sa["libs"]          = json::array();
	sa["objs"]          = json::array();
	program_->modules[moduleId_] = this;
	moduleAstRoot_ = &ast;
	enterScope();
	json argsType = argsStorageType();
	argsType["mutable"] = false;
	declareVar("args", argsType);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Resolve every top-level type and const declaration, and the types
// top-level imports bring in, so function signatures pre-registered in
// step 1 see fully-resolved types, not alias names. A const whose value
// names a variable is left to step 2.
void PlnSemanticAnalyzer::prescanTypes(const json& stmts)
{
	indexTopDecls(stmts);
	// Imports first, so a type they hide is known as hidden by the time a
	// declaration here writes it.
	for (auto& stmt : stmts)
		if (stmt.value("stmt-type", "") == "import")
			for (auto& [key, t] : importedTypes_)
				if (t.importStmt == &stmt) ensureTypeName(key, false);
	for (auto& stmt : stmts) {
		auto d = topDecls_.find(stmt.value("name", ""));
		if (d != topDecls_.end() && d->second.stmt == &stmt) ensureTypeName(d->first, false);
	}
}

// C types depend on no Palan type, so a top-level cinclude's are registered
// here, ahead of any Palan declaration that may use them.
void PlnSemanticAnalyzer::indexTopDecls(const json& stmts)
{
	for (size_t pos = 0; pos < stmts.size(); pos++) {
		const json& stmt = stmts[pos];
		string t = stmt.value("stmt-type", "");
		if (t == "var-decl") {
			for (size_t i = 0; i < stmt["vars"].size(); i++)
				topVars_.emplace(stmt["vars"][i]["name"].get<string>(), TopVar{&stmt, i, (int)pos});
		} else if (t == "struct-def" || t == "enum-def" || t == "type-alias" || t == "const-decl") {
			string name = stmt["name"].get<string>();
			topDecls_.emplace(name, TopDecl{&stmt, TopDecl::Pending, (int)pos});
			if (t == "struct-def" && stmt.value("export", false)) declaredStructs_.insert(name);
		} else if (t == "cinclude") {
			registerCIncludeTypes(stmt);
		} else if (t == "import") {
			filesystem::path impPath;
			json impAst = loadImportAst(stmt, impPath);
			if (impAst.is_null()) continue;
			PlnSemanticAnalyzer& mod = importTypeContext(move(impAst), impPath);
			if (!(*mod.moduleAstRoot_)["ast"].contains("statements")) continue;
			string alias = stmt.value("alias", "");
			set<string> targets;
			if (stmt.contains("targets"))
				for (auto& tg : stmt["targets"]) targets.insert(tg.get<string>());
			for (auto& d : (*mod.moduleAstRoot_)["ast"]["statements"]) {
				if (!d.value("export", false)) continue;
				string name = d["name"].get<string>();
				if (stmt.contains("targets") && !targets.count(name)) continue;
				importedTypes_.emplace(alias.empty() ? name : alias + "." + name, ImportedType{&stmt, &mod, &d});
				if (d["stmt-type"] == "struct-def") declaredStructs_.insert(name);
			}
		}
	}
	// Once every C and imported name is known, so a clash with one is
	// reported at the Palan declaration wherever it is written.
	set<string> vars;
	for (auto& stmt : stmts) {
		addDeclaredVarNames(stmt, vars);
		string t = stmt.value("stmt-type", "");
		if (t != "struct-def" && t != "enum-def" && t != "type-alias" && t != "const-decl") continue;
		string name = stmt["name"].get<string>();
		bool localType = (t == "struct-def" || t == "enum-def") && !stmt.value("export", false);
		bool clash = topDecls_.at(name).stmt != &stmt || isWrittenNameTaken(name);
		if (t == "const-decl") clash = clash || vars.count(name) || findVar(name);
		if (clash) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(
				t == "const-decl" ? E_DuplicateConstDecl : E_DuplicateTypeName, name) << endl;
			exit(1);
		}
		if (localType) declareLocalTypeName(stmt);
	}
}

void PlnSemanticAnalyzer::ensureTypeName(const string& name, bool viaPointer)
{
	auto d = topDecls_.find(name);
	if (d != topDecls_.end()) {
		if (viaPointer && (*d->second.stmt)["stmt-type"] == "struct-def")
			whenSettled([this, name] { ensureTypeName(name, false); });
		else
			resolveTopDecl(name, d->second);
		return;
	}
	auto t = importedTypes_.find(name);
	if (t == importedTypes_.end() || t->second.adopted) return;
	size_t dot = name.find('.');
	string alias = dot == string::npos ? "" : name.substr(0, dot);
	const json& decl = *t->second.decl;
	if (viaPointer && decl["stmt-type"] == "struct-def") {
		// The pointer's type names the struct by its program-wide name now;
		// its layout comes with the adoption.
		if (!alias.empty())
			typeAliases_[name] = {{"type-kind", "prim"}, {"type-name", decl["name"]}};
		ImportedType* it = &t->second;
		whenSettled([this, it, alias] { if (!it->adopted) adoptImportedType(*it, alias); });
		return;
	}
	adoptImportedType(t->second, alias);
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::ensureTypeRefs(const json& node, bool viaPointer)
{
	if (node.is_array()) {
		for (auto& e : node) ensureTypeRefs(e, viaPointer);
		return;
	}
	if (!node.is_object()) return;
	string tk = node.value("type-kind", "");
	if (!tk.empty()) {
		viaPointer = viaPointer || tk == "pntr";
		if (node.contains("type-name") && node["type-name"].is_string())
			ensureTypeName(node["type-name"].get<string>(), viaPointer);
		// A size expression (`[N]@T`) is evaluated whatever the element is.
		for (auto& [k, v] : node.items())
			ensureTypeRefs(v, k == "base-type" && viaPointer);
		return;
	}
	string et = node.value("expr-type", "");
	if (et == "id" || et == "call") {
		if (node.contains("name") && node["name"].is_string())
			ensureTypeName(node["name"].get<string>(), false);
	} else if (et == "field-access") {
		// `V.Color.Red`: the object spells the imported type `V.Color`.
		const json& obj = node["object"];
		if (obj.value("expr-type", "") == "id")
			ensureTypeName(obj["name"].get<string>() + "." + node["field"].get<string>(), false);
	}
	for (auto& [k, v] : node.items()) ensureTypeRefs(v, false);
}

void PlnSemanticAnalyzer::ensureStructDef(const string& name)
{
	for (auto& [written, internal] : localTypeNames_)
		if (internal == name) {
			ensureTypeName(written, false);
			return;
		}
	if (topDecls_.count(name)) {
		ensureTypeName(name, false);
		return;
	}
	for (auto& [key, t] : importedTypes_)
		if ((*t.decl)["name"] == name) {
			ensureTypeName(key, false);
			return;
		}
}

void PlnSemanticAnalyzer::resolveTopDecl(const string& name, TopDecl& d)
{
	if (d.state == TopDecl::Done) return;
	auto& resolving = program_->resolving;
	if (d.state == TopDecl::Resolving) {
		// A struct embedding itself is E_RecursiveStruct's to report.
		if (resolving.back().module == this && resolving.back().name == name) return;
		string path;
		bool inCycle = false;
		for (auto& r : resolving) {
			inCycle = inCycle || (r.module == this && r.name == name);
			if (inCycle) path += r.name + " -> ";
		}
		auto& closer = resolving.back();
		cerr << closer.module->locPrefix(*closer.stmt)
		     << PlnSaMessage::getMessage(E_CircularTypeDecl, name, path + name) << endl;
		exit(1);
	} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here
	d.state = TopDecl::Resolving;
	resolving.push_back({this, name, d.stmt});
	const json& stmt = *d.stmt;
	ensureTypeRefs(stmt);
	string t = stmt["stmt-type"].get<string>();
	bool resolved = true;
	if (t != "const-decl") rejectDeferredConstRefs(stmt, stmt);
	if      (t == "struct-def") sa_struct_def(stmt);
	else if (t == "enum-def")   sa_enum_def(stmt);
	else if (t == "type-alias") sa_type_alias(stmt);
	else resolved = resolveTopConst(d);
	if (resolved) prescannedDecls_.insert(&stmt);
	resolving.pop_back();
	d.state = TopDecl::Done;
	while (resolving.empty() && !program_->settled.empty()) {
		auto f = move(program_->settled.front());
		program_->settled.pop_front();
		f();
	}
}

// A const naming variables is evaluated now only when each is already in
// scope or is a top-level variable declared before it whose declaration fixes
// its type: the value needs only that type, never the initializer. Any other
// is left to its statement, where the variables it names are declared.
bool PlnSemanticAnalyzer::resolveTopConst(TopDecl& d)
{
	const json& stmt = *d.stmt;
	vector<const json*> ids;
	nonConstNames(stmt["value"], ids);
	map<string, json> vars;
	for (const json* id : ids) {
		string name = (*id)["name"].get<string>();
		if (findVar(name)) continue;
		auto v = topVars_.find(name);
		const json* vt = d.pos >= 0 && v != topVars_.end() && v->second.pos < d.pos ? topVarType(name) : nullptr;
		if (!vt) {
			auto c = topDecls_.find(name);
			bool waits = c != topDecls_.end() && !c->second.deferredBy.empty();
			d.deferredBy = waits ? c->second.deferredBy : name;
			return false;
		}
		vars[name] = *vt;
	}
	varScopes.push_back(move(vars));
	sa_const_decl(stmt);
	varScopes.pop_back();
	return true;
}

const json* PlnSemanticAnalyzer::topVarType(const string& name)
{
	TopVar& v = topVars_.at(name);
	if (v.state == TopDecl::Done) return v.type.is_null() ? nullptr : &v.type;
	v.state = TopDecl::Resolving;
	program_->resolving.push_back({this, name, v.stmt});
	const json& var = (*v.stmt)["vars"][v.idx];
	json vt = var["var-type"];
	takeVarStorage(vt);
	// Only an array's length can make a const; any other variable is left to
	// its statement.
	string spec = vt.value("specifier", "");
	if (vt.value("type-kind", "") == "arr" && (spec == "variable" || spec == "raw")) {
		ensureTypeRefs(vt);
		vt = resolveKnownType(*v.stmt, vt);
		// A size known only at run time would have its expression analyzed
		// before the functions it may call are registered.
		bool constSizes = true;
		for (const json* l = &vt; l->value("type-kind", "") == "arr"; l = &(*l)["base-type"]) {
			const json& size = (*l)["size-expr"];
			constSizes = constSizes && (size.is_null() || (!hasCall(size) && !firstNonConstName(size)));
		}
		if (constSizes && var.contains("init") && var["init"].value("expr-type", "") == "arr-lit")
			fillArrLitSizes(*v.stmt, name, vt, var["init"]["items"]);
		if (constSizes && !vt["size-expr"].is_null())
			v.type = arrVarStorage(*v.stmt, vt).type;
	}
	program_->resolving.pop_back();
	v.state = TopDecl::Done;
	return v.type.is_null() ? nullptr : &v.type;
}

// A const left to its statement is unknown until then.
void PlnSemanticAnalyzer::rejectDeferredConstRefs(const json& locNode, const json& node) const
{
	vector<const json*> ids;
	nonConstNames(node, ids);
	for (const json* id : ids) {
		string name = (*id)["name"].get<string>();
		auto c = topDecls_.find(name);
		if (c == topDecls_.end() || c->second.deferredBy.empty()) continue;
		cerr << locPrefix(id->contains("loc") ? *id : locNode)
		     << PlnSaMessage::getMessage(E_ConstUsedBeforeKnown, name, c->second.deferredBy) << endl;
		exit(1);
	}
}

void PlnSemanticAnalyzer::adoptImportedType(ImportedType& t, const string& alias)
{
	adoptExportedType(*t.importStmt, *t.decl, *t.module);
	if (!alias.empty() && (*t.decl)["stmt-type"] == "struct-def")
		typeAliases_[alias + "." + (*t.decl)["name"].get<string>()] =
			{{"type-kind", "prim"}, {"type-name", (*t.decl)["name"]}};
	t.adopted = true;
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::whenSettled(function<void()> f)
{
	if (program_->resolving.empty()) f();
	else program_->settled.push_back(move(f));
}

void PlnSemanticAnalyzer::analysis(const json &ast)
{
	beginModule(ast);
	if (ast["ast"].contains("statements")) {
		prescanTypes(ast["ast"]["statements"]);
		noteImplicitArgs(ast["ast"]["statements"]);
	}
	// 1. Pre-register Palan functions so calls can resolve them
	if (ast["ast"].contains("functions"))
		for (auto& f : ast["ast"]["functions"])
			preregisterFunc(f, symbolScope_);
	// 2. Process top-level statements (cinclude/import registered here,
	//    visible in Palan function bodies processed next).
	sa["statements"] = sa_statements(ast["ast"]["statements"]);
	if (argsUsed_) {
		json stmts = implicitArgsDecl();
		for (auto& s : sa["statements"]) stmts.push_back(move(s));
		sa["statements"] = move(stmts);
	}
	if (pendingAliases_)
		resolvePending(sa["statements"]);
	// 3. Process each function body
	if (ast["ast"].contains("functions"))
		sa_functions(ast["ast"]["functions"]);
	// 4. Emit the libraries collected from cinclude `link` clauses, after
	//    steps 2-3 since a cinclude may sit inside a block or function body.
	for (auto& lib : linkLibs_) sa["libs"].push_back(lib);
	for (auto& obj : linkObjs_) sa["objs"].push_back(obj);
	leaveScope();
}

const json& PlnSemanticAnalyzer::result()
{
	return sa;
}

// Registers a typedef name as an alias, same first-wins/conflict-diagnosed
// policy as a native "type X = ...;" alias.
void PlnSemanticAnalyzer::registerTypeAliasChecked(const json& locNode, const string& aliasName, const json& resolved)
{
	requireNotPalanType(aliasName, locNode, false);
	auto it = typeAliases_.find(aliasName);
	if (it == typeAliases_.end()) {
		typeAliases_[aliasName] = resolved;
	} else if (it->second != resolved) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ConflictingTypedef, aliasName) << endl;
		exit(1);
	}
}

// Recursively find "typedef-name" hints (added by c2ast for scalar typedefs
// like size_t) inside a type node, register the underlying primitive as a
// type alias so Palan code can reference the typedef name, and strip the
// hint so it doesn't leak into sa.json (e.g. via sa_expr_call copying a C
// function's ret-type into a call's value-type).
void PlnSemanticAnalyzer::registerTypedefAliasInType(const json& locNode, json& vtype)
{
	string tk = vtype.value("type-kind", "");
	if (tk == "prim" && vtype.contains("typedef-name")) {
		string aliasName = vtype["typedef-name"].get<string>();
		registerTypeAliasChecked(locNode, aliasName, {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}});
		vtype.erase("typedef-name");
	} else if (isCRecordTag(vtype) && vtype.contains("typedef-name")) {
		// typedef struct/union Tag X (e.g. "typedef struct _IO_FILE FILE;"): register X
		// as a type alias for the tag, same prim(Tag) representation a native
		// "type A = SomeStruct;" alias uses.
		string aliasName = vtype["typedef-name"].get<string>();
		registerTypeAliasChecked(locNode, aliasName, {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}});
		vtype.erase("typedef-name");
	} else if (tk == "pntr" && vtype.contains("base-type")) {
		registerTypedefAliasInType(locNode, vtype["base-type"]);
	} else if (tk == "func") {
		if (vtype.contains("ret-type"))
			registerTypedefAliasInType(locNode, vtype["ret-type"]);
		if (vtype.contains("parameters"))
			for (auto& p : vtype["parameters"])
				if (p.contains("var-type"))
					registerTypedefAliasInType(locNode, p["var-type"]);
	}
} // LCOV_EXCL_BR_LINE -- closing brace of a function with many local json/string temporaries; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

void PlnSemanticAnalyzer::registerCFuncTypedefAliases(const json& locNode, json& funcEntry)
{
	if (funcEntry.contains("ret-type"))
		registerTypedefAliasInType(locNode, funcEntry["ret-type"]);
	if (funcEntry.contains("parameters"))
		for (auto& p : funcEntry["parameters"])
			if (p.contains("var-type"))
				registerTypedefAliasInType(locNode, p["var-type"]);
}

// A `link` library name is concatenated onto "-l" with no separator, so this
// checks it's something `ld -l` can actually consume, diagnosed here rather
// than as an obscure linker failure.
static bool isValidLinkLibName(const string& name)
{
	if (name.empty()) return false;
	for (char c : name) {
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		       || (c >= '0' && c <= '9')
		       || c == '_' || c == '.' || c == '+' || c == '-';
		if (!ok) return false;
	}
	return true;
}

void PlnSemanticAnalyzer::registerCIncludeTypes(const json& stmt)
{
	// Before structs, whose fields can be of an enum type.
	if (stmt.contains("enums"))
		for (auto& e : stmt["enums"])
			registerCEnum(stmt, e);

	if (stmt.contains("structs"))
		for (auto& s : stmt["structs"])
			registerCStruct(stmt, s);

	// Covers a typedef no C function/global actually references, which
	// registerTypedefAliasInType's per-reference-site calls wouldn't reach.
	if (stmt.contains("typedefs"))
		for (auto& td : stmt["typedefs"]) {
			json vt = td["var-type"];
			resolveCTypeRefs(vt);
			// Still "user"/"enum": its underlying type is unknown here too, so
			// there is nothing to alias. Keeping such entries out of cTypedefs_
			// also means a substitution never yields another one to resolve.
			string tk = vt.value("type-kind", "");
			if (tk == "user" || tk == "enum") continue;
			string name = td["name"].get<string>();
			cTypedefs_[name] = vt;
			// LCOV_EXCL_EXCEPTION_BR_START
			if (tk == "pntr") {
				// An unrepresentable pointee (e.g. a function) leaves the name unknown.
				json n = normalizeCType(vt);
				if (unrepresentableTypeName(n).empty())
					registerTypeAliasChecked(stmt, name, cTypeAsAlias(n));
			} else if (vt.contains("enum"))
				registerTypeAliasChecked(stmt, name, vt);
			else
				registerTypeAliasChecked(stmt, name, {{"type-kind", "prim"}, {"type-name", vt.value("type-name", "")}});
			// LCOV_EXCL_EXCEPTION_BR_STOP
		}
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::sa_cinclude(const json &stmt)
{
	if (stmt.contains("libs"))
		for (auto& l : stmt["libs"]) {
			string lib = l.get<string>();
			if (!isValidLinkLibName(lib)) {
				cerr << locPrefix(stmt)
				     << PlnSaMessage::getMessage(E_InvalidLinkLibName, lib) << endl;
				exit(1);
			}
			linkLibs_.insert(lib);
		}
	if (stmt.contains("objs"))
		for (auto& o : stmt["objs"]) {
			string obj = o.get<string>();
			if (!filesystem::is_regular_file(obj)) {
				cerr << locPrefix(stmt)
				     << PlnSaMessage::getMessage(E_LinkObjNotFound, obj) << endl;
				exit(1);
			}
			linkObjs_.insert(obj);
		}

	// Must run before functions: PlnTypeRegistry::fromJson needs structDefs_
	// entries to already exist for a struct-pointer parameter/return type.
	registerCIncludeTypes(stmt);

	if (stmt.contains("globals"))
		for (auto& g : stmt["globals"]) {
			json entry = g;
			registerTypedefAliasInType(stmt, entry["var-type"]);
			resolveCTypeRefs(entry["var-type"]);
			normalizeCGlobal(entry);
			registerCGlobal(entry["name"].get<string>(), entry);
		}

	if (stmt.contains("functions")) {
		if (stmt.contains("alias")) {
			const string& alias = stmt["alias"].get<string>();
			auto& currentScope = importScopes.back();
			for (auto& f : stmt["functions"]) {
				string fname = f["name"].get<string>();
				json entry = f;
				registerCFuncTypedefAliases(stmt, entry);
				resolveCTypeRefs(entry);
				normalizeCFuncSig(entry);
				entry["_c-func"] = true;
				currentScope[alias][fname].push_back(entry);
			}
		} else {
			for (auto& f : stmt["functions"]) {
				json entry = f;
				registerCFuncTypedefAliases(stmt, entry);
				resolveCTypeRefs(entry);
				normalizeCFuncSig(entry);
				registerCFunc(entry["name"].get<string>(), entry);
			}
		}
	}

} // LCOV_EXCL_BR_LINE

static bool is_absolute(filesystem::path &path)
{
	if (path.string()[0] == '/') {
		return true;
	}
	return false;
}

// Null for an import of a non-Palan file.
json PlnSemanticAnalyzer::loadImportAst(const json& stmt, filesystem::path& impPath) const
{
	impPath = stmt["path"].get<string>();
	if (stmt["path-type"] == "src" && !is_absolute(impPath))
		impPath = basePath + '/' + impPath.string();

	if (!impPath.string().ends_with(".pa")) return json();
	filesystem::path astPath = impPath;
	astPath += ".ast.json";

	ifstream astfile(astPath.string());
	if (!astfile.is_open()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ImportFileNotFound, astPath.string()) << endl;
		exit(1);
	}
	return json::parse(astfile);
}

PlnSemanticAnalyzer& PlnSemanticAnalyzer::importTypeContext(json impAst, const filesystem::path& impPath)
{
	auto known = program_->modules.find(filesystem::weakly_canonical(impAst["original"].get<string>()).string());
	if (known != program_->modules.end()) return *known->second;
	auto sub = make_unique<PlnSemanticAnalyzer>(impPath.parent_path().string(), "", c2astPath);
	sub->ownedProgram_.reset();
	sub->program_ = program_;
	sub->moduleAst_ = move(impAst);
	sub->beginModule(sub->moduleAst_);
	PlnSemanticAnalyzer& mod = *sub;
	program_->imported.push_back(move(sub));
	if (mod.moduleAst_["ast"].contains("statements"))
		mod.indexTopDecls(mod.moduleAst_["ast"]["statements"]);
	return mod;
}

void PlnSemanticAnalyzer::sa_import(const json& stmt)
{
	filesystem::path imp_path;
	json imp_ast = loadImportAst(stmt, imp_path);
	if (imp_ast.is_null()) return;

	PlnSemanticAnalyzer& sub = importTypeContext(imp_ast, imp_path);
	adoptExportedTypes(stmt, imp_ast, sub);

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

		// Resolved in the exporting module, where the type names it uses mean
		// what its author meant.
		json funcEntry = sub.normalizeFuncSig(f, moduleSymbol(sub.moduleId_));
		funcEntry["_module"] = sub.moduleId_;
		for (auto& p : funcEntry.value("parameters", json::array()))
			adoptTypeDeps(sub, p["var-type"], stmt);
		for (auto& r : funcEntry.value("rets", json::array()))
			adoptTypeDeps(sub, r["var-type"], stmt);
		if (funcEntry.contains("ret-type")) adoptTypeDeps(sub, funcEntry["ret-type"], stmt);

		currentScope[alias][fname].push_back(funcEntry);
	}
}
