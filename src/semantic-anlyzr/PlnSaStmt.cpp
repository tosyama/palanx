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
	pushStructDefNames(stmts);
	for (auto& stmt : stmts) {
		string t = stmt["stmt-type"];
		if      (t == "import")   sa_import(stmt);
		else if (t == "cinclude") sa_cinclude(stmt);
		else if (t == "expr")     result.push_back(sa_expression_stmt(stmt));
		else if (t == "var-decl") { for (auto& s : sa_var_decl(stmt)) result.push_back(s); }
		else if (t == "assign")     result.push_back(sa_assign_stmt(stmt));
		else if (t == "arr-assign")   { for (auto& s : sa_arr_assign_stmt(stmt)) result.push_back(s); }
		else if (t == "struct-def")   sa_struct_def(stmt);
		else if (t == "type-alias")   sa_type_alias(stmt);
		else if (t == "const-decl")   sa_const_decl(stmt);
		else if (t == "field-assign") result.push_back(sa_field_assign(stmt));
		else if (t == "return") {
			if (funcBodyScopeIdx_ > 0) {
				if (stmt.contains("values") && stmt["values"].size() == 1
						&& stmt["values"][0].value("expr-type","") == "id")
					removeFromArrayScope(stmt["values"][0]["name"].get<string>());
				json ret = sa_return_stmt(stmt);
				json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
				// The return value may read what the frees release, so it is
				// evaluated into a temp first.
				if (!frees.empty() && ret.contains("values"))
					result.push_back(bindReturnValueToTemp(stmt, ret));
				for (auto& s : frees) result.push_back(s);
				result.push_back(ret);
			} else
				result.push_back(sa_return_stmt(stmt));
		}
		else if (t == "tapple-decl") result.push_back(sa_tapple_decl(stmt));
		else if (t == "tapple-assign") { for (auto& s : sa_tapple_assign(stmt)) result.push_back(s); }
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
		else if (t == "not-impl") {
			if (stmt.contains("untyped-var"))
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VarTypeInferenceNotImpl, stmt["untyped-var"]) << endl;
			else
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
			exit(1);
		}
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
	structDefNameScopes_.pop_back();
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
		preregisterFunc(f, &f);
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
	// A syscall declaration is a prototype: no block to analyze, and nothing to emit.
	if (funcDef.value("func-type", "") == "syscall") return;

	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes      = varScopes;
	auto savedCurrentFunc    = currentFunc_;
	auto savedArrayScopeVars = arrayScopeVars_;
	auto savedFuncBodyIdx    = funcBodyScopeIdx_;

	varScopes       = {{}};
	arrayScopeVars_ = {{}};  // scope[0] = params; no arrays expected here

	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			declareVar(p["name"], deepNormalizePrimToStruct(toStructPntrType(normalizeArrBorrowType(funcDef, unsizedArrToPntr(resolveTypeAlias(p["var-type"]))))), &funcDef);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"]) {
			if (!isStructType(resolveTypeAlias(r["var-type"]))) {
				declareVar(r["name"], deepNormalizePrimToStruct(unsizedArrToPntr(resolveTypeAlias(r["var-type"]))), &funcDef);
			} else if (r.contains("init")) {
				// A struct-type named return is declared by the body itself, so there's no variable to initialize here.
				cerr << locPrefix(r["init"]) << PlnSaMessage::getMessage(E_NamedRetInitOnStruct, r["name"].get<string>()) << endl;
				exit(1);
			}
		}

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
		preregisterFunc(f, &f);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = funcDef;
	normalizeUnsizedArrSig(saFunc);
	normalizeArrBorrowSig(saFunc);
	validateEmbeddedParams(saFunc);
	normalizeStructSig(saFunc);
	validateNativeSig(saFunc);
	// Single named return: add ret-type so codegen knows the return type
	if (!saFunc.contains("ret-type") && saFunc.contains("rets") && saFunc["rets"].size() == 1)
		saFunc["ret-type"] = saFunc["rets"][0]["var-type"];

	json body = json::array();
	if (saFunc.contains("rets"))
		for (auto& r : saFunc["rets"]) {
			if (!r.contains("init")) continue;
			json assign = {{"stmt-type", "assign"}, {"name", r["name"]}, {"value", r["init"]},
			               {"loc", r["init"].value("loc", json::array())}};
			body.push_back(sa_assign_stmt(assign));
			r.erase("init");
		}
	for (auto& s : sa_statements(blk["body"])) body.push_back(move(s));

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

	sa["functions"].push_back(saFunc);
}

json PlnSemanticAnalyzer::sa_assign_stmt(const json& stmt)
{
	string name = stmt["name"];
	const json* varType = findVar(name);
	if (varType == nullptr) {
		if (findCGlobal(name) != nullptr) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_CGlobalNotAssignable, name) << endl;
			exit(1);
		}
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UndefinedVariable, name) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(*varType);
	json value = sa_expression(stmt["value"], toType);
	if (!value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	// A borrowed array variable ("mutable" on its levels) is rebound to what
	// '@x' names; an expiring value is moved in as it is.
	if (isCopiedByValue(*varType) && !varType->contains("mutable")
	    && value.value("category", "") != "expiring") {
		json dst = {{"expr-type","id"},{"name",name},{"var-type",*varType},{"value-type",*varType}};
		return makeCopyStmt(stmt, dst, value);
	}
	if (varType->contains("arr-size") && !isInArrayScope(name))
		checkArrBorrowBinding(stmt, stmt["value"], value, *varType);
	value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
	checkStructBorrowSource(stmt, value, *varType);
	if (!ptrPermissionOk(value["value-type"], *varType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
		exit(1);
	}
	return {{"stmt-type", "assign"}, {"name", name}, {"value", value}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_assign_stmt(const json& stmt)
{
	json sa_target = sa_expr_arr_index(stmt["target"], /*forWrite=*/true);
	bool transfer = stmt.value("ownership-transfer", false);
	if (!isWritableThrough(sa_target["array"]["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
		exit(1);
	}
	json sa_value;
	if (!transfer && isCopiedByValue(sa_target["value-type"])) {
		sa_value = sa_expression(stmt["value"]);
		if (sa_value.value("category", "") != "expiring")
			return json::array({makeCopyStmt(stmt, sa_target, sa_value)});
	}
	if (sa_target.value("addr-only", false)) {
		// The element itself is an address computation (e.g. a struct array
		// element), not a storage slot to overwrite -- assign to its fields instead.
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AssignToWholeStructElem) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(sa_target["value-type"]);
	if (sa_value.is_null())
		sa_value = sa_expression(stmt["value"], toType);
	if (!sa_value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	sa_value = convertForBinding(stmt, sa_value, toType, registry_.toJson(toType));
	// '->>' hands the struct over rather than borrowing it.
	if (!transfer)
		checkStructBorrowSource(stmt, sa_value, sa_target["value-type"]);
	if (!ptrPermissionOk(sa_value["value-type"], sa_target["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
		exit(1);
	}

	json arr_assign = {{"stmt-type", "arr-assign"}, {"target", sa_target}, {"value", sa_value}};
	if (!transfer)
		return json::array({arr_assign});

	arr_assign["ownership-transfer"] = true;
	json result = json::array({arr_assign});

	// Null out the source variable so its scope-exit free is a no-op: C free and
	// the build-mgr generated __pln_free_* functions all accept NULL.
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
			value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
			checkStructBorrowSource(stmt, value, (*currentFunc_)["ret-type"]);
			if (!ptrPermissionOk(value["value-type"], (*currentFunc_)["ret-type"])) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
				exit(1);
			}
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

// Moves ret's single value into a fresh temp var; returns that var's decl and
// rewrites ret to return the temp.
json PlnSemanticAnalyzer::bindReturnValueToTemp(const json& stmt, json& ret)
{
	json& value = ret["values"][0];
	json type = value.contains("value-type") ? value["value-type"] : (*currentFunc_)["ret-type"];
	string name = "__ret_" + to_string(tempVarCounter_++);
	declareVar(name, type, &stmt);
	json decl = {
		{"stmt-type", "var-decl"},
		{"vars", json::array({{{"name", name}, {"var-type", type}, {"init", value}}})}
	};
	value = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
	return decl;
} // LCOV_EXCL_EXCEPTION_BR_LINE

const json& PlnSemanticAnalyzer::findMultiRetFunc(const json& stmt, size_t recvCount)
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
	if (recvCount != (*pFunc)["rets"].size()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleVarCountMismatch, fname) << endl;
		exit(1);
	}
	return *pFunc;
}

json PlnSemanticAnalyzer::sa_tapple_decl(const json& stmt)
{
	const json& rets = findMultiRetFunc(stmt, stmt["vars"].size())["rets"];

	// Process the call expression via sa_expression (resolves func-type, annotates args)
	json saCall = sa_expression(stmt["value"]);

	// Add multi-return value-types from the function's rets
	json valueTypes = json::array();
	for (auto& r : rets)
		valueTypes.push_back(r["var-type"]);
	saCall["value-types"] = valueTypes;

	// Register declared variables in the symbol table
	for (size_t i = 0; i < stmt["vars"].size(); i++)
		declareVar(stmt["vars"][i]["var-name"].get<string>(), rets[i]["var-type"]);

	return {{"stmt-type", "tapple-decl"}, {"vars", stmt["vars"]}, {"value", saCall}};
} // LCOV_EXCL_EXCEPTION_BR_LINE

// All return values are received into temps first, then assigned left to
// right through the single-target assignment paths, so `f() -> (i, arr[i])`
// indexes with the new i.
json PlnSemanticAnalyzer::sa_tapple_assign(const json& stmt)
{
	const json& targets = stmt["targets"];
	const json& rets = findMultiRetFunc(stmt, targets.size())["rets"];

	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", stmt["value"]},
	             {"loc", stmt["loc"]}};
	vector<string> temps;
	for (auto& r : rets) {
		temps.push_back("__tap_" + to_string(tempVarCounter_++));
		decl["vars"].push_back({{"var-name", temps.back()}, {"var-type", r["var-type"]}});
	}
	json result = json::array({sa_tapple_decl(decl)});

	for (size_t i = 0; i < targets.size(); i++) {
		const json& t = targets[i];
		json value = {{"expr-type", "id"}, {"name", temps[i]}, {"loc", t["loc"]}};
		string et = t["expr-type"];
		if (et == "id") {
			result.push_back(sa_assign_stmt({{"stmt-type", "assign"}, {"name", t["name"]},
			                                 {"value", value}, {"loc", t["loc"]}}));
		} else if (et == "arr-index") {
			for (auto& s : sa_arr_assign_stmt({{"stmt-type", "arr-assign"}, {"target", t},
			                                   {"value", value}, {"loc", t["loc"]}}))
				result.push_back(s);
		} else {
			result.push_back(sa_field_assign({{"stmt-type", "field-assign"}, {"object", t["object"]},
			                                  {"field", t["field"]}, {"value", value}, {"loc", t["loc"]}}));
		}
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_field_assign(const json& stmt)
{
	FieldChain chain = resolveObjectChain(stmt["object"], /*forWrite=*/true);
	string fn = stmt["field"].get<string>();
	const StructDef& def = requireCompleteStruct(chain.structName, stmt);
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
	if (it == def.fields.end()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownField, chain.structName, fn, def.keyword()) << endl;
		exit(1);
	}
	json fieldType = fieldValueType(*it);
	const PlnType* toType = registry_.fromJson(fieldType);
	json value = sa_expression(stmt["value"], toType);
	if (!value.contains("value-type")) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	if (isCopiedByValue(fieldType) && value.value("category", "") != "expiring")
		return makeCopyStmt(stmt, makeFieldAccess(chain, *it), value);
	value = convertForBinding(stmt, value, toType, fieldType);
	checkStructBorrowSource(stmt, value, fieldType);
	if (!ptrPermissionOk(value["value-type"], fieldType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
		exit(1);
	}
	return makeFieldAssign(chain, *it, move(value));
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::makeFieldAssign(const FieldChain& chain, const FieldLayout& field, json value)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	int off = chain.offset + field.offset;
	json fieldType = fieldValueType(field);
	if (!chain.isPointerBased)
		return {{"stmt-type","field-assign"},{"var",chain.varName},
		        {"offset",off},{"value-type",fieldType},{"value",move(value)}};
	else
		return {{"stmt-type","field-assign"},{"ptr-expr",chain.ptrExpr},
		        {"offset",off},{"value-type",fieldType},{"value",move(value)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::makeCopyStmt(const json& locNode, const json& dst, const json& src)
{
	const json& t = dst["value-type"];
	const json& st = src["value-type"];
	// A lone struct may be copied from a '@T' as well as from another struct's storage.
	bool match = isStructStorage(t)
		? isStructPntr(st) && st["base-type"] == t["base-type"]
		: copyShapeMatch(st, t);
	if (!match) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyShapeMismatch,
			arrShapeName(t), arrShapeName(st)) << endl;
		exit(1);
	}

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	auto lit = [&](int64_t v) -> json {
		return {{"expr-type","lit-uint"},{"value",to_string(v)},{"value-type",uint64_type}};
	};
	auto callStmt = [&](const string& name, const string& funcType, json args) -> json {
		return {{"stmt-type","expr"},{"body",{
			{"expr-type","call"},{"name",name},{"func-type",funcType},{"args",move(args)}}}};
	};
	auto memcpyStmt = [&](int64_t bytes) {
		return callStmt("memcpy", "c", json::array({dst, src, lit(bytes)}));
	};
	auto structHasOwned = [&](const string& name) {
		const StructDef& def = requireCompleteStruct(name, locNode);
		return def.ownsFields;
	};
	// LCOV_EXCL_EXCEPTION_BR_STOP

	if (isStructStorage(t)) {
		string name = t["base-type"]["type-name"];
		if (!structHasOwned(name))
			return memcpyStmt(structDefs_[name].totalSize);
		recordAllocShape(name);
		return callStmt("__pln_copy_" + name, "palan", json::array({dst, src}));
	}

	int64_t n = t["arr-size"];
	const json& elem = t["base-type"];
	if (t.value("embedded", false)) {
		int64_t rowBytes = t.contains("stride") ? t["stride"].get<int64_t>()
			: t["inner-size"].get<int64_t>() * elemSizeBytes(elem["type-name"]);
		return memcpyStmt(n * rowBytes);
	}
	if (isStructStorage(elem)) {
		string name = elem["base-type"]["type-name"];
		recordArrStructShape(name);
		return callStmt("__pln_copy_arr_" + name, "palan", json::array({dst, src, lit(n)}));
	}
	if (isArrLevel(elem)) {
		int64_t m = elem["arr-size"];
		const json& leaf = elem["base-type"];
		if (elem.value("embedded", false) && elem.contains("stride")) {
			// [n][m]$T: rows are byte blocks, allocated as arr_arr_uint8.
			recordArrArrShape("uint8");
			return callStmt("__pln_copy_arr_arr_uint8", "palan",
				json::array({dst, src, lit(n), lit(m * elem["stride"].get<int64_t>())}));
		}
		if (isStructStorage(leaf)) {
			string name = leaf["base-type"]["type-name"];
			recordArrArrStructShape(name);
			return callStmt("__pln_copy_arr_arr_" + name, "palan", json::array({dst, src, lit(n), lit(m)}));
		}
		if (leaf.value("type-kind","") == "prim" && !elem.value("embedded", false)) {
			string leafName = leaf["type-name"];
			recordArrArrShape(leafName);
			return callStmt("__pln_copy_arr_arr_" + leafName, "palan", json::array({dst, src, lit(n), lit(m)}));
		}
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyUnsupportedShape, arrShapeName(t)) << endl;
		exit(1);
	}
	// Primitive elements, or '@T' slots whose pointers are copied as they are.
	int64_t elemBytes = isPtrBorrow(elem) ? 8 : elemSizeBytes(elem["type-name"]);
	return memcpyStmt(n * elemBytes);
} // LCOV_EXCL_EXCEPTION_BR_LINE
