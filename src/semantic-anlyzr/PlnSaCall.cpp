/// Palan Semantic Analyzer — calls and callee lookup
///
/// @file PlnSaCall.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

// LCOV_EXCL_EXCEPTION_BR_START
static json makeTempExpr(json body, json value, json release)
{
	json out = {{"expr-type","temp"},{"body",move(body)},{"release",move(release)}};
	if (value.contains("value-type")) out["value-type"] = value["value-type"];
	if (value.contains("category")) out["category"] = value["category"];
	out["value"] = move(value);
	return out;
}
// LCOV_EXCL_EXCEPTION_BR_STOP

const json* PlnSemanticAnalyzer::findCFunc(const string& name) const
{
	for (auto it = cFuncScopes.rbegin(); it != cFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

const json* PlnSemanticAnalyzer::findPlnFunc(const string& name) const
{
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return &f->second;
	}
	return nullptr;
}

// The functions `alias.name` names in the innermost scope that imports one;
// "" is the unqualified imports.
vector<Callee> PlnSemanticAnalyzer::findCalleesByAlias(const string& alias, const string& name) const
{
	vector<Callee> cands;
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find(alias);
		if (alias_it == it->end()) continue;
		auto f_it = alias_it->second.find(name);
		if (f_it == alias_it->second.end()) continue;
		for (auto& def : f_it->second)
			cands.push_back({&def, def.value("_c-func", false)});
		break;
	}
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The functions a call by `name` can mean. A C function hides a Palan one,
// and either hides an import.
vector<Callee> PlnSemanticAnalyzer::findCallees(const string& name) const
{
	if (const json* c = findCFunc(name)) return {{c, true}};
	if (const json* p = findPlnFunc(name)) return {{p, false}};
	return findCalleesByAlias("", name);
}

void PlnSemanticAnalyzer::requireAlias(const json& locNode, const string& alias) const
{
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		if (it->count(alias)) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownAlias, alias) << endl;
	exit(1);
}

Callee PlnSemanticAnalyzer::requireUniqueCallee(const json& locNode, const string& name, const vector<Callee>& cands) const
{
	BOOST_ASSERT(!cands.empty());
	if (cands.size() > 1) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_AmbiguousCall, name) << endl;
		exit(1);
	}
	return cands[0];
}

// A struct returned by a call and given as an argument is held in a temp,
// since the callee may return a borrow of it.
json PlnSemanticAnalyzer::bindCallTemp(json saArg)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = "__tmp_" + to_string(tempVarCounter_++);
	json storage = saArg["value-type"];
	storage.erase("mutable");
	json valueType = saArg["value-type"];
	json decl = {{"stmt-type","var-decl"},
	             {"vars",json::array({{{"name",name},{"var-type",storage},{"init",move(saArg)}}})}};
	json value = {{"expr-type","id"},{"name",name},{"var-type",storage},{"value-type",move(valueType)}};
	return makeTempExpr(json::array({move(decl)}), move(value),
	                    json::array({makeStructFreeStmt(name, storage)}));
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// '@f()'/'@!f()' as a call argument: the returned struct is borrowed for the
// call, held in a temp (bindCallTemp).
json PlnSemanticAnalyzer::sa_call_borrow_arg(const json& arg)
{
	json saArg = sa_expression(arg["object"]);
	if (!isExpiringStruct(saArg)) {
		cerr << locPrefix(arg) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
		exit(1);
	}
	saArg["value-type"]["mutable"] = arg["mutable"];
	return saArg;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A literal argument ('[..]', '{..}', or one under '@'/'@!') becomes a temp of
// the parameter's struct or array type, initialized by body; the argument is
// then analyzed as the temp written in its place ('@__lit_N'), returned with
// argAst set to that form. A null paramVT is a C function's parameter.
json PlnSemanticAnalyzer::sa_lit_arg(const json& arg, const json* paramVT, const string& paramName,
                                     const string& funcName, json& argAst, json& body, json& release)
{
	bool isAddrOf = arg.value("expr-type", "") == "addr-of";
	const json& lit = isAddrOf ? arg["object"] : arg;
	auto rejectParamType = [&](PlnSaMessageCode code) __attribute__((noreturn)) {
		cerr << locPrefix(lit) << PlnSaMessage::getMessage(code, paramName, typeDisplayName(*paramVT)) << endl;
		exit(1);
	};
	auto rejectContext = [&]() {
		if (!paramVT) {
			cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_LitArgCFunc, funcName) << endl;
			exit(1);
		}
		rejectParamType(lit["expr-type"] == "arr-lit" ? E_ArrLitArgType : E_DictLitArgType);
	};
	if (!paramVT)
		rejectContext();
	bool isStruct = isStructPntr(*paramVT);
	json declType = isStruct
		? json{{"type-kind","prim"},{"type-name",(*paramVT)["base-type"]["type-name"]}} // LCOV_EXCL_EXCEPTION_BR_LINE
		: paramVT->contains("arr-size") ? astArrType(*paramVT) : json();
	if (declType.is_null()) {
		if (!paramVT->contains("arr-size"))
			rejectContext();
		cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_ArrLitElemType, paramName) << endl;
		exit(1);
	}

	enterScope();
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = "__lit_" + to_string(tempVarCounter_++);
	json decl = {{"stmt-type","var-decl"},{"loc",lit.value("loc", json::array())},
		{"vars",json::array({{{"name",name},{"var-type",declType}}})}};
	body = sa_resolved_var_decl(decl);
	json target = {{"expr-type","id"},{"name",name}};
	const json varType = *findVar(name);
	if (isStruct) {
		const StructDef& def = requireCompleteStruct(varType["base-type"]["type-name"].get<string>(), lit);
		emitStructLitAssigns(target, def, sa_struct_lit(lit, def), body);
	} else {
		if (lit["expr-type"] != "arr-lit")
			rejectParamType(E_DictLitArgType);
		ArrLitShape shape = arrLitShape(lit, paramName, varType);
		json values = sa_arr_lit_values(lit, shape, paramName);
		checkArrLitCount(lit, lit["items"], paramName, shape.is2d, varType["arr-size"].get<uint64_t>(),
		                 shape.is2d ? varType["base-type"]["arr-size"].get<uint64_t>() : 0);
		emitArrLitAssigns(target, shape, values, body);
	}
	argAst = isAddrOf ? json{{"expr-type","addr-of"},{"object",target},{"mutable",arg["mutable"]},{"loc",arg.value("loc", json::array())}}
	                  : target;
	json saArg = sa_expression(argAst);
	size_t idx = arrayScopeVars_.size() - 1;
	release = collectFreeStmts(idx, idx + 1);
	// LCOV_EXCL_EXCEPTION_BR_STOP
	leaveScope();
	return saArg;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// An upgrade's diagnostic differs by callee kind, since a C parameter's
// permission comes from const-qualification while a Palan parameter's comes
// from `@!T`.
void PlnSemanticAnalyzer::checkArgPtrPermission(const json& expr, const string& funcName,
		bool isCFunc, const json& saArg, const json& param, size_t argIdx)
{
	if (isCFunc && ptrPermission(saArg["value-type"], param["var-type"]) == PtrPermission::Upgrade) {
		string paramName = param.value("name", "");
		if (paramName.empty()) paramName = "#" + to_string(argIdx + 1);
		cerr << locPrefix(expr)
		     << PlnSaMessage::getMessage(E_ReadOnlyPtrToNonConstCParam, funcName, paramName) << endl;
		exit(1);
	}
	checkPtrPermission(expr, saArg["value-type"], param["var-type"]);
}

// The call node for `expr` calling `callee`, as written by `writtenName`.
json PlnSemanticAnalyzer::makeCallNode(const json& expr, const Callee& callee, const string& writtenName)
{
	const json& def = *callee.def;
	json sa_expr = expr;
	sa_expr["expr-type"] = "call";
	sa_expr.erase("object");
	sa_expr.erase("method");
	if (callee.isC) {
		requireSupportedCFuncSig(def, writtenName, expr);
		sa_expr["func-type"] = "c";
		sa_expr["name"] = cLinkName(def);
		if (def.contains("ret-type") && def["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = def["ret-type"];
	} else {
		sa_expr["func-type"] = def["func-type"];
		sa_expr["name"] = writtenName;
		if (def.contains("ret-type"))
			sa_expr["value-type"] = def["ret-type"];
		if (def.value("func-type", "") == "syscall")
			sa_expr["syscall-number"] = def["syscall-number"];
	}
	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";
	sa_expr["args"] = saCallArgs(expr, expr["args"], def["parameters"], callee.isC, writtenName);
	return sa_expr;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A function named as a value: its address, typed as the `@func` it can be
// called through. Null if `expr` names no function.
json PlnSemanticAnalyzer::sa_func_ref(const json& expr)
{
	const string& name = expr["name"].get<string>();
	auto reject = [&](const string& reason) __attribute__((noreturn)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNotValue, name, reason) << endl;
		exit(1);
	};
	json func = {{"type-kind","func"},{"parameters",json::array()}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	vector<Callee> cands = findCallees(name);
	if (cands.empty()) return json();
	Callee callee = requireUniqueCallee(expr, name, cands);
	const json* pFunc = callee.def;
	if (callee.isC) {
		requireSupportedCFuncSig(*pFunc, name, expr);
		func["ret-type"] = (*pFunc)["ret-type"];
	} else {
		if (pFunc->value("func-type", "") == "syscall") reject("it is a syscall");
		if (pFunc->contains("rets") && (*pFunc)["rets"].size() > 1) reject("it returns multiple values");
		// LCOV_EXCL_EXCEPTION_BR_START
		func["ret-type"] = pFunc->contains("ret-type") ? (*pFunc)["ret-type"]
		                 : pFunc->contains("rets") ? (*pFunc)["rets"][0]["var-type"]
		                 : json{{"type-kind","prim"},{"type-name","void"}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	for (auto& p : (*pFunc)["parameters"]) {
		if (p.value("name", "") == "...") reject("it takes variable arguments");
		func["parameters"].push_back({{"var-type", p["var-type"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	const json& ret = func["ret-type"];
	bool retOk = ret.value("type-name", "") == "void" || isFuncSigType(ret);
	for (auto& p : func["parameters"])
		if (!isFuncSigType(p["var-type"]))
			reject("its parameter type '" + typeDisplayName(p["var-type"]) + "' cannot be passed through one");
	if (!retOk)
		reject("its return type '" + typeDisplayName(ret) + "' cannot be returned through one");
	// LCOV_EXCL_EXCEPTION_BR_START
	json out = {{"expr-type", "func-ref"}, {"name", callee.isC ? cLinkName(*pFunc) : json(name)},
	            {"value-type", {{"type-kind","pntr"},{"mutable",false},{"base-type",move(func)}}}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
	if (expr.contains("loc")) out["loc"] = expr["loc"];
	return out;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The function pointer variable a call names, or null. A function of the same
// name makes the call ambiguous.
const json* PlnSemanticAnalyzer::findFuncPtrVar(const json& call) const
{
	const string& name = call["name"].get<string>();
	const json* vt = findVar(name);
	if (!vt) {
		const json* g = findCGlobal(name);
		if (g) vt = &(*g)["var-type"];
	}
	if (!vt || !isFuncPtr(*vt)) return nullptr;
	if (!findCallees(name).empty()) {
		cerr << locPrefix(call) << PlnSaMessage::getMessage(E_AmbiguousFuncPtrCall, name) << endl;
		exit(1);
	}
	return vt;
}

// `s.f(args)` calls the function pointer field `f` of `s`, if it has one: the
// field access, or null. A function `f` the call could also be the method
// form of makes it ambiguous.
json PlnSemanticAnalyzer::funcPtrField(const json& memberCall)
{
	if (!isMethodForm(memberCall)) return json();
	const json& obj = memberCall["object"];
	const string& method = memberCall["method"].get<string>();
	json vt = operandValueType(obj);
	if (!isStructPntr(vt)) return json();
	auto& fields = structDefs_.at(vt["base-type"]["type-name"].get<string>()).fields;
	auto f = find_if(fields.begin(), fields.end(), [&](const FieldLayout& f) { return f.name == method; });
	if (f == fields.end() || !f->unsupported.empty() || !isFuncPtr(fieldValueType(*f))) return json();
	bool isCFunc;
	const json* pFunc = findCalleeByName(method, isCFunc);
	if (pFunc && !(*pFunc)["parameters"].empty() && (*pFunc)["parameters"][0]["var-type"].value("mutable", false)) {
		cerr << locPrefix(memberCall) << PlnSaMessage::getMessage(E_AmbiguousFuncPtrCall, method) << endl;
		exit(1);
	}
	json access = {{"expr-type", "field-access"}, {"object", obj}, {"field", method}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (memberCall.contains("loc")) access["loc"] = memberCall["loc"];
	return access;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A call through a function pointer. Its arguments follow a Palan function's
// rules whatever the pointer came from: the pointer's type is all that is
// known of the callee.
json PlnSemanticAnalyzer::sa_expr_call_ptr(const json& expr, json callee, const string& funcName)
{
	const json& func = callee["value-type"]["base-type"];
	json sa_expr = {{"expr-type", "call-ptr"}, {"callee", callee}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (expr.contains("loc")) sa_expr["loc"] = expr["loc"];
	if (func["ret-type"].value("type-name", "") != "void")
		sa_expr["value-type"] = func["ret-type"];
	sa_expr["args"] = saCallArgs(expr, expr["args"], func["parameters"], false, funcName);
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_call(const json& expr)
{
	const string& callName = expr["name"].get<string>();
	if (findFuncPtrVar(expr)) {
		json id = {{"expr-type", "id"}, {"name", expr["name"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if (expr.contains("loc")) id["loc"] = expr["loc"];
		return sa_expr_call_ptr(expr, sa_expression(id), callName);
	}

	vector<Callee> cands = findCallees(callName);
	if (cands.empty()) {
		for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
			for (auto& [als, bucket] : *it)
				if (als != "" && bucket.count(callName)) {
					cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnqualifiedAliasCall, callName) << endl;
					exit(1);
				}
		undefinedNameError(expr, expr, E_UndefinedFunction);
	}
	return makeCallNode(expr, requireUniqueCallee(expr, callName, cands), callName);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Shared by sa_expr_call and sa_expr_member_call: these used to duplicate
// only part of this loop, silently dropping variadic promotion for an
// aliased call like `S.printf("%f\n", someFlo32)`.
json PlnSemanticAnalyzer::saCallArgs(const json& locNode, const json& args, const json& funcParams,
                                      bool isCFunc, const string& funcName)
{
	bool isVariadic = false;
	for (auto& p : funcParams)
		if (p.value("name", "") == "...") { isVariadic = true; break; }

	size_t fixedCount = funcParams.size() - (isVariadic ? 1 : 0);
	// C's `()` arrives as an empty list just like `(void)` and is checked as
	// `(void)` (as C23 reads it); glibc headers are fully prototyped.
	if (isVariadic ? args.size() < fixedCount : args.size() != fixedCount) {
		string expected = (isVariadic ? "at least " : "") + to_string(fixedCount);
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArgCountMismatch,
			funcName, expected, to_string(args.size())) << endl;
		exit(1);
	} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here

	json saArgs = json::array();
	size_t argIdx = 0;
	for (auto& arg : args) {
		const json* paramVT = argIdx < fixedCount ? &funcParams[argIdx]["var-type"] : nullptr;
		bool byValStruct = isCFunc && paramVT && isStructStorage(*paramVT);
		json argAst = arg;
		json litBody, litRelease;
		bool isLit = isAggregateLit(arg)
			|| (arg.value("expr-type", "") == "addr-of" && isAggregateLit(arg["object"]));
		// A bare integer literal argument adopts the parameter's type here
		// instead of defaulting to int64/uint64, avoiding a spurious narrowing
		// rejection below.
		json saArg = isLit ? sa_lit_arg(arg, isCFunc && !byValStruct ? nullptr : paramVT,
		                                paramVT ? funcParams[argIdx].value("name", "") : "", funcName,
		                                argAst, litBody, litRelease)
		           : isCallBorrow(arg) ? sa_call_borrow_arg(arg)
		           : sa_expression(arg, paramVT ? registry_.fromJson(*paramVT) : nullptr);
		bool expiring = isCallBorrow(arg) || isExpiringStruct(saArg);
		const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
		if (paramVT) {
			if (paramVT->contains("arr-size")) {
				checkArrBorrowBinding(locNode, argAst, saArg, *paramVT);
			} else if (paramVT->value("embedded", false)) {
				// The type registry can't tell contiguous elements from pointer slots,
				// nor a contiguous struct array from a single struct.
				const json& argVT = saArg["value-type"];
				bool argContig = argVT.value("embedded", false) && argVT.contains("stride");
				const json& paramElem = (*paramVT)["base-type"];
				if (isArrLevel(paramElem)) {
					const json* argRow = argContig ? &argVT["base-type"] : nullptr;
					bool argSized = argRow && isArrLevel(*argRow) && argRow->contains("arr-size");
					if (!argSized || (*argRow)["arr-size"] != paramElem["arr-size"]) {
						string actual = argSized
							? to_string((*argRow)["arr-size"].get<int64_t>()) : "variable";
						cerr << locPrefix(locNode)
						     << PlnSaMessage::getMessage(E_EmbeddedArrInnerSizeMismatch,
						            to_string(paramElem["arr-size"].get<int64_t>()), actual) << endl;
						exit(1);
					}
				} else if (!argContig) {
					cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompatibleTypes,
						typeDisplayName(argVT), typeDisplayName(*paramVT)) << endl;
					exit(1);
				}
			}
			checkElemShape(locNode, saArg, *paramVT);
			saArg = convertCallArg(locNode, saArg, *paramVT);
			if (isStructBorrow(*paramVT) && isStructStorage(saArg["value-type"])) {
				cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StructBorrowNeedsAddrOf) << endl;
				exit(1);
			}
			// A by-value struct is only read, to copy it, so a read-only source is fine.
			if (!byValStruct)
				checkArgPtrPermission(locNode, funcName, isCFunc, saArg, funcParams[argIdx], argIdx);
		} else if (isVariadic) {
			const PlnType* promoted = variadicPromote(fromType, registry_);
			if (promoted != fromType)
				saArg = wrapConvert(saArg, registry_.toJson(promoted));
		}
		if (isLit)
			saArg = makeTempExpr(move(litBody), move(saArg), move(litRelease));
		else if (expiring)
			saArg = bindCallTemp(move(saArg));
		if (byValStruct) {
			const StructDef& def = requireCompleteStruct((*paramVT)["base-type"]["type-name"].get<string>(), locNode);
			saArg["struct-arg"] = {{"size", def.totalSize}, {"eightbytes", eightbytesJson(def, structDefs_)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		} else {
			noteEscape(saArg);
		}
		saArgs.push_back(saArg);
		argIdx++;
	}
	return saArgs;
}

// Convert a single call argument to a parameter's type. Embedded-array shape
// and pointer-permission checks are each caller's own responsibility, since
// those differ by callee kind (C vs. Palan).
json PlnSemanticAnalyzer::convertCallArg(const json& locNode, json saArg, const json& paramVT)
{
	const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
	const PlnType* toType   = registry_.fromJson(paramVT);
	if (fromType == toType) return saArg;
	auto isScalar = [](const PlnType* t) {
		return t->kind == PlnType::Kind::Prim || t->kind == PlnType::Kind::Enum;
	};
	if (isScalar(fromType) && isScalar(toType)) {
		if (argConvOk(fromType, toType))
			return wrapConvert(saArg, registry_.toJson(toType));
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_InvalidNarrowingConv,
			typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)) << endl;
		exit(1);
	}
	// pntr(void) is bidirectionally Identical with any pntr(T), which is how NULL passes.
	if (typeCompat(fromType, toType, registry_) == TypeCompat::Incompatible) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompatibleTypes,
			typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)) << endl;
		exit(1);
	}
	return saArg;
}

// A member-call whose object is a variable (or C global) is a method-form
// call; otherwise it is a module-alias call `L.f(args)`. A variable takes
// precedence over an alias of the same name, as it does over an enum name.
bool PlnSemanticAnalyzer::isMethodForm(const json& memberCall) const
{
	const json& obj = memberCall["object"];
	if (obj.value("expr-type", "") != "id") return true;
	const string& name = obj["name"].get<string>();
	return findVar(name) || findCGlobal(name);
}

// The function a call names, looked up as sa_expr_call does; null when it is
// unknown or ambiguous, which sa_expr_call reports.
const json* PlnSemanticAnalyzer::findCalleeByName(const string& name, bool& isCFunc) const
{
	vector<Callee> cands = findCallees(name);
	if (cands.size() != 1) return nullptr;
	isCFunc = cands[0].isC;
	return cands[0].def;
}

// The function a call, a module-alias call, or a chained call (its last call)
// names; null otherwise.
const json* PlnSemanticAnalyzer::findCallee(const json& call, bool& isCFunc) const
{
	isCFunc = false;
	string et = call.value("expr-type", "");
	if (et == "chain-call")
		return findCallee(call["call"], isCFunc);
	if (et == "call")
		return findCalleeByName(call["name"].get<string>(), isCFunc);
	if (et != "member-call" || isMethodForm(call))
		return nullptr;
	vector<Callee> cands = findCalleesByAlias(call["object"]["name"].get<string>(), call["method"].get<string>());
	if (cands.size() != 1) return nullptr;
	isCFunc = cands[0].isC;
	return cands[0].def;
}

// The value type of a method-form receiver or a chained value; null when it
// has none (a void call). An inline struct field is storage and cannot be
// evaluated as a value, so its type comes from the layout.
json PlnSemanticAnalyzer::operandValueType(const json& obj)
{
	if (obj.value("expr-type", "") == "field-access") {
		FieldChain chain = resolveObjectChain(obj["object"], /*forWrite=*/false);
		return fieldValueType(findFieldOrExit(chain.structName, obj["field"].get<string>(), obj));
	}
	return sa_expression(obj)["value-type"];
}

// Rewrites a method-form call `aa.f(args)` to the call `f(@!aa, args)`. Any
// other expression, including a module-alias call `L.f(args)`, is returned as
// is.
json PlnSemanticAnalyzer::normalizeMethodCall(const json& expr)
{
	if (expr["expr-type"] != "member-call" || !isMethodForm(expr)) return expr;
	const json& obj = expr["object"];

	const string& method = expr["method"].get<string>();
	bool isCFunc;
	const json* pFunc = findCalleeByName(method, isCFunc);
	if (pFunc) {
		const json& params = (*pFunc)["parameters"];
		if (params.empty() || !params[0]["var-type"].value("mutable", false)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_MethodNeedsMutableFirstParam, method) << endl;
			exit(1);
		}
	}

	// A receiver that is already a borrow is passed as is: '@!' on it would
	// be rejected (or would mean a pointer to the borrow's slot).
	json recvType = operandValueType(obj);
	bool isBorrow = isPtrBorrow(recvType) || isArrBorrowVar(recvType);
	json recv = isBorrow ? obj : json{{"expr-type", "addr-of"}, {"mutable", true}, {"object", obj}};
	if (!isBorrow && obj.contains("loc")) recv["loc"] = obj["loc"];

	json call = {{"expr-type", "call"}, {"name", method}, {"args", json::array({recv})}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	for (auto& a : expr["args"]) call["args"].push_back(a);
	if (expr.contains("loc")) call["loc"] = expr["loc"];
	return call;
} // LCOV_EXCL_EXCEPTION_BR_LINE

static const json* fixedParamType(const json* callee, size_t idx)
{
	if (!callee || idx >= (*callee)["parameters"].size()) return nullptr;
	const json& param = (*callee)["parameters"][idx];
	if (param.value("name", "") == "...") return nullptr;
	return &param["var-type"];
}

// A chained value given for a parameter: '@' may be omitted for a read-only
// borrow, so it is added here, while '@!' stays explicit so a write is
// visible at the call. A C function takes an array as it is.
json PlnSemanticAnalyzer::chainArg(const json& value, const json* paramVT, bool isCFunc)
{
	if (!paramVT || value.value("expr-type", "") == "addr-of") return value;
	if (paramVT->value("type-kind", "") != "pntr" || paramVT->value("mutable", true)) return value;
	// A literal has no type of its own until it is given a target, so it is
	// borrowed as '@[..]'/'@{..}' and checked against the parameter.
	string et = value.value("expr-type", "");
	if (et != "arr-lit" && et != "dict-lit") {
		json vt = operandValueType(value);
		bool ownedArr = isArrLevel(vt) && !isArrBorrowVar(vt);
		if (!isStructStorage(vt) && vt.value("type-kind", "") != "prim"
				&& !(ownedArr && !isCFunc && isArrBorrowVar(*paramVT)))
			return value;
	}
	json out = {{"expr-type", "addr-of"}, {"mutable", false}, {"object", value}};
	if (value.contains("loc")) out["loc"] = value["loc"];
	return out;
} // LCOV_EXCL_EXCEPTION_BR_LINE

size_t PlnSemanticAnalyzer::chainRetCount(const json& value) const
{
	bool isCFunc;
	const json* callee = findCallee(value, isCFunc);
	return callee && callee->contains("rets") ? (*callee)["rets"].size() : 0;
}

// Rewrites a chained call `v -> f(args)` to `f(v, args)`. A value that is a
// call returning several values is left as a chain-call, received into temps
// by sa_expr_chain_call.
json PlnSemanticAnalyzer::normalizeChainCall(const json& expr)
{
	json value = expr["value"];
	if (value.value("expr-type", "") == "chain-call")
		value = normalizeChainCall(value);
	json call = expr["call"];
	if (call["expr-type"] == "member-call" && isMethodForm(call)) {
		cerr << locPrefix(call) << PlnSaMessage::getMessage(E_ChainToMethodCall) << endl;
		exit(1);
	}
	if (chainRetCount(value) >= 2) {
		json out = expr;
		out["value"] = move(value);
		return out;
	}
	bool isCFunc;
	const json* callee = findCallee(call, isCFunc);
	json arg = chainArg(value, fixedParamType(callee, 0), isCFunc);
	call["args"].insert(call["args"].begin(), move(arg));
	return call;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Receives the return values of a chained call's value into temps, appending
// their declarations to body, and returns the call with the temps in front of
// its arguments.
json PlnSemanticAnalyzer::chainMultiCall(const json& chain, json& body)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json value = chain["value"];
	if (value["expr-type"] == "chain-call")
		value = chainMultiCall(value, body);
	bool isCFunc;
	const json& rets = (*findCallee(value, isCFunc))["rets"];
	json loc = chain.value("loc", json::array());
	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", value}, {"loc", loc}};
	vector<string> temps;
	for (auto& r : rets) {
		temps.push_back("__tap_" + to_string(tempVarCounter_++));
		decl["vars"].push_back({{"var-name", temps.back()}, {"var-type", r["var-type"]}});
	}
	for (auto& s : sa_tapple_decl(decl))
		body.push_back(s);

	json call = chain["call"];
	const json* callee = findCallee(call, isCFunc);
	json args = json::array();
	for (size_t i = 0; i < temps.size(); i++) {
		json temp = {{"expr-type", "id"}, {"name", temps[i]}, {"loc", loc}};
		args.push_back(chainArg(temp, fixedParamType(callee, i), isCFunc));
	}
	for (auto& a : call["args"])
		args.push_back(a);
	call["args"] = move(args);
	return call;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_chain_call(const json& expr)
{
	json norm = normalizeChainCall(expr);
	if (norm["expr-type"] != "chain-call")
		return sa_expr_maybe_void(norm);

	enterScope();
	json body = json::array();
	json saCall = sa_expr_maybe_void(chainMultiCall(norm, body));
	size_t idx = arrayScopeVars_.size() - 1;
	json release = collectFreeStmts(idx, idx + 1);
	leaveScope();
	return makeTempExpr(move(body), move(saCall), move(release));
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_member_call(const json& expr)
{
	const string& alias = expr["object"]["name"].get<string>();
	const string& method = expr["method"].get<string>();
	requireAlias(expr, alias);
	vector<Callee> cands = findCalleesByAlias(alias, method);
	if (cands.empty()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedFunction, method) << endl;
		exit(1);
	}
	return makeCallNode(expr, requireUniqueCallee(expr, method, cands), method);
} // LCOV_EXCL_EXCEPTION_BR_LINE

const json& PlnSemanticAnalyzer::findMultiRetFunc(const json& stmt, const json& callExpr, size_t recvCount)
{
	vector<Callee> cands;
	string fname;
	if (callExpr["expr-type"] == "member-call") {
		const string& alias = callExpr["object"]["name"].get<string>();
		fname = callExpr["method"].get<string>();
		requireAlias(stmt, alias);
		cands = findCalleesByAlias(alias, fname);
	} else {
		fname = callExpr["name"].get<string>();
		cands = findCallees(fname);
	}
	if (cands.empty()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleUndefinedFunction, fname) << endl;
		exit(1);
	}
	const json* pFunc = requireUniqueCallee(stmt, fname, cands).def;
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

// The call whose return values a multiple-return declaration or assignment
// receives.
json PlnSemanticAnalyzer::tappleCallExpr(const json& stmt)
{
	json value = stmt["value"];
	if (value.value("expr-type", "") == "chain-call") {
		value = normalizeChainCall(value);
		if (value["expr-type"] == "chain-call") {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ChainMultiToTapple) << endl;
			exit(1);
		}
	}
	return normalizeMethodCall(value);
}
