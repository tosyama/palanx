/// Palan Semantic Analyzer — statement, control flow, and function analysis
///
/// @file PlnSaStmt.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

json PlnSemanticAnalyzer::sa_statements(const json& stmts)
{
	json result = json::array();
	for (auto& stmt : stmts) {
		string t = stmt["stmt-type"];
		if      (t == "import")   sa_import(stmt);
		else if (t == "cinclude") sa_cinclude(stmt);
		else if (t == "expr")     result.push_back(sa_expression_stmt(stmt));
		else if (t == "var-decl") { for (auto& s : sa_var_decl(stmt)) result.push_back(s); }
		else if (t == "assign")     result.push_back(sa_assign_stmt(stmt));
		else if (t == "arr-assign")   { for (auto& s : sa_arr_assign_stmt(stmt)) result.push_back(s); }
		else if (t == "struct-def")   sa_struct_def(stmt);
		else if (t == "field-assign") result.push_back(sa_field_assign(stmt));
		else if (t == "return") {
			if (funcBodyScopeIdx_ > 0) {
				if (stmt.contains("values") && stmt["values"].size() == 1
						&& stmt["values"][0].value("expr-type","") == "id")
					removeFromArrayScope(stmt["values"][0]["name"].get<string>());
				json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
				for (auto& s : frees) result.push_back(s);
			}
			result.push_back(sa_return_stmt(stmt));
		}
		else if (t == "tapple-decl") result.push_back(sa_tapple_decl(stmt));
		else if (t == "if")       result.push_back(sa_if_stmt(stmt));
		else if (t == "while")    result.push_back(sa_while_stmt(stmt));
		else if (t == "break") {
			if (!whileScopeStack_.empty()) {
				json frees = collectFreeStmts(whileScopeStack_.back(), arrayScopeVars_.size());
				for (auto& s : frees) result.push_back(s);
			}
			result.push_back(sa_break_stmt(stmt));
		}
		else if (t == "continue") {
			if (!whileScopeStack_.empty()) {
				json frees = collectFreeStmts(whileScopeStack_.back(), arrayScopeVars_.size());
				for (auto& s : frees) result.push_back(s);
			}
			result.push_back(sa_continue_stmt(stmt));
		}
		else if (t == "not-impl")    result.push_back(stmt);
		else if (t == "func-def") {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_InternalError, "1") << endl;
			exit(1);
		}
		else if (t == "block")    result.push_back(sa_block(stmt));
		else {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_InternalError, "2") << endl;
			exit(1);
		}
	}
	return result;
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
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInBlock, f["name"].get<string>()) << endl;
			exit(1);
		}
		json funcEntry = f;
		normalizeUnsizedArrSig(funcEntry);
		validateEmbeddedParams(funcEntry);
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry, &f);
	}

	// analyze block-local func bodies -> appended to sa["functions"]
	for (auto& f : stmt.value("functions", json::array()))
		sa_function(f);

	// analyze body statements (no func-defs)
	json body = sa_statements(stmt["body"]);

	// Append free() for array vars declared in this block (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);

	leaveScope();
	return {{"stmt-type", "block"}, {"body", move(body)}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_if_stmt(const json& stmt)
{
	json result;
	result["stmt-type"] = "if";
	result["cond"] = sa_expression(stmt["cond"]);
	result["then"] = sa_block(stmt["then"]);
	if (stmt.contains("else")) {
		const json& els = stmt["else"];
		if (els["stmt-type"] == "if")
			result["else"] = sa_if_stmt(els);
		else
			result["else"] = sa_block(els);
	}
	if (stmt.contains("loc"))
		result["loc"] = stmt["loc"];
	return result;
}

json PlnSemanticAnalyzer::sa_while_stmt(const json& stmt)
{
	json result;
	result["stmt-type"] = "while";
	result["cond"] = sa_expression(stmt["cond"]);
	enterScope();
	whileScopeStack_.push_back(arrayScopeVars_.size() - 1);
	loopDepth_++;

	json body = sa_statements(stmt["body"]);

	// Append free() for array vars in while body scope (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);

	loopDepth_--;
	whileScopeStack_.pop_back();
	leaveScope();

	result["body"] = move(body);
	if (stmt.contains("loc"))
		result["loc"] = stmt["loc"];
	return result;
}

json PlnSemanticAnalyzer::sa_break_stmt(const json& stmt)
{
	if (loopDepth_ == 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_BreakOutsideLoop) << endl;
		exit(1);
	}
	return {{"stmt-type", "break"}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_continue_stmt(const json& stmt)
{
	if (loopDepth_ == 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ContinueOutsideLoop) << endl;
		exit(1);
	}
	return {{"stmt-type", "continue"}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::sa_function(const json& funcDef)
{
	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes      = varScopes;
	auto savedCurrentFunc    = currentFunc_;
	auto savedArrayScopeVars = arrayScopeVars_;
	auto savedFuncBodyIdx    = funcBodyScopeIdx_;
	bool savedInAllocFunc    = inAllocFunc_;

	string funcName = funcDef["name"].get<string>();
	inAllocFunc_ = (funcName.rfind("__pln_alloc_", 0) == 0);

	varScopes       = {{}};
	arrayScopeVars_ = {{}};  // scope[0] = params; no arrays expected here

	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			declareVar(p["name"], toStructPntrType(unsizedArrToPntr(p["var-type"])), &funcDef);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
			if (!isStructType(r["var-type"]))
				declareVar(r["name"], unsizedArrToPntr(r["var-type"]), &funcDef);

	currentFunc_ = findPlnFunc(funcDef["name"]);
	enterScope();  // push scope[1] = function body
	funcBodyScopeIdx_ = arrayScopeVars_.size() - 1;  // = 1

	const json& blk = funcDef["block"];

	// pre-register inner func-defs (visible only within this function)
	for (auto& f : blk.value("functions", json::array())) {
		if (f.value("export", false)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInFunction, f["name"].get<string>()) << endl;
			exit(1);
		}
		json funcEntry = f;
		normalizeUnsizedArrSig(funcEntry);
		validateEmbeddedParams(funcEntry);
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry, &f);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = funcDef;
	normalizeUnsizedArrSig(saFunc);
	validateEmbeddedParams(saFunc);
	normalizeStructSig(saFunc);
	// Single named return: add ret-type so codegen knows the return type
	if (!saFunc.contains("ret-type") && saFunc.contains("rets") && saFunc["rets"].size() == 1)
		saFunc["ret-type"] = saFunc["rets"][0]["var-type"];

	json body = sa_statements(blk["body"]);

	// Append free() for array vars in function body scope (reverse declaration order)
	json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
	for (auto& s : frees) body.push_back(s);

	saFunc["body"] = move(body);
	saFunc.erase("block");

	leaveScope();
	varScopes        = savedVarScopes;
	arrayScopeVars_  = savedArrayScopeVars;
	funcBodyScopeIdx_ = savedFuncBodyIdx;
	currentFunc_     = savedCurrentFunc;
	inAllocFunc_     = savedInAllocFunc;

	sa["functions"].push_back(saFunc);
}

json PlnSemanticAnalyzer::sa_assign_stmt(const json& stmt)
{
	string name = stmt["name"];
	const json* varType = findVar(name);
	if (varType == nullptr) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UndefinedVariable, name) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(*varType);
	json value = sa_expression(stmt["value"], toType);
	if (!value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	const PlnType* fromType = registry_.fromJson(value["value-type"]);
	TypeCompat compat = typeCompat(fromType, toType, registry_);
	if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast)
		value = wrapConvert(value, registry_.toJson(toType));
	return {{"stmt-type", "assign"}, {"name", name}, {"value", value}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_assign_stmt(const json& stmt)
{
	json sa_target = sa_expression(stmt["target"]);
	const PlnType* toType = registry_.fromJson(sa_target["value-type"]);
	json sa_value = sa_expression(stmt["value"], toType);
	if (!sa_value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	const PlnType* fromType = registry_.fromJson(sa_value["value-type"]);
	TypeCompat compat = typeCompat(fromType, toType, registry_);
	if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast)
		sa_value = wrapConvert(sa_value, registry_.toJson(toType));

	json arr_assign = {{"stmt-type", "arr-assign"}, {"target", sa_target}, {"value", sa_value}};
	if (!stmt.value("ownership-transfer", false))
		return json::array({arr_assign});

	arr_assign["ownership-transfer"] = true;
	json result = json::array({arr_assign});

	// Null out the source variable so free(NULL) at scope end is a no-op.
	if (sa_value.value("expr-type","") == "id" && sa_value.contains("value-type")) {
		result.push_back({
			{"stmt-type", "assign"},
			{"name", sa_value["name"]},
			{"value", {{"expr-type","lit-int"},{"value","0"},{"value-type",sa_value["value-type"]}}}
		});
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_return_stmt(const json& stmt)
{
	if (currentFunc_ == nullptr) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ReturnOutsideFunction) << endl;
		exit(1);
	}
	bool hasRets   = currentFunc_->contains("rets");
	bool hasRetType = currentFunc_->contains("ret-type");
	bool hasValues = stmt.contains("values");

	if (hasRets) {
		// Multiple return values: bare return only
		if (hasValues) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_MultiRetBareReturn) << endl;
			exit(1);
		}
		return {{"stmt-type", "return"}};
	}
	if (hasRetType) {
		// Single return value: exactly one expression required
		if (!hasValues || stmt["values"].size() != 1) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_SingleRetOneExpr) << endl;
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
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidBareReturn) << endl;
		exit(1);
	}
	return {{"stmt-type", "return"}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_tapple_decl(const json& stmt)
{
	const json& callExpr = stmt["value"];
	const json* pFunc = nullptr;
	string fname;

	if (callExpr["expr-type"] == "member-call") {
		const string& alias = callExpr["object"]["name"].get<string>();
		fname = callExpr["method"].get<string>();
		bool aliasFound = false;
		for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
			if (it->count(alias)) { aliasFound = true; break; }
		if (!aliasFound) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownAlias, alias) << endl;
			exit(1);
		}
		pFunc = findImportFuncByAlias(alias, fname);
	} else {
		fname = callExpr["name"].get<string>();
		pFunc = findPlnFunc(fname);
		if (pFunc == nullptr) {
			pFunc = findImportFunc(fname);
			if (pFunc != nullptr && pFunc->empty()) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AmbiguousCall, fname) << endl;
				exit(1);
			}
		}
	}

	if (pFunc == nullptr) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleUndefinedFunction, fname) << endl;
		exit(1);
	}
	if (!pFunc->contains("rets") || (*pFunc)["rets"].size() < 2) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleNeedsMultiRet, fname) << endl;
		exit(1);
	}
	if (stmt["vars"].size() != (*pFunc)["rets"].size()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleVarCountMismatch, fname) << endl;
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
} // LCOV_EXCL_EXCEPTION_BR_LINE

FieldChain PlnSemanticAnalyzer::resolveStoreLocChain(const json& loc)
{
	if (loc.value("kind","") == "var") {
		string varName = loc["name"].get<string>();
		const json* vt = findVar(varName);
		if (!vt) {
			cerr << locPrefix(loc) << PlnSaMessage::getMessage(E_UndefinedVariable, varName) << endl;
			exit(1);
		}
		if (vt->value("type-kind","") != "pntr" || (*vt)["base-type"].value("type-kind","") != "struct") {
			cerr << locPrefix(loc) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		return {false, varName, 0, {}, (*vt)["base-type"]["type-name"].get<string>()};
	}
	FieldChain base = resolveStoreLocChain(loc["base"]);
	string fn = loc["field"].get<string>();
	const StructDef& def = structDefs_[base.structName];
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
	if (it == def.fields.end()) {
		cerr << locPrefix(loc) << PlnSaMessage::getMessage(E_UnknownField, base.structName, fn) << endl;
		exit(1);
	}
	if (it->typeKind == "prim") {
		cerr << locPrefix(loc) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	if (it->typeKind == "raw-ptr" && !it->isMutable) {
		cerr << locPrefix(loc) << PlnSaMessage::getMessage(E_WriteToImmutablePtrField) << endl;
		exit(1);
	}
	if (it->typeKind == "embed") {
		base.offset += it->offset;
		base.structName = it->typeName;
		return base;
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	json pntr_type = {{"type-kind","pntr"},{"base-type",{{"type-kind","struct"},{"type-name",it->typeName}}}};
	if (it->typeKind == "raw-ptr") pntr_type["mutable"] = it->isMutable;
	json ptrNode;
	if (!base.isPointerBased)
		ptrNode = {{"expr-type","field-access"},{"var",base.varName},
		           {"offset",base.offset+it->offset},{"value-type",pntr_type}};
	else
		ptrNode = {{"expr-type","field-access"},{"ptr-expr",base.ptrExpr},
		           {"offset",base.offset+it->offset},{"value-type",pntr_type}};
	return {true, "", 0, move(ptrNode), it->typeName};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_field_assign(const json& stmt)
{
	FieldChain chain = resolveStoreLocChain(stmt["object"]);
	string fn = stmt["field"].get<string>();
	const StructDef& def = structDefs_[chain.structName];
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
	if (it == def.fields.end()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownField, chain.structName, fn) << endl;
		exit(1);
	}
	json fieldType = fieldValueType(*it);
	const PlnType* toType = registry_.fromJson(fieldType);
	json value = sa_expression(stmt["value"], toType);
	if (!value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	const PlnType* fromType = registry_.fromJson(value["value-type"]);
	TypeCompat compat = typeCompat(fromType, toType, registry_);
	if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast)
		value = wrapConvert(value, fieldType);
	// LCOV_EXCL_EXCEPTION_BR_START
	int off = chain.offset + it->offset;
	if (!chain.isPointerBased)
		return {{"stmt-type","field-assign"},{"var",chain.varName},
		        {"offset",off},{"value-type",fieldType},{"value",move(value)}};
	else
		return {{"stmt-type","field-assign"},{"ptr-expr",chain.ptrExpr},
		        {"offset",off},{"value-type",fieldType},{"value",move(value)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE
