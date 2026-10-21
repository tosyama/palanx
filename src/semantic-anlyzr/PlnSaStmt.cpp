/// Palan Semantic Analyzer — statement, control flow, and function analysis
///
/// @file PlnSaStmt.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <functional>
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
		else if (t == "expr" && chainsFromStore(stmt["body"])) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (t == "expr")     result.push_back(sa_expression_stmt(stmt));
		else if (t == "var-decl") { for (auto& s : sa_var_decl(stmt)) result.push_back(s); }
		else if (isStoreStmt(stmt)) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (prescannedDecls_.count(&stmt)) continue;
		else if (t == "struct-def")   sa_struct_def(stmt);
		else if (t == "type-alias")   sa_type_alias(stmt);
		else if (t == "const-decl")   sa_const_decl(stmt);
		else if (t == "enum-def")     sa_enum_def(stmt);
		else if (t == "return") {
			if (funcBodyScopeIdx_ > 0) {
				json retStmt = stmt;
				for (auto& s : bindReturnStructLit(retStmt)) result.push_back(s);
				if (retStmt.contains("values") && retStmt["values"].size() == 1
						&& retStmt["values"][0].value("expr-type","") == "id") {
					string name = retStmt["values"][0]["name"];
					if (isFrameVar(name) && !isStructStorage(*findVar(name))) {
						cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ReturnFrameArr, name) << endl;
						exit(1);
					}
				}
				bool ownedVar = retStmt.contains("values") && retStmt["values"].size() == 1
						&& retStmt["values"][0].value("expr-type","") == "id"
						&& isOwnedVar(retStmt["values"][0]["name"].get<string>());
				if (ownedVar)
					removeFromArrayScope(retStmt["values"][0]["name"].get<string>());
				json ret = sa_return_stmt(retStmt);
				if (!ownedVar)
					for (auto& s : copyReturnStruct(retStmt, ret)) result.push_back(s);
				json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
				// The return value may read what the frees release, so it is
				// evaluated into a temp first.
				if (!frees.empty() && ret.contains("values"))
					result.push_back(bindReturnValueToTemp(retStmt, ret));
				for (auto& s : frees) result.push_back(s);
				result.push_back(ret);
			} else
				result.push_back(sa_return_stmt(stmt)); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		else if (t == "tapple-decl") { for (auto& s : sa_tapple_decl(stmt)) result.push_back(s); }
		else if (t == "tapple-assign" && chainsFromStore(stmt["value"])) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (t == "tapple-assign") result.push_back(sa_tapple_assign(stmt));
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
	return result;
}

void PlnSemanticAnalyzer::sa_functions(const json& funcs)
{
	for (auto& f : funcs) sa_function(f);
}

json PlnSemanticAnalyzer::sa_block(const json& stmt)
{
	enterScope();
	// Before the functions, whose signatures may name the block's types.
	enterBlockDecls(stmt["body"]);

	const json& funcs = stmt.value("functions", json::array());
	string savedSymbolScope = symbolScope_;
	if (!funcs.empty())
		symbolScope_ += "..b" + to_string(++blockSymbolSeq_);

	// pre-register block-local func-defs (forward reference support)
	for (auto& f : funcs) {
		if (f.value("export", false)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInBlock, f["name"].get<string>()) << endl;
			exit(1);
		}
		preregisterFunc(f, symbolScope_);
	}

	// analyze block-local func bodies -> appended to sa["functions"]
	for (auto& f : funcs)
		sa_function(f);
	symbolScope_ = savedSymbolScope;

	// analyze body statements (no func-defs)
	json body = sa_statements(stmt["body"]);

	// Append free() for array vars declared in this block (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);

	leaveBlockDecls();
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
	enterBlockDecls(stmt["body"]);

	json body = sa_statements(stmt["body"]);

	// Append free() for array vars in while body scope (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);

	leaveBlockDecls();
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
	// Resolved here, before the body's declarations are in scope.
	const json sig = resolveSigTypes(funcDef);

	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes      = varScopes;
	auto savedCurrentFunc    = currentFunc_;
	auto savedCurrentFuncDef = currentFuncDef_;
	auto savedArrayScopeVars = arrayScopeVars_;
	auto savedFuncBodyIdx    = funcBodyScopeIdx_;
	auto savedEscapedVars    = move(escapedVars_);
	auto savedPendingAliases = pendingAliases_;
	auto savedSymbolScope    = symbolScope_;
	auto savedBlockSymbolSeq = blockSymbolSeq_;
	escapedVars_.clear();
	pendingAliases_ = 0;

	varScopes       = {{}};
	arrayScopeVars_ = {{}};  // scope[0] = params; no arrays expected here

	if (sig.contains("parameters"))
		for (auto& p : sig["parameters"])
			declareVar(p["name"], deepNormalizePrimToStruct(toStructPntrType(normalizeArrBorrowType(funcDef, unsizedArrToPntr(funcDef, p["var-type"])))), &funcDef);
	if (sig.contains("rets"))
		for (auto& r : sig["rets"]) {
			if (!isStructType(r["var-type"])) {
				declareVar(r["name"], deepNormalizePrimToStruct(unsizedArrToPntr(funcDef, r["var-type"])), &funcDef);
			} else if (r.contains("init")) {
				// A struct-type named return is allocated at body entry, where an initializer can't run yet.
				cerr << locPrefix(r["init"]) << PlnSaMessage::getMessage(E_NamedRetInitOnStruct, r["name"].get<string>()) << endl;
				exit(1);
			}
		}

	currentFunc_ = findPlnFuncDef(funcDef);
	currentFuncDef_ = &sig;
	symbolScope_ = (*currentFunc_)["symbol"];
	blockSymbolSeq_ = 0;
	enterScope();  // push scope[1] = function body
	funcBodyScopeIdx_ = arrayScopeVars_.size() - 1;  // = 1

	const json& blk = funcDef["block"];
	enterBlockDecls(blk["body"]);

	// pre-register inner func-defs (visible only within this function)
	for (auto& f : blk.value("functions", json::array())) {
		if (f.value("export", false)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInFunction, f["name"].get<string>()) << endl;
			exit(1);
		}
		preregisterFunc(f, symbolScope_);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = sig;
	saFunc["name"] = (*currentFunc_)["symbol"];
	normalizeUnsizedArrSig(saFunc);
	normalizeArrBorrowSig(saFunc);
	validateEmbeddedParams(saFunc);
	normalizeStructSig(saFunc);
	validateNativeSig(saFunc);
	// Single named return: add ret-type so codegen knows the return type
	if (!saFunc.contains("ret-type") && saFunc.contains("rets") && saFunc["rets"].size() == 1)
		saFunc["ret-type"] = saFunc["rets"][0]["var-type"];

	json body = json::array();
	if (sig.contains("rets")) {
		auto& bodyScopeVars = arrayScopeVars_[funcBodyScopeIdx_];
		size_t nonRetVars = bodyScopeVars.size();
		for (auto& r : sig["rets"]) {
			if (!isStructType(r["var-type"])) continue;
			json decl = {{"stmt-type", "var-decl"},
			             {"vars", json::array({{{"name", r["name"]}, {"var-type", r["var-type"]}}})},
			             {"loc", r.value("loc", json::array())}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			for (auto& s : sa_resolved_var_decl(decl)) body.push_back(move(s));
		}
		// The caller takes over the named returns, so they are not freed here.
		bodyScopeVars.resize(nonRetVars);
	}
	if (saFunc.contains("rets"))
		for (auto& r : saFunc["rets"]) {
			if (!r.contains("init")) continue;
			json assign = {{"stmt-type", "assign"}, {"name", r["name"]}, {"value", r["init"]},
			               {"loc", r["init"].value("loc", json::array())}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			for (auto& st : sa_assign_stmt(assign)) body.push_back(move(st));
			r.erase("init");
		}
	for (auto& s : sa_statements(blk["body"])) body.push_back(move(s));

	// Append free() for array vars in function body scope (reverse declaration order)
	json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
	for (auto& s : frees) body.push_back(s);

	if (pendingAliases_)
		resolvePending(body);
	saFunc["body"] = move(body);
	saFunc.erase("block");

	leaveBlockDecls();
	leaveScope();
	varScopes        = savedVarScopes;
	arrayScopeVars_  = savedArrayScopeVars;
	funcBodyScopeIdx_ = savedFuncBodyIdx;
	currentFunc_     = savedCurrentFunc;
	currentFuncDef_  = savedCurrentFuncDef;
	escapedVars_     = move(savedEscapedVars);
	pendingAliases_  = savedPendingAliases;
	symbolScope_     = savedSymbolScope;
	blockSymbolSeq_  = savedBlockSymbolSeq;

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
	if (isAggregateLit(stmt["value"]) && (isStructPntr(*varType) || isArrLevel(*varType)))
		return json::array({sa_aggregate_lit_assign(stmt, {{"expr-type", "id"}, {"name", name}}, *varType)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	const PlnType* toType = registry_.fromJson(*varType);
	if (stmt.value("ownership-transfer", false))
		return sa_transfer_to_var(stmt, name, *varType, toType);
	json value = sa_expression(stmt["value"], toType);
	// A borrowed array variable ("mutable" on its levels) is rebound to what
	// '@x' names.
	if (isCopiedByValue(*varType) && !varType->contains("mutable")) {
		json dst = {{"expr-type","id"},{"name",name},{"var-type",*varType},{"value-type",*varType}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		return json::array({makeCopyStmt(stmt, dst, value)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	if (varType->contains("arr-size") && !isOwnedVar(name))
		checkArrBorrowBinding(stmt, stmt["value"], value, *varType);
	value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
	checkBorrowSource(stmt, value, *varType);
	checkPtrPermission(stmt, value["value-type"], *varType);
	return json::array({{{"stmt-type", "assign"}, {"name", name}, {"value", value}}}); // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The variable releases what it held the way its scope exit would, which
// knows its sizes, so the value must have those same sizes.
json PlnSemanticAnalyzer::sa_transfer_to_var(const json& stmt, const string& name, const json& varType,
                                             const PlnType* toType)
{
	if (!isOwnedVar(name)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToNonOwnedVar, name) << endl;
		exit(1);
	}
	const json& src = stmt["value"];
	if (src.value("expr-type", "") == "id" && src["name"] == name) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToSelf, name) << endl;
		exit(1);
	}
	json value = sa_expression(src, toType);
	if (!copyShapeMatch(value["value-type"], varType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferShapeMismatch,
			typeDisplayName(varType), typeDisplayName(value["value-type"])) << endl;
		exit(1);
	}
	json store = {{"stmt-type", "assign"}, {"name", name}, {"value", value}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return transferStmts(stmt, scopeRelease(name), move(store), value);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The key of the location a store writes to; none for a plain variable.
static const char* storeTargetKey(const json& stmt)
{
	string t = stmt["stmt-type"];
	if (t == "arr-assign") return "target";
	if (t == "field-assign") return "object";
	return nullptr;
}

static bool isLocationExpr(const json& e)
{
	string et = e.value("expr-type", "");
	return et == "id" || et == "arr-index" || et == "field-access";
}

// A store needs a scope for temps when its value is a store whose target is
// read back, when its target has a call (the value is evaluated before it, and
// a '->>' target is freed and stored to through one evaluation), or when '->>'
// resets the location it read.
static bool storePins(const json& stmt)
{
	const json& v = stmt["value"];
	if (chainsFromStore(v))
		return true;
	const char* target = storeTargetKey(stmt);
	if (target && hasCall(stmt[target]))
		return true;
	return stmt.value("ownership-transfer", false) && hasCall(v) && isLocationExpr(v);
}

// Whether stmts declare a temp at their own level, so they need a block.
static bool declaresTemp(const json& stmts)
{
	for (auto& st : stmts) {
		string t = st.value("stmt-type", "");
		if (t == "var-decl" || (t == "pending-alias" && declaresTemp(st["unaliased"])))
			return true;
	}
	return false;
}

// Stores run left to right, each evaluating every call in it once. A
// statement whose value chains from a store runs that store first, and one
// that evaluates a call into a temp holds the temp in a block of its own.
json PlnSemanticAnalyzer::sa_store_scope(const json& stmt)
{
	string t = stmt["stmt-type"];
	if (isStoreStmt(stmt) && !storePins(stmt))
		return sa_store_stmt(stmt);
	enterScope();
	json body = json::array();
	if (t == "expr") {
		json s = stmt;
		hoistStores(s["body"], body);
		body.push_back(sa_expression_stmt(s));
	} else if (t == "tapple-assign") {
		json s = stmt;
		hoistStores(s["value"], body);
		body.push_back(sa_tapple_assign(s));
	} else {
		sa_store_steps(stmt, body, false);
	}
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	leaveScope();
	if (!declaresTemp(body) && frees.empty())
		return body;
	for (auto& f : frees)
		body.push_back(move(f));
	return json::array({{{"stmt-type", "block"}, {"body", move(body)}}}); // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_store_stmt(const json& stmt, json* litAddr)
{
	string t = stmt["stmt-type"];
	if (t == "assign")
		return sa_assign_stmt(stmt);
	if (t == "arr-assign")
		return sa_arr_assign_stmt(stmt, litAddr);
	return sa_field_assign(stmt, litAddr);
}

// Runs the store `value` is, or the store a chained call's value starts from,
// and replaces it with the target it stored, read back.
void PlnSemanticAnalyzer::hoistStores(json& value, json& body)
{
	if (isStoreStmt(value))
		value = sa_store_steps(value, body, true);
	else if (value.value("expr-type", "") == "chain-call")
		hoistStores(value["value"], body);
}

// Appends the statements of one store to body and returns its target as an
// expression. The value is evaluated before the calls in the target, and a
// target read back, or one '->>' frees before storing to, has its calls
// evaluated into temps first so every use is of the same location.
json PlnSemanticAnalyzer::sa_store_steps(const json& stmt, json& body, bool readBack)
{
	json s = stmt;
	hoistStores(s["value"], body);
	json& value = s["value"];
	const char* target = storeTargetKey(s);
	bool targetCall = target && hasCall(s[target]);
	bool transfer = s.value("ownership-transfer", false);
	// '->>' reads the location it gives away again to reset it.
	if (hasCall(value) && isLocationExpr(value) && (targetCall || transfer))
		value = pinLocation(move(value), body);
	bool litReadBack = readBack && targetCall && isAggregateLit(value);
	json targetPins = json::array();
	if (targetCall && !litReadBack && (readBack || transfer))
		s[target] = pinLocation(move(s[target]), targetPins);

	json sFirst = s;
	json pre = json::array(), post = json::array();
	set<string> roots;
	bool first = targetCall && !isAggregateLit(value) && valueFirst(sFirst, pre, post, roots);

	json litAddr;
	json plain = targetPins;
	if (!first || !roots.empty())
		for (auto& r : sa_store_stmt(s, litReadBack ? &litAddr : nullptr))
			plain.push_back(move(r));
	if (!first) {
		for (auto& p : plain)
			body.push_back(move(p));
	} else {
		// LCOV_EXCL_EXCEPTION_BR_START
		json aliased = pre;
		for (auto& p : targetPins)
			aliased.push_back(p);
		for (auto& r : sa_store_stmt(sFirst))
			aliased.push_back(move(r));
		for (auto& f : post)
			aliased.push_back(move(f));
		// Only pins read back must outlive the store.
		if (!readBack)
			aliased = json::array({{{"stmt-type", "block"}, {"body", move(aliased)}}});
		if (roots.empty()) {
			for (auto& a : aliased)
				body.push_back(move(a));
		} else {
			pendingAliases_++;
			body.push_back({{"stmt-type", "pending-alias"}, {"vars", roots},
			                {"aliased", move(aliased)}, {"unaliased", move(plain)}});
		}
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}

	if (litReadBack)
		return litAddr;
	string t = s["stmt-type"];
	json readExpr;
	if (t == "assign")
		readExpr = {{"expr-type", "id"}, {"name", s["name"]}};
	else if (t == "arr-assign")
		return s["target"];
	else
		readExpr = {{"expr-type", "field-access"}, {"object", s["object"]}, {"field", s["field"]}};
	if (s.contains("loc")) readExpr["loc"] = s["loc"];
	return readExpr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Rewrites s so its value is evaluated into pre, ahead of the target's calls,
// with post releasing what pre holds; false when there is nothing to evaluate
// ahead. When all the value reads is the storage of local variables, roots
// gets them: the value can change only if one of them has given a pointer to
// its storage away, which the end of the function tells.
bool PlnSemanticAnalyzer::valueFirst(json& s, json& pre, json& post, set<string>& roots)
{
	json& value = s["value"];
	json toType = storeTargetType(s);
	auto settle = [&](bool known) {
		if (!known) roots.clear();
	};
	if (s.value("ownership-transfer", false)) {
		// A variable's own slot can't be reached through a pointer.
		if (value.value("expr-type", "") == "id")
			return false;
		if (!isLocationExpr(value)) {
			if (!hasCall(value))
				return false;
			value = pinValue(sa_expression(value, registry_.fromJson(toType)), value, pre);
			return true;
		}
		// What it gives away moves out now: the pointer is read and the
		// location reset before the target's calls.
		json saSrc = sa_expression(value);
		settle(storageRoots(saSrc, roots, false));
		json src = s;
		value = pinValue(saSrc, value, pre);
		appendTransferSourceReset(pre, src, saSrc);
		value["sa"]["category"] = "expiring";
		return true;
	}
	if (isCopiedByValue(toType)) {
		json saSrc = sa_expression(value);
		if (isExpiringStruct(saSrc)) {
			value = pinValue(move(saSrc), value, pre);
			return true;
		}
		settle(storageRoots(saSrc, roots, true));
		value = snapshotCopy(saSrc, value, pre, post);
		return true;
	}
	json saValue = sa_expression(value, registry_.fromJson(toType));
	if (!hasCall(value)) {
		bool known = storageRoots(saValue, roots, false);
		if (known && roots.empty())
			return false;
		settle(known);
	}
	value = pinValue(move(saValue), value, pre);
	return true;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::storeTargetType(const json& stmt)
{
	if (stmt["stmt-type"] == "arr-assign")
		return sa_expr_arr_index(stmt["target"], /*forWrite=*/true)["value-type"];
	FieldChain chain = resolveObjectChain(stmt["object"], /*forWrite=*/true);
	return fieldValueType(findFieldOrExit(chain.structName, stmt["field"].get<string>(), stmt));
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A copy of what saSrc holds, made into pre and released by post, standing in
// for the value as a `pinned` node; the value itself when its type can't be
// declared, which the store then rejects as a copy.
json PlnSemanticAnalyzer::snapshotCopy(const json& saSrc, const json& locNode, json& pre, json& post)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json& vt = saSrc["value-type"];
	bool isStruct = isStructPntr(vt);
	json declType = isStruct ? json{{"type-kind", "prim"}, {"type-name", vt["base-type"]["type-name"]}}
	              : vt.contains("arr-size") ? astArrType(vt) : json();
	if (declType.is_null())
		return locNode;
	string name = "__snap_" + to_string(tempVarCounter_++);
	json decl = {{"stmt-type", "var-decl"}, {"loc", locNode.value("loc", json::array())},
	             {"vars", json::array({{{"name", name}, {"var-type", declType}}})}};
	for (auto& st : isStruct ? sa_struct_var_decl(decl, VarStorage::Heap) : sa_resolved_var_decl(decl))
		pre.push_back(st);
	post.push_back(takeScopeRelease(name));
	const json type = *findVar(name);
	json dst = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
	pre.push_back(makeCopyStmt(locNode, dst, saSrc));
	json pinned = {{"expr-type", "pinned"}, {"sa", move(dst)}};
	if (locNode.contains("loc")) pinned["loc"] = locNode["loc"];
	return pinned;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The release of a variable of the current scope, which its caller now runs
// instead of the scope's end.
json PlnSemanticAnalyzer::takeScopeRelease(const string& name)
{
	auto& scope = arrayScopeVars_.back();
	auto it = find_if(scope.begin(), scope.end(), [&](const ScopeVar& v){ return v.name == name; });
	json release = move(it->release);
	scope.erase(it);
	return release;
}

const json& PlnSemanticAnalyzer::scopeRelease(const string& name) const
{
	for (auto scope = arrayScopeVars_.rbegin(); scope != arrayScopeVars_.rend(); ++scope)
		for (auto& v : *scope)
			if (v.name == name) return v.release;
	static const json none;
	return none; // LCOV_EXCL_LINE -- callers check isOwnedVar first
}

// The variable whose storage a pointer value points into, if one does.
static string pointeeVar(const json& v)
{
	string et = v.value("expr-type", "");
	if (et == "id" || et == "addr-of")
		return v["name"];
	if (et == "convert")
		return pointeeVar(v["src"]);
	if (et == "field-access")
		return v.contains("var") ? v["var"].get<string>() : pointeeVar(v["ptr-expr"]);
	if (et == "arr-index")
		return pointeeVar(v["array"]);
	return "";
}

void PlnSemanticAnalyzer::noteEscape(const json& saValue)
{
	if (!saValue.contains("value-type") || saValue["value-type"].value("type-kind", "") != "pntr")
		return;
	string name = pointeeVar(saValue);
	if (!name.empty())
		escapedVars_.insert(name);
}

// Collects into roots the local variables whose storage v reads -- with
// contents, also all that v points to, as a copy of it reads. False when v
// reads memory reached through a pointer, which a call may reach too.
bool PlnSemanticAnalyzer::storageRoots(const json& v, set<string>& roots, bool contents)
{
	string et = v.value("expr-type", "");
	auto ownStorage = [&](const string& name) { return isOwnedVar(name) || isFrameVar(name); };
	if (et == "id") {
		string name = v["name"];
		const json* vt = findVar(name);
		if (vt == nullptr || (contents && vt->value("type-kind", "") == "pntr" && !ownStorage(name)))
			return false;
		roots.insert(name);
		return true;
	}
	// The struct a field is read from, or the array an element is: storage
	// reached only through an owner, never through a borrow or raw pointer.
	auto ownedBase = [&](const json& base) {
		return !base["value-type"].contains("mutable") && storageRoots(base, roots, true);
	};
	if (et == "field-access") {
		if (!v.contains("var"))
			return ownedBase(v["ptr-expr"]);
		string name = v["var"];
		if (!ownStorage(name))
			return false;
		roots.insert(name);
		return true;
	}
	if (et == "arr-index")
		return ownedBase(v["array"]) && storageRoots(v["index"], roots, false);
	if (et == "call" || et == "call-ptr" || et == "temp")
		return false;
	for (auto& [k, child] : v.items())
		if (child.is_object() && child.contains("expr-type") && !storageRoots(child, roots, false))
			return false;
	return true;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Replaces each `pending-alias` with the statements for whether one of its
// variables gave a pointer to its storage away.
void PlnSemanticAnalyzer::resolvePending(json& stmts)
{
	if (stmts.is_object()) {
		for (auto& [k, v] : stmts.items())
			resolvePending(v);
		return;
	}
	if (!stmts.is_array())
		return;
	json out = json::array();
	for (auto& st : stmts) {
		if (!st.is_object() || st.value("stmt-type", "") != "pending-alias") {
			resolvePending(st);
			out.push_back(move(st));
			continue;
		}
		bool aliased = false;
		for (auto& name : st["vars"])
			aliased = aliased || escapedVars_.count(name.get<string>());
		json& pick = st[aliased ? "aliased" : "unaliased"];
		resolvePending(pick);
		for (auto& p : pick)
			out.push_back(move(p));
	}
	stmts = move(out);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The calls along a location -- its indices and the expression it starts
// from -- each evaluated once into a temp, leaving the same location.
json PlnSemanticAnalyzer::pinLocation(json loc, json& body)
{
	string et = loc.value("expr-type", "");
	if (et == "arr-index") {
		loc["array"] = pinLocation(move(loc["array"]), body);
		if (hasCall(loc["index"]))
			loc["index"] = pinValue(sa_expression(loc["index"]), loc["index"], body);
	} else if (et == "field-access") {
		loc["object"] = pinLocation(move(loc["object"]), body);
	} else if (et != "id" && hasCall(loc)) {
		loc = pinValue(sa_expression(loc), loc, body);
	}
	return loc;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// saValue evaluated into a temp, standing in for the expression it was
// analyzed from: a `pinned` node, which sa_expression gives back as the temp
// with saValue's type and category, so a store treats it as the value itself.
json PlnSemanticAnalyzer::pinValue(json saValue, const json& astValue, json& body)
{
	// A C function's by-value struct, which only a declaration receives: left
	// where it is, to be diagnosed or evaluated by the store.
	if (saValue["value-type"].value("type-kind", "") == "struct") // LCOV_EXCL_EXCEPTION_BR_LINE
		return astValue;
	if (holdsCallTemps(saValue)) {
		cerr << locPrefix(astValue) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = "__pin_" + to_string(tempVarCounter_++);
	const json type = saValue["value-type"];
	declareVar(name, type);
	json temp = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
	if (saValue.contains("category")) temp["category"] = saValue["category"];
	body.push_back({{"stmt-type", "var-decl"},
	                {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(saValue)}}})}});
	json pinned = {{"expr-type", "pinned"}, {"sa", move(temp)}};
	if (astValue.contains("loc")) pinned["loc"] = astValue["loc"];
	return pinned;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_assign_stmt(const json& stmt, json* litAddr)
{
	json sa_target = sa_expr_arr_index(stmt["target"], /*forWrite=*/true);
	bool transfer = stmt.value("ownership-transfer", false);
	if (!isWritableThrough(sa_target["array"]["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
		exit(1);
	}
	if (isAggregateLit(stmt["value"]) && (isStructPntr(sa_target["value-type"]) || isArrLevel(sa_target["value-type"]))) {
		json lit = sa_aggregate_lit_assign(stmt, stmt["target"], sa_target["value-type"], litAddr);
		return litAddr ? lit : json::array({move(lit)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	if (!transfer && isCopiedByValue(sa_target["value-type"]))
		return json::array({makeCopyStmt(stmt, sa_target, sa_expression(stmt["value"]))}); // LCOV_EXCL_EXCEPTION_BR_LINE
	if (sa_target.value("addr-only", false)) {
		// The element itself is an address computation (e.g. a struct array
		// element), not a storage slot to overwrite -- assign to its fields instead.
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AssignToWholeStructElem) << endl;
		exit(1);
	}
	const json& targetType = sa_target["value-type"];
	// A borrowed slot never owns what it points to, so it cannot be given anything.
	if (transfer && isPtrBorrow(targetType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToBorrowSlot) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(targetType);
	json sa_value = sa_expression(stmt["value"], toType);
	sa_value = convertForBinding(stmt, sa_value, toType, registry_.toJson(toType));
	checkElemShape(stmt, sa_value, targetType);
	checkBorrowSource(stmt, sa_value, targetType);
	checkPtrPermission(stmt, sa_value["value-type"], targetType);

	json arr_assign = {{"stmt-type", "arr-assign"}, {"target", sa_target}, {"value", sa_value}};
	if (!transfer)
		return json::array({arr_assign});

	// The row's owner frees it by the row's declared size, which the new row must have.
	if (isArrLevel(targetType) && !copyShapeMatch(sa_value["value-type"], targetType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferShapeMismatch,
			typeDisplayName(targetType), typeDisplayName(sa_value["value-type"])) << endl;
		exit(1);
	}
	arr_assign["ownership-transfer"] = true;
	return transferStmts(stmt, makeOwnedValueFreeStmt(sa_target), move(arr_assign), sa_value);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The name an array-literal diagnostic gives the target: the variable or
// field that holds it (a row's array).
static string arrLitTargetName(const json& ast)
{
	string et = ast.value("expr-type", "");
	if (et == "field-access")
		return ast["field"].get<string>();
	if (et == "arr-index")
		return arrLitTargetName(ast["array"]);
	return ast["name"].get<string>();
}

// Every value is evaluated into a temp before anything is written, so
// `[v.y, v.x] -> v[0]` swaps, and the target's address is taken once, so an
// index expression in it is evaluated once rather than once per element.
json PlnSemanticAnalyzer::sa_aggregate_lit_assign(const json& stmt, const json& targetAst, const json& targetType,
                                                   json* litAddr)
{
	if (stmt.value("ownership-transfer", false)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner) << endl;
		exit(1);
	}
	bool isStruct = isStructPntr(targetType);
	const StructDef* def = nullptr;
	ArrLitShape shape;
	json values;
	if (isStruct) {
		if (!isStructStorage(targetType)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructLitTarget) << endl;
			exit(1);
		}
		def = &requireCompleteStruct(targetType["base-type"]["type-name"].get<string>(), stmt);
		values = sa_struct_lit(stmt["value"], *def);
	} else {
		string name = arrLitTargetName(targetAst);
		// A row reached through a borrowed array has the same type, but it is
		// the borrowed storage itself, so only the variable is a borrow.
		if (targetAst.value("expr-type", "") == "id" && isArrBorrowVar(targetType)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitTarget, name) << endl;
			exit(1);
		}
		const json& lit = stmt["value"];
		if (lit.value("expr-type", "") != "arr-lit") {
			cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
			exit(1);
		}
		shape = arrLitShape(stmt, name, targetType);
		values = sa_arr_lit_values(lit, shape, name);
		checkArrLitCount(stmt, lit["items"], name, shape.is2d, targetType["arr-size"].get<uint64_t>(),
		                 shape.is2d ? targetType["base-type"]["arr-size"].get<uint64_t>() : 0);
	}

	if (!litAddr)
		enterScope();
	// LCOV_EXCL_EXCEPTION_BR_START
	json body = json::array();
	auto bindTemp = [&](json init) -> string {
		string name = "__lit_" + to_string(tempVarCounter_++);
		const json type = init["value-type"];
		declareVar(name, type);
		body.push_back({{"stmt-type", "var-decl"},
		                {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(init)}}})}});
		return name;
	};
	// A struct value is copied into a temp struct of its own, so a source
	// overlapping the target (`[q.b, q.a] -> q`) is read before any write.
	auto bindStructTemp = [&](json v) -> string {
		if (isExpiringStruct(v)) {
			string name = bindTemp(v);
			addOwnedScopeVar(name, makeStructFreeStmt(name, v["value-type"]));
			return name;
		}
		string name = "__lit_" + to_string(tempVarCounter_++);
		json decl = {{"stmt-type", "var-decl"}, {"loc", stmt.value("loc", json::array())},
			{"vars", json::array({{{"name", name},
				{"var-type", {{"type-kind", "prim"}, {"type-name", v["value-type"]["base-type"]["type-name"]}}}}})}};
		for (auto& s : sa_struct_var_decl(decl, VarStorage::Heap)) body.push_back(s);
		const json type = *findVar(name);
		body.push_back(makeCopyStmt(stmt, {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}, v));
		return name;
	};
	function<void(json&)> bindLeaves = [&](json& vals) { // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		for (auto& v : vals) {
			if (v.is_array()) {
				bindLeaves(v);
				continue;
			}
			string name = isStructPntr(v["value-type"]) ? bindStructTemp(move(v)) : bindTemp(move(v));
			const json type = *findVar(name);
			v = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
		}
	};
	bindLeaves(values);
	json target = {{"expr-type", "id"},
	               {"name", bindTemp(sa_expression({{"expr-type", "addr-of"}, {"mutable", true}, {"object", targetAst}}))}};
	if (isStruct) {
		json obj = {{"expr-type", "arr-index"}, {"array", target},
		            {"index", {{"expr-type", "lit-uint"}, {"value", "0"}}}};
		emitStructLitAssigns(obj, *def, values, body);
	} else {
		emitArrLitAssigns(target, shape, values, body);
	}
	// The caller's scope holds the temps and releases them.
	if (litAddr) {
		const json type = *findVar(target["name"].get<string>());
		*litAddr = {{"expr-type", "pinned"},
		            {"sa", {{"expr-type", "id"}, {"name", target["name"]}, {"var-type", type}, {"value-type", type}}}};
		return body;
	}
	size_t idx = arrayScopeVars_.size() - 1;
	for (auto& s : collectFreeStmts(idx, idx + 1)) body.push_back(s);
	// LCOV_EXCL_EXCEPTION_BR_STOP
	leaveScope();
	return {{"stmt-type", "block"}, {"body", move(body)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::transferStmts(const json& stmt, json targetRelease, json store, const json& value)
{
	json result = json::array({move(targetRelease), move(store)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	appendTransferSourceReset(result, stmt, value);
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Only an owner may give its value away, and it then holds NULL so its own free
// is a no-op: C free and the build-mgr generated __pln_free_* functions all
// accept NULL. A call result has no owner to reset.
void PlnSemanticAnalyzer::appendTransferSourceReset(json& stmts, const json& stmt, const json& saValue)
{
	if (saValue.value("category", "") == "expiring")
		return;
	const json& src = stmt["value"];
	string et = src.value("expr-type", "");
	auto nullOf = [](const json& vt) -> json {
		return {{"expr-type","lit-int"},{"value","0"},{"value-type",vt}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (et == "id" && saValue.value("category", "") == "owned") {
		stmts.push_back({{"stmt-type", "assign"}, {"name", saValue["name"]}, // LCOV_EXCL_EXCEPTION_BR_LINE
		                 {"value", nullOf(saValue["value-type"])}});
		return;
	}
	if (et == "field-access" || et == "arr-index") {
		json loc = et == "field-access" ? sa_expr_field_access(src, /*forWrite=*/true)
		                                : sa_expr_arr_index(src, /*forWrite=*/true);
		if (!loc.value("addr-only", false) && isCopiedByValue(loc["value-type"])) {
			if (et == "arr-index") {
				if (!isWritableThrough(loc["array"]["value-type"])) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
					exit(1);
				}
				stmts.push_back({{"stmt-type", "arr-assign"}, {"target", loc}, // LCOV_EXCL_EXCEPTION_BR_LINE
				                 {"value", nullOf(loc["value-type"])}});
			} else {
				json reset = {{"stmt-type", "field-assign"}, {"offset", loc["offset"]},
				              {"value-type", loc["value-type"]}, {"value", nullOf(loc["value-type"])}}; // LCOV_EXCL_EXCEPTION_BR_LINE
				if (loc.contains("var")) reset["var"] = loc["var"];
				else reset["ptr-expr"] = loc["ptr-expr"];
				stmts.push_back(move(reset));
			}
			return;
		}
	}
	cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner) << endl;
	exit(1);
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
		// Named return values: bare return only
		if (hasValues) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_NamedRetBareReturn) << endl;
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
		value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
		checkElemShape(stmt, value, (*currentFunc_)["ret-type"]);
		checkBorrowSource(stmt, value, (*currentFunc_)["ret-type"]);
		checkPtrPermission(stmt, value["value-type"], (*currentFunc_)["ret-type"]);
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
		{"vars", json::array({{{"name", name}, {"var-type", type}, {"init", value}}})} // LCOV_EXCL_EXCEPTION_BR_LINE
	}; // LCOV_EXCL_EXCEPTION_BR_LINE
	value = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return decl;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// `return [..];` from a function returning a struct becomes
// `T __ret_N = [..]; return __ret_N;`, so the literal goes through the
// struct initializer and the return through the path of a returned variable.
// The declared type is the resolved one, not the normalized ret-type, which is
// the struct's storage pointer.
json PlnSemanticAnalyzer::bindReturnStructLit(json& stmt)
{
	if (!currentFunc_->contains("ret-type") || !isStructStorage((*currentFunc_)["ret-type"])
			|| !stmt.contains("values") || stmt["values"].size() != 1
			|| !isAggregateLit(stmt["values"][0]))
		return json::array();
	string name = "__ret_" + to_string(tempVarCounter_++);
	json& value = stmt["values"][0];
	const json loc = stmt.value("loc", json::array());
	json decl = {{"stmt-type", "var-decl"}, {"loc", loc}, // LCOV_EXCL_EXCEPTION_BR_LINE
		{"vars", json::array({{{"name", name}, {"var-type", (*currentFuncDef_)["ret-type"]}, {"init", value}}})}};
	value = {{"expr-type", "id"}, {"name", name}, {"loc", value.value("loc", loc)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return sa_resolved_var_decl(decl);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A returned struct is freed by the caller, so one the function doesn't own
// (a parameter, a field, an element) is returned as a copy. The copy is made
// after sa_return_stmt so a wrong value is still reported as a return error.
json PlnSemanticAnalyzer::copyReturnStruct(const json& stmt, json& ret)
{
	if (!ret.contains("values") || !isStructStorage((*currentFunc_)["ret-type"]))
		return json::array();
	json& value = ret["values"][0];
	if (!value.contains("value-type") || !isStructPntr(value["value-type"]) || isExpiringStruct(value))
		return json::array();
	string name = "__ret_" + to_string(tempVarCounter_++);
	json decl = {{"stmt-type", "var-decl"}, {"loc", stmt.value("loc", json::array())}, // LCOV_EXCL_EXCEPTION_BR_LINE
		{"vars", json::array({{{"name", name}, {"var-type", (*currentFuncDef_)["ret-type"]}}})}};
	json result = sa_resolved_var_decl(decl);
	removeFromArrayScope(name);
	const json type = *findVar(name);
	json dst = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	result.push_back(makeCopyStmt(stmt, dst, value));
	value = dst;
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A variable whose declared type differs from its return value's receives it
// through a temp and is declared as `T x = temp;`, so it follows the same
// conversion rules as a single-value declaration.
json PlnSemanticAnalyzer::sa_tapple_decl(const json& stmt)
{
	json callExpr = tappleCallExpr(stmt);
	const json& rets = findMultiRetFunc(stmt, callExpr, stmt["vars"].size())["rets"];
	callExpr["multi-ret"] = rets.size();

	// Process the call expression via sa_expression (resolves func-type, annotates args)
	json saCall = sa_expr_maybe_void(callExpr);

	// Add multi-return value-types from the function's rets
	json valueTypes = json::array();
	for (auto& r : rets)
		valueTypes.push_back(r["var-type"]);
	saCall["value-types"] = valueTypes;
	if (holdsCallTemps(saCall)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}

	json vars = json::array();
	json convDecls = json::array();
	vector<VarStorage> convStorage;
	for (size_t i = 0; i < stmt["vars"].size(); i++) {
		const json& v = stmt["vars"][i];
		const json& retType = rets[i]["var-type"];
		json varType = v["var-type"];
		VarStorage storage = takeVarStorage(varType);
		varType = resolveKnownType(stmt, varType);
		string name = v["var-name"];
		// A frame variable can't take the returned block over, so it is
		// declared from the temp like a single `$T x = f();`, which moves a
		// returned struct's contents into the frame and frees the block.
		bool movesIntoFrame = storage == VarStorage::Frame && isStructStorage(retType);
		if (storage == VarStorage::Frame || normalizeSigType(varType) != retType) {
			string temp = "__tap_" + to_string(tempVarCounter_++);
			json init = {{"expr-type", "id"}, {"name", temp}, {"loc", stmt["loc"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			if (movesIntoFrame)
				init = {{"expr-type", "pinned"}, {"loc", stmt["loc"]}, {"sa", {{"expr-type", "id"}, {"name", temp}, // LCOV_EXCL_EXCEPTION_BR_LINE
					{"var-type", retType}, {"value-type", retType}, {"category", "expiring"}}}};
			convDecls.push_back({{"stmt-type", "var-decl"}, {"loc", stmt["loc"]}, // LCOV_EXCL_EXCEPTION_BR_LINE
				{"vars", json::array({{{"name", name}, {"var-type", varType}, {"init", move(init)}}})}}); // LCOV_EXCL_EXCEPTION_BR_LINE
			convStorage.push_back(storage);
			name = temp;
		}
		declareVar(name, retType);
		// A returned struct is owned by the caller.
		if (isStructStorage(retType) && !movesIntoFrame)
			addOwnedScopeVar(name, makeStructFreeStmt(name, retType));
		vars.push_back({{"var-name", name}, {"var-type", retType}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	json result = json::array({{{"stmt-type", "tapple-decl"}, {"vars", vars}, {"value", saCall}}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	for (size_t i = 0; i < convDecls.size(); i++)
		for (auto& s : sa_resolved_var_decl(convDecls[i], {convStorage[i]}))
			result.push_back(s);
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// All return values are received into temps first, then assigned left to
// right through the single-target assignment paths, so `f() -> (i, arr[i])`
// indexes with the new i. The block limits the temps to this statement and
// releases a returned struct once it has been copied into its target.
json PlnSemanticAnalyzer::sa_tapple_assign(const json& stmt)
{
	const json& targets = stmt["targets"];
	json callExpr = tappleCallExpr(stmt);
	const json& rets = findMultiRetFunc(stmt, callExpr, targets.size())["rets"];

	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", callExpr},
	             {"loc", stmt["loc"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	vector<string> temps;
	for (auto& r : rets) {
		temps.push_back("__tap_" + to_string(tempVarCounter_++));
		decl["vars"].push_back({{"var-name", temps.back()}, {"var-type", r["var-type"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	json body = json::array({decl}); // LCOV_EXCL_EXCEPTION_BR_LINE

	for (size_t i = 0; i < targets.size(); i++) {
		const json& t = targets[i];
		json value = {{"expr-type", "id"}, {"name", temps[i]}, {"loc", t["loc"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		string et = t["expr-type"];
		if (et == "id")
			body.push_back({{"stmt-type", "assign"}, {"name", t["name"]}, {"value", value}, {"loc", t["loc"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
		else if (et == "arr-index")
			body.push_back({{"stmt-type", "arr-assign"}, {"target", t}, {"value", value}, {"loc", t["loc"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
		else
			body.push_back({{"stmt-type", "field-assign"}, {"object", t["object"]}, // LCOV_EXCL_EXCEPTION_BR_LINE
			                {"field", t["field"]}, {"value", value}, {"loc", t["loc"]}});
	}
	return sa_block({{"stmt-type", "block"}, {"body", body}}); // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_field_assign(const json& stmt, json* litAddr)
{
	FieldChain chain = resolveObjectChain(stmt["object"], /*forWrite=*/true);
	string fn = stmt["field"].get<string>();
	const FieldLayout* it = &findFieldOrExit(chain.structName, fn, stmt);
	bool transfer = stmt.value("ownership-transfer", false);
	if (transfer && it->typeKind != "struct-ptr" && it->typeKind != "arr-ptr") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToNonOwnedField, fn) << endl;
		exit(1);
	}
	json fieldType = fieldValueType(*it);
	if (isAggregateLit(stmt["value"]) && (isStructPntr(fieldType) || isArrLevel(fieldType))) {
		json fieldAst = {{"expr-type", "field-access"}, {"object", stmt["object"]}, {"field", fn}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		json lit = sa_aggregate_lit_assign(stmt, fieldAst, fieldType, litAddr);
		return litAddr ? lit : json::array({move(lit)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	const PlnType* toType = registry_.fromJson(fieldType);
	json value = sa_expression(stmt["value"], toType);
	if (!transfer && isCopiedByValue(fieldType))
		return json::array({makeCopyStmt(stmt, makeFieldAccess(chain, *it), value)}); // LCOV_EXCL_EXCEPTION_BR_LINE
	value = convertForBinding(stmt, value, toType, fieldType);
	checkBorrowSource(stmt, value, fieldType);
	checkPtrPermission(stmt, value["value-type"], fieldType);
	if (!transfer)
		return json::array({makeFieldAssign(chain, *it, move(value))}); // LCOV_EXCL_EXCEPTION_BR_LINE
	// The field owns what it held, so '->>' releases that before taking over.
	return transferStmts(stmt, makeOwnedValueFreeStmt(makeFieldAccess(chain, *it)),
	                     makeFieldAssign(chain, *it, value), value);
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
			typeDisplayName(t), typeDisplayName(st)) << endl;
		exit(1);
	}

	// A struct returned by a Palan call is owned by nobody yet: receive it in a
	// temp, copy from it like from a named value, and free it.
	if (isExpiringStruct(src)) {
		// LCOV_EXCL_EXCEPTION_BR_START
		string temp = "__cpy_" + to_string(tempVarCounter_++);
		json decl = {{"stmt-type","var-decl"},
		             {"vars",json::array({{{"name",temp},{"var-type",st},{"init",src}}})}};
		json tempId = {{"expr-type","id"},{"name",temp},{"var-type",st},{"value-type",st}};
		json body = json::array({decl, makeCopyStmt(locNode, dst, tempId), makeStructFreeStmt(temp, st)});
		return {{"stmt-type","block"},{"body",move(body)}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	auto lit = [&](int64_t v) -> json {
		return {{"expr-type","lit-uint"},{"value",to_string(v)},{"value-type",uint64_type}};
	};
	auto copyStmt = [&](AllocFamily family, const string& leaf, json args) -> json {
		return {{"stmt-type","expr"},{"body",allocHelperCall(family, leaf, "copy", move(args))}};
	};
	auto memcpyStmt = [&](int64_t bytes) {
		return json{{"stmt-type","expr"},{"body",{{"expr-type","call"},{"name","memcpy"},{"func-type","c"},
			{"args",json::array({dst, src, lit(bytes)})}}}};
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
		return copyStmt(AllocFamily::Struct, name, json::array({dst, src})); // LCOV_EXCL_EXCEPTION_BR_LINE
	}

	int64_t n = t["arr-size"];
	const json& elem = t["base-type"];
	if (t.value("embedded", false)) {
		return memcpyStmt(n * t["stride"].get<int64_t>());
	}
	if (isStructStorage(elem)) {
		string name = elem["base-type"]["type-name"];
		recordArrStructShape(name);
		return copyStmt(AllocFamily::ArrStruct, name, json::array({dst, src, lit(n)})); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	if (isArrLevel(elem)) {
		int64_t m = elem["arr-size"];
		const json& leaf = elem["base-type"];
		if (elem.value("embedded", false) && elem.contains("stride")) {
			// [n][m]$T: rows are byte blocks, allocated as arr_arr_uint8.
			recordArrArrShape("uint8");
			return copyStmt(AllocFamily::ArrArrPrim, "uint8",
				json::array({dst, src, lit(n), lit(m * elem["stride"].get<int64_t>())})); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		if (isStructStorage(leaf)) {
			string name = leaf["base-type"]["type-name"];
			recordArrArrStructShape(name);
			return copyStmt(AllocFamily::ArrArrStruct, name, json::array({dst, src, lit(n), lit(m)})); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		if (leaf.value("type-kind","") == "prim" && !elem.value("embedded", false)) {
			string leafName = leaf["type-name"];
			recordArrArrShape(leafName);
			return copyStmt(AllocFamily::ArrArrPrim, leafName, json::array({dst, src, lit(n), lit(m)})); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyUnsupportedShape, typeDisplayName(t)) << endl;
		exit(1);
	}
	// Primitive elements, or '@T' slots whose pointers are copied as they are.
	int64_t elemBytes = isPtrBorrow(elem) ? 8 : elemSizeBytes(elem["type-name"]);
	return memcpyStmt(n * elemBytes);
} // LCOV_EXCL_EXCEPTION_BR_LINE
