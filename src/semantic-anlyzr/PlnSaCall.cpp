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

// The registered definition of the function `funcDef` defines, among those
// sharing its name.
const json* PlnSemanticAnalyzer::findPlnFuncDef(const json& funcDef) const
{
	const string& name = funcDef["name"].get<string>();
	// The definition is always registered, in the scope being analyzed.
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) { // LCOV_EXCL_BR_LINE
		auto f = it->find(name);
		if (f == it->end()) continue; // LCOV_EXCL_BR_LINE
		for (auto& def : f->second) // LCOV_EXCL_BR_LINE
			if (def.value("loc", json()) == funcDef.value("loc", json())) return &def;
	}
	BOOST_ASSERT(false); // LCOV_EXCL_LINE
	return nullptr; // LCOV_EXCL_LINE
}

// One function brought in more than once (a header included again, a module
// imported again) has one key.
string PlnSemanticAnalyzer::calleeKey(const json& def, bool isC) const
{
	if (isC) return "c:" + cLinkName(def).get<string>();
	string key = def.value("_module", moduleId_) + ":" + def["name"].get<string>();
	for (auto& p : def.value("parameters", json::array()))
		key += "|" + p["var-type"].dump();
	return key;
}

// The functions `alias.name` names in the innermost scope that imports one.
vector<Callee> PlnSemanticAnalyzer::findCalleesByAlias(const string& alias, const string& name) const
{
	vector<Callee> cands;
	set<string> seen;
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find(alias);
		if (alias_it == it->end()) continue;
		auto f_it = alias_it->second.find(name);
		if (f_it == alias_it->second.end()) continue;
		for (auto& def : f_it->second) {
			bool isC = def.value("_c-func", false);
			if (seen.insert(calleeKey(def, isC)).second) cands.push_back({&def, isC});
		}
		break;
	}
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The functions a call by `name` can mean: C functions, Palan functions,
// unqualified imports and `[+]T` built-ins alike, each once.
vector<Callee> PlnSemanticAnalyzer::findCallees(const string& name) const
{
	vector<Callee> cands;
	set<string> seen;
	auto add = [&](const json& def, bool isC) {
		if (seen.insert(calleeKey(def, isC)).second) cands.push_back({&def, isC});
	};
	for (auto it = cFuncScopes.rbegin(); it != cFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) add(f->second, true);
	}
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end())
			for (auto& def : f->second) add(def, false);
	}
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find("");
		if (alias_it == it->end()) continue;
		auto f = alias_it->second.find(name);
		if (f != alias_it->second.end())
			for (auto& def : f->second) add(def, def.value("_c-func", false));
	}
	auto b = varrBuiltins_.find(name);
	if (b != varrBuiltins_.end())
		for (auto& def : b->second) add(def, false);
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

void PlnSemanticAnalyzer::requireAlias(const json& locNode, const string& alias) const
{
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		if (it->count(alias)) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownAlias, alias) << endl;
	exit(1);
}

static bool isVariadicSig(const json& funcParams)
{
	for (auto& p : funcParams)
		if (p.value("name", "") == "...") return true;
	return false;
}

static bool isAggregateLitArg(const json& arg)
{
	return isAggregateLit(arg)
		|| (arg.value("expr-type", "") == "addr-of" && isAggregateLit(arg["object"])); // LCOV_EXCL_EXCEPTION_BR_LINE
}

static bool isScalar(const PlnType* t)
{
	return t->kind == PlnType::Kind::Prim || t->kind == PlnType::Kind::Enum;
}

static bool firstParamMutable(const json& def)
{
	const json& params = def["parameters"];
	return !params.empty() && params[0]["var-type"].value("mutable", false);
}

// The type registry does not tell a read-only pointer from a writable one.
bool PlnSemanticAnalyzer::sameArgType(const json& argVT, const json& paramVT)
{
	if (registry_.fromJson(argVT) != registry_.fromJson(paramVT)) return false;
	return paramVT.value("type-kind", "") != "pntr" // LCOV_EXCL_EXCEPTION_BR_LINE
		|| argVT.value("mutable", true) == paramVT.value("mutable", true); // LCOV_EXCL_EXCEPTION_BR_LINE
}

// How an argument fits a parameter. A pending argument is judged by what it
// can become once the parameter is known: a literal fits any primitive and is
// exact only for its default type, an aggregate literal fits any pointer or
// struct and is never exact, and a function name is exact when one of its
// functions has the parameter's type.
ArgFit PlnSemanticAnalyzer::argFit(const CallArg& arg, const json& param, size_t argIdx,
                                   bool isCFunc, const string& funcName)
{
	const json& paramVT = param["var-type"];
	auto fitOf = [&](const json& ast, const json& sa0) {
		json sa = takesVarrData(sa0["value-type"], &paramVT, isCFunc) ? varrData(sa0) : sa0;
		if (callArgMismatch(ast, sa, param, argIdx, isCFunc, funcName)) return ArgFit::None;
		return sameArgType(sa["value-type"], paramVT) ? ArgFit::Exact : ArgFit::Viable;
	};
	if (!arg.pending) return fitOf(arg.ast, arg.sa);
	if (isAggregateLitArg(arg.ast))
		return paramVT.value("type-kind", "") == "pntr" || isStructStorage(paramVT) ? ArgFit::Viable : ArgFit::None; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (namesOverloadedFunc(arg.ast)) {
		const PlnType* want = registry_.fromJson(paramVT);
		for (auto& c : findCallees(arg.ast["name"].get<string>())) {
			json fp;
			if (funcRefType(*c.def, fp).empty() && registry_.fromJson(fp) == want) return ArgFit::Exact; // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		return ArgFit::None;
	}
	if (!arg.chained || !arg.probe.contains("expr-type")) {
		const PlnType* t = registry_.fromJson(paramVT);
		if (t->kind != PlnType::Kind::Prim) return ArgFit::None;
		return !arg.probe.is_null() && sameArgType(arg.probe, paramVT) ? ArgFit::Exact : ArgFit::Viable;
	}
	// A chained value: '@v' when the parameter would take it so.
	const json& vt = arg.probe["value-type"];
	json form = chainArg(arg.ast, &paramVT, isCFunc, &vt);
	if (form.value("expr-type", "") != "addr-of" || arg.ast.value("expr-type", "") == "addr-of") // LCOV_EXCL_EXCEPTION_BR_LINE
		return fitOf(arg.ast, arg.probe);
	const json& base = paramVT["base-type"];
	bool match = isArrLevel(vt) ? arrShapeMatch(vt, paramVT)
	           : isStructStorage(vt) ? base.value("type-kind", "") == "struct" // LCOV_EXCL_EXCEPTION_BR_LINE
	                                   && base["type-name"] == vt["base-type"]["type-name"]
	           : isVarrStorage(vt) ? base == vt["base-type"]
	           : base == vt;
	return match ? ArgFit::Exact : ArgFit::None;
}

string PlnSemanticAnalyzer::argTypesText(const vector<CallArg>& args)
{
	string text;
	for (auto& a : args) {
		if (!text.empty()) text += ", "; // LCOV_EXCL_EXCEPTION_BR_LINE
		const json& vt = !a.pending ? a.sa["value-type"]
		               : a.chained && a.probe.contains("value-type") ? a.probe["value-type"]
		               : a.probe;
		text += vt.is_object() ? typeDisplayName(vt) // LCOV_EXCL_EXCEPTION_BR_LINE
		      : namesOverloadedFunc(a.ast) ? string("function")
		      : string("literal");
	}
	return text;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

string PlnSemanticAnalyzer::calleesText(const string& name, const vector<Callee>& cands) const
{
	string text;
	for (auto& c : cands) {
		if (!text.empty()) text += ", "; // LCOV_EXCL_EXCEPTION_BR_LINE
		text += name + "(";
		bool first = true;
		for (auto& p : (*c.def)["parameters"]) {
			if (!first) text += ", ";
			first = false;
			text += p.value("name", "") == "..." ? string("...") : typeDisplayName(p["var-type"]); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		text += ")";
	}
	return text;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// Among several functions of one name, the one every argument fits; when more
// than one fits, the one every argument matches exactly.
Callee PlnSemanticAnalyzer::selectCallee(const json& locNode, const string& name,
                                         const vector<Callee>& cands, const vector<CallArg>& args)
{
	vector<Callee> viable, exact;
	for (auto& c : cands) {
		const json& params = (*c.def)["parameters"];
		bool variadic = isVariadicSig(params);
		size_t fixedCount = params.size() - (variadic ? 1 : 0);
		if (variadic ? args.size() < fixedCount : args.size() != fixedCount) continue;
		bool fits = true;
		bool allExact = args.size() == fixedCount;
		for (size_t i = 0; i < fixedCount && fits; i++) {
			ArgFit f = argFit(args[i], params[i], i, c.isC, name);
			fits = f != ArgFit::None;
			if (f == ArgFit::Viable) allExact = false;
		}
		if (!fits) continue;
		viable.push_back(c);
		if (allExact) exact.push_back(c);
	}
	if (viable.size() == 1) return viable[0];
	if (viable.empty()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_NoMatchingFunc,
			name, argTypesText(args), calleesText(name, cands)) << endl;
		exit(1);
	}
	if (exact.size() == 1) return exact[0];
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_AmbiguousCall,
		name, calleesText(name, exact.empty() ? viable : exact)) << endl;
	exit(1);
}

// The functions a call marked by normalizeMethodCall (a method-form call) or
// sa_tapple_decl (receiving several values) can mean.
vector<Callee> PlnSemanticAnalyzer::usableCallees(const json& expr, const vector<Callee>& cands) const
{
	vector<Callee> usable;
	for (auto& c : cands)
		if ((!expr.value("method-form", false) || firstParamMutable(*c.def))
		    && (!expr.contains("multi-ret")
		        || (*c.def).value("rets", json::array()).size() == expr["multi-ret"].get<size_t>())) // LCOV_EXCL_EXCEPTION_BR_LINE
			usable.push_back(c);
	return usable;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A chained value given to one of several functions: whether it is borrowed
// ('@v') depends on the function chosen, so bindCallArgs decides it.
static json chainedValue(const json& value)
{
	return {{"expr-type", "chained-value"}, {"value", value}}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A call of one of `cands`: a single one is bound as it is, so its own
// diagnostics are what an unfit argument reports.
json PlnSemanticAnalyzer::makeCall(const json& expr, const vector<Callee>& cands, const string& writtenName)
{
	if (cands.size() == 1) return makeCallNode(expr, cands[0], writtenName);
	vector<CallArg> args = analyzeCallArgs(expr["args"]);
	Callee callee = selectCallee(expr, writtenName, cands, args);
	return makeCallNode(expr, callee, writtenName, &args);
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
	                    json::array({makeOwnedFreeStmt(name, storage)}));
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// '@f()'/'@!f()' as a call argument: the returned struct is borrowed for the
// call, held in a temp (bindCallTemp).
json PlnSemanticAnalyzer::sa_call_borrow_arg(const json& arg)
{
	json saArg = sa_expression(arg["object"]);
	if (!isExpiringOwned(saArg)) {
		cerr << locPrefix(arg) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
		exit(1);
	}
	bool isMutable = arg["mutable"];
	json& vt = saArg["value-type"];
	if (isArrLevel(vt)) vt = withArrPermission(vt, isMutable);
	else vt["mutable"] = isMutable;
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
		: paramVT->contains("arr-size") ? astTypeOf(*paramVT) : json();
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
		checkArrLitCount(lit, lit["items"], paramName, shape);
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

// The call node for `expr` calling `callee`, as written by `writtenName`.
json PlnSemanticAnalyzer::makeCallNode(const json& expr, const Callee& callee, const string& writtenName,
                                       vector<CallArg>* analyzedArgs)
{
	const json& def = *callee.def;
	json sa_expr = expr;
	sa_expr["expr-type"] = "call";
	sa_expr.erase("object");
	sa_expr.erase("method");
	sa_expr.erase("method-form");
	sa_expr.erase("multi-ret");
	if (callee.isC) {
		requireSupportedCFuncSig(def, writtenName, expr);
		sa_expr["func-type"] = "c";
		sa_expr["name"] = cLinkName(def);
		if (def.contains("ret-type") && def["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = def["ret-type"];
	} else {
		sa_expr["func-type"] = def["func-type"];
		sa_expr["name"] = def.contains("symbol") ? def["symbol"] : json(writtenName); // a syscall has none
		if (def.contains("ret-type"))
			sa_expr["value-type"] = def["ret-type"];
		if (def.value("func-type", "") == "syscall")
			sa_expr["syscall-number"] = def["syscall-number"];
	}
	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";
	sa_expr["args"] = analyzedArgs
		? bindCallArgs(expr, *analyzedArgs, def["parameters"], callee.isC, writtenName)
		: saCallArgs(expr, expr["args"], def["parameters"], callee.isC, writtenName);
	if (def.value("inline", false)) return varrLen(move(sa_expr["args"][0]));
	// A builtin's generated function also takes the lengths the element's type gives.
	if (def.contains("len-args"))
		for (auto& len : def["len-args"]) sa_expr["args"].push_back(len);
	return sa_expr;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The `@func` type a call through a pointer to `def` would have, or why
// there can be none.
string PlnSemanticAnalyzer::funcRefType(const json& def, json& funcPtr) const
{
	json func = {{"type-kind","func"},{"parameters",json::array()}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (def.value("func-type", "") == "syscall") return "it is a syscall";
	if (def.contains("rets") && def["rets"].size() > 1) return "it returns multiple values";
	// LCOV_EXCL_EXCEPTION_BR_START
	func["ret-type"] = def.contains("ret-type") ? def["ret-type"]
	                 : def.contains("rets") ? def["rets"][0]["var-type"]
	                 : json{{"type-kind","prim"},{"type-name","void"}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
	for (auto& p : def["parameters"]) {
		if (p.value("name", "") == "...") return "it takes variable arguments";
		func["parameters"].push_back({{"var-type", p["var-type"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	for (auto& p : func["parameters"])
		if (!isFuncSigType(p["var-type"]))
			return "its parameter type '" + typeDisplayName(p["var-type"]) + "' cannot be passed through one";
	const json& ret = func["ret-type"];
	if (ret.value("type-name", "") != "void" && !isFuncSigType(ret)) // LCOV_EXCL_EXCEPTION_BR_LINE
		return "its return type '" + typeDisplayName(ret) + "' cannot be returned through one";
	funcPtr = {{"type-kind","pntr"},{"mutable",false},{"base-type",move(func)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return "";
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A name that is not a variable but several functions: as a value, it needs
// the type it is given to.
bool PlnSemanticAnalyzer::namesOverloadedFunc(const json& expr) const
{
	if (expr.value("expr-type", "") != "id") return false;
	const string& name = expr["name"].get<string>();
	return !findVar(name) && !findCGlobal(name) && findCallees(name).size() > 1; // LCOV_EXCL_EXCEPTION_BR_LINE
}

// A function named as a value: its address, typed as the `@func` it can be
// called through. Null if `expr` names no function. Of several functions of
// the name, the one whose type expectedType is.
json PlnSemanticAnalyzer::sa_func_ref(const json& expr, const PlnType* expectedType)
{
	const string& name = expr["name"].get<string>();
	vector<Callee> cands = findCallees(name);
	if (cands.empty()) return json();
	Callee callee = cands[0];
	json funcPtr;
	if (cands.size() > 1) {
		vector<Callee> typed;
		for (auto& c : cands) {
			json fp;
			if (funcRefType(*c.def, fp).empty() && registry_.fromJson(fp) == expectedType) typed.push_back(c); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		if (typed.size() > 1) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AmbiguousCall,
				name, calleesText(name, typed)) << endl;
			exit(1);
		}
		if (typed.empty()) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNameNeedsType,
				name, calleesText(name, cands)) << endl;
			exit(1);
		}
		callee = typed[0];
	}
	if (callee.isC) requireSupportedCFuncSig(*callee.def, name, expr);
	string reason = funcRefType(*callee.def, funcPtr);
	if (!reason.empty()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNotValue, name, reason) << endl;
		exit(1);
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	json out = {{"expr-type", "func-ref"}, {"name", callee.isC ? cLinkName(*callee.def) : (*callee.def)["symbol"]},
	            {"value-type", move(funcPtr)}};
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
	for (auto& c : findCallees(method))
		if (firstParamMutable(*c.def)) {
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
	return makeCall(expr, usableCallees(expr, cands), callName);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Shared by sa_expr_call and sa_expr_member_call: these used to duplicate
// only part of this loop, silently dropping variadic promotion for an
// aliased call like `S.printf("%f\n", someFlo32)`.
json PlnSemanticAnalyzer::saCallArgs(const json& locNode, const json& args, const json& funcParams,
                                      bool isCFunc, const string& funcName)
{
	bool isVariadic = isVariadicSig(funcParams);
	size_t fixedCount = funcParams.size() - (isVariadic ? 1 : 0);
	// C's `()` arrives as an empty list just like `(void)` and is checked as
	// `(void)` (as C23 reads it); glibc headers are fully prototyped.
	if (isVariadic ? args.size() < fixedCount : args.size() != fixedCount) {
		string expected = (isVariadic ? "at least " : "") + to_string(fixedCount);
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArgCountMismatch,
			funcName, expected, to_string(args.size())) << endl;
		exit(1);
	} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here

	vector<CallArg> callArgs = analyzeCallArgs(args);
	return bindCallArgs(locNode, callArgs, funcParams, isCFunc, funcName);
}

// Analyzes the arguments, left to right, before any parameter is known.
vector<CallArg> PlnSemanticAnalyzer::analyzeCallArgs(const json& args)
{
	vector<CallArg> callArgs;
	for (auto& rawArg : args) {
		CallArg a;
		a.chained = rawArg.value("expr-type", "") == "chained-value";
		a.ast = a.chained ? rawArg["value"] : rawArg;
		bool typeless = isAggregateLitArg(a.ast) || namesOverloadedFunc(a.ast);
		bool literal = !typeless && takesExpectedType(a.ast);
		a.pending = a.chained || typeless || literal;
		if (literal)
			a.probe = literalDefaultType(a.ast);
		else if (!typeless)
			(a.chained ? a.probe : a.sa) = isCallBorrow(a.ast) ? sa_call_borrow_arg(a.ast) : sa_expression(a.ast);
		callArgs.push_back(move(a));
	}
	return callArgs;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// Whether an analyzed argument fits a parameter, and if not, why.
optional<SaDiag> PlnSemanticAnalyzer::callArgMismatch(const json& argAst, const json& saArg,
		const json& param, size_t argIdx, bool isCFunc, const string& funcName)
{
	const json& paramVT = param["var-type"];
	// What an owned parameter takes is copied into it.
	if (!isCFunc && isOwnedParam(paramVT)) {
		if (copySourceMatch(saArg["value-type"], paramVT)) return nullopt;
		return SaDiag{E_IncompatibleTypes, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	if (paramVT.contains("arr-size")) {
		if (auto d = arrBorrowBindingMismatch(argAst, saArg, paramVT)) return d;
	} else if (paramVT.value("embedded", false)) {
		// The type registry can't tell contiguous elements from pointer slots,
		// nor a contiguous struct array from a single struct.
		const json& argVT = saArg["value-type"];
		bool argContig = argVT.value("embedded", false) && argVT.contains("stride");
		const json& paramElem = paramVT["base-type"];
		if (isArrLevel(paramElem)) {
			const json* argRow = argContig ? &argVT["base-type"] : nullptr;
			bool argSized = argRow && isArrLevel(*argRow) && argRow->contains("arr-size");
			if (!argSized || (*argRow)["arr-size"] != paramElem["arr-size"]) {
				string actual = argSized
					? to_string((*argRow)["arr-size"].get<int64_t>()) : "variable";
				// LCOV_EXCL_EXCEPTION_BR_START
				return SaDiag{E_EmbeddedArrInnerSizeMismatch,
				              to_string(paramElem["arr-size"].get<int64_t>()), actual};
				// LCOV_EXCL_EXCEPTION_BR_STOP
			}
		} else if (!argContig) {
			return SaDiag{E_IncompatibleTypes, typeDisplayName(argVT), typeDisplayName(paramVT)}; // LCOV_EXCL_EXCEPTION_BR_LINE
		}
	}
	if (auto d = elemShapeMismatch(saArg, paramVT)) return d;
	if (auto d = callArgConvMismatch(saArg, paramVT)) return d;
	if (isStructBorrow(paramVT) && isStructStorage(saArg["value-type"]))
		return SaDiag{E_StructBorrowNeedsAddrOf}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (isVarrPntr(paramVT) && isPtrBorrow(paramVT) && isVarrStorage(saArg["value-type"]))
		return SaDiag{E_VarrBorrowNeedsAddrOf}; // LCOV_EXCL_EXCEPTION_BR_LINE
	// A by-value struct is only read, to copy it, so a read-only source is fine.
	if (isCFunc && isStructStorage(paramVT)) return nullopt;
	// An upgrade's diagnostic differs by callee kind, since a C parameter's
	// permission comes from const-qualification while a Palan parameter's comes
	// from `@!T`.
	if (isCFunc && ptrPermission(saArg["value-type"], paramVT) == PtrPermission::Upgrade) {
		string paramName = param.value("name", "");
		if (paramName.empty()) paramName = "#" + to_string(argIdx + 1);
		return SaDiag{E_ReadOnlyPtrToNonConstCParam, funcName, paramName}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	return ptrPermissionMismatch(saArg["value-type"], paramVT);
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// Binds the analyzed arguments to the chosen callee's parameters: a pending
// argument is analyzed now with its parameter's type, then each is checked
// and converted.
json PlnSemanticAnalyzer::bindCallArgs(const json& locNode, vector<CallArg>& args, const json& funcParams,
                                        bool isCFunc, const string& funcName)
{
	bool isVariadic = isVariadicSig(funcParams);
	size_t fixedCount = funcParams.size() - (isVariadic ? 1 : 0);
	json saArgs = json::array();
	for (size_t argIdx = 0; argIdx < args.size(); argIdx++) {
		CallArg& ca = args[argIdx];
		const json* param = argIdx < fixedCount ? &funcParams[argIdx] : nullptr;
		const json* paramVT = param ? &(*param)["var-type"] : nullptr;
		bool probed = ca.chained && ca.probe.contains("expr-type");
		json arg = ca.chained ? chainArg(ca.ast, paramVT, isCFunc, probed ? &ca.probe["value-type"] : nullptr)
		                      : ca.ast;
		bool byValStruct = isCFunc && paramVT && isStructStorage(*paramVT);
		json argAst = arg;
		json litBody, litRelease;
		bool isLit = isAggregateLitArg(arg);
		if (!isCFunc && paramVT && isOwnedParam(*paramVT)) {
			saArgs.push_back(ownedBlockArg(locNode, arg, ca, *param, argIdx, funcName));
			continue;
		}
		json saArg = isLit ? sa_lit_arg(arg, isCFunc && !byValStruct ? nullptr : paramVT,
		                                param ? param->value("name", "") : "", funcName,
		                                argAst, litBody, litRelease)
		           : probed && arg == ca.ast ? move(ca.probe)
		           : !ca.pending ? move(ca.sa)
		           : isCallBorrow(arg) ? sa_call_borrow_arg(arg)
		           : sa_expression(arg, paramVT ? registry_.fromJson(*paramVT) : nullptr);
		bool expiring = isCallBorrow(arg) || isExpiringOwned(saArg);
		if (takesVarrData(saArg["value-type"], paramVT, isCFunc))
			saArg = varrData(move(saArg));
		if (paramVT) {
			if (auto d = callArgMismatch(argAst, saArg, *param, argIdx, isCFunc, funcName))
				reportDiag(locNode, *d);
			saArg = convertCallArg(move(saArg), *paramVT);
		} else if (isVariadic) {
			const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
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
	}
	return saArgs;
}

// An owned parameter takes the argument over: a returned block as it is,
// anything else as a copy the callee frees.
json PlnSemanticAnalyzer::ownedBlockArg(const json& locNode, const json& arg, CallArg& ca,
                                         const json& param, size_t argIdx, const string& funcName)
{
	const json& paramVT = param["var-type"];
	bool isLit = isAggregateLitArg(arg);
	json saArg;
	if (!isLit) {
		saArg = !ca.pending ? move(ca.sa) : sa_expression(arg, registry_.fromJson(paramVT));
		if (auto d = callArgMismatch(arg, saArg, param, argIdx, false, funcName))
			reportDiag(locNode, *d);
		if (isExpiringOwned(saArg)) return saArg;
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = "__arg_" + to_string(tempVarCounter_++);
	json var = {{"name",name},{"var-type",astTypeOf(paramVT)}};
	if (isLit) {
		var["init"] = arg;
		var["lit-name"] = param.value("name", "");
	}
	json decl = {{"stmt-type","var-decl"},{"loc",arg.value("loc", json::array())},{"vars",json::array({var})}};
	enterScope();
	json body = sa_resolved_var_decl(decl);
	json value = {{"expr-type","id"},{"name",name},{"var-type",paramVT},{"value-type",paramVT}};
	if (!isLit) body.push_back(makeCopyStmt(arg, value, saArg));
	takeScopeRelease(name);
	leaveScope();
	return makeTempExpr(move(body), move(value), json::array());
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Embedded-array shape and pointer-permission checks are callArgMismatch's,
// since those differ by callee kind (C vs. Palan).
optional<SaDiag> PlnSemanticAnalyzer::callArgConvMismatch(const json& saArg, const json& paramVT)
{
	const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
	const PlnType* toType   = registry_.fromJson(paramVT);
	if (fromType == toType) return nullopt;
	if (isScalar(fromType) && isScalar(toType)) {
		if (argConvOk(fromType, toType)) return nullopt;
		return SaDiag{E_InvalidNarrowingConv, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	// pntr(void) is bidirectionally Identical with any pntr(T), which is how NULL passes.
	if (typeCompat(fromType, toType, registry_) == TypeCompat::Incompatible)
		return SaDiag{E_IncompatibleTypes, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return nullopt;
}

// Converts an argument callArgConvMismatch accepted to its parameter's type;
// it accepts a scalar only into a scalar.
json PlnSemanticAnalyzer::convertCallArg(json saArg, const json& paramVT)
{
	const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
	const PlnType* toType   = registry_.fromJson(paramVT);
	if (fromType == toType || !isScalar(fromType)) return saArg;
	return wrapConvert(saArg, registry_.toJson(toType));
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

// How many functions the call a value is chained to chooses among.
size_t PlnSemanticAnalyzer::countCallees(const json& call) const
{
	string et = call.value("expr-type", "");
	if (et == "call") return findCallees(call["name"].get<string>()).size();
	// chainTarget has rewritten a method-form call.
	BOOST_ASSERT(et == "member-call" && !isMethodForm(call));
	return findCalleesByAlias(call["object"]["name"].get<string>(), call["method"].get<string>()).size();
}

// The function a call, a module-alias call, a method-form call, or a chained
// call (its last call) names; null otherwise.
const json* PlnSemanticAnalyzer::findCallee(const json& call, bool& isCFunc) const
{
	isCFunc = false;
	string et = call.value("expr-type", "");
	if (et == "chain-call")
		return findCallee(call["call"], isCFunc);
	if (et == "call")
		return findCalleeByName(call["name"].get<string>(), isCFunc);
	if (et != "member-call")
		return nullptr;
	if (isMethodForm(call))
		return findCalleeByName(call["method"].get<string>(), isCFunc);
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
	vector<Callee> cands = findCallees(method);
	if (!cands.empty() && none_of(cands.begin(), cands.end(),
	                              [](const Callee& c) { return firstParamMutable(*c.def); })) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_MethodNeedsMutableFirstParam, method) << endl;
		exit(1);
	}

	// A receiver that is already a borrow is passed as is: '@!' on it would
	// be rejected (or would mean a pointer to the borrow's slot).
	json recvType = operandValueType(obj);
	bool isBorrow = isPtrBorrow(recvType) || isArrBorrowVar(recvType);
	json recv = isBorrow ? obj : json{{"expr-type", "addr-of"}, {"mutable", true}, {"object", obj}};
	if (!isBorrow && obj.contains("loc")) recv["loc"] = obj["loc"];

	json call = {{"expr-type", "call"}, {"name", method}, {"args", json::array({recv})}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (cands.size() > 1) call["method-form"] = true;
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
json PlnSemanticAnalyzer::chainArg(const json& value, const json* paramVT, bool isCFunc, const json* valueType)
{
	if (!paramVT || value.value("expr-type", "") == "addr-of") return value;
	if (paramVT->value("type-kind", "") != "pntr" || paramVT->value("mutable", true)) return value;
	// A literal has no type of its own until it is given a target, so it is
	// borrowed as '@[..]'/'@{..}' and checked against the parameter.
	string et = value.value("expr-type", "");
	if (et != "arr-lit" && et != "dict-lit") {
		json vt = valueType ? *valueType : operandValueType(value);
		bool ownedArr = isArrLevel(vt) && !isArrBorrowVar(vt);
		if (!isStructStorage(vt) && !isVarrStorage(vt) && vt.value("type-kind", "") != "prim"
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

// The call a value is chained to, and where the chained values go in its
// arguments: after the receiver of a method-form call.
json PlnSemanticAnalyzer::chainTarget(const json& call, size_t& at)
{
	json out = normalizeMethodCall(call);
	at = call["expr-type"] == "member-call" && out["expr-type"] == "call" ? 1 : 0;
	return out;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

json PlnSemanticAnalyzer::chainInto(json call, size_t at, const vector<json>& values)
{
	bool isCFunc;
	const json* callee = findCallee(call, isCFunc);
	bool overloaded = countCallees(call) > 1;
	json& args = call["args"];
	for (size_t i = 0; i < values.size(); i++) {
		json arg = overloaded ? chainedValue(values[i]) : chainArg(values[i], fixedParamType(callee, at + i), isCFunc);
		args.insert(args.begin() + at + i, move(arg)); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	return call;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A value chained to a method-form call, evaluated into a temp in body so that
// it comes before the receiver; returns what is chained in its place. What no
// call can change, and what takes its type from the parameter, is left as is.
json PlnSemanticAnalyzer::chainValueAhead(const json& value, const json* paramVT, json& body)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	if (foldConstExpr(value)["expr-type"].get<string>().rfind("lit-", 0) == 0
	    || isAggregateLitArg(value) || namesOverloadedFunc(value))
		return value;
	auto tempId = [&](const string& name) {
		json id = {{"expr-type", "id"}, {"name", name}};
		if (value.contains("loc")) id["loc"] = value["loc"];
		return id;
	};
	if (isCallBorrow(value)) {
		json sa = sa_call_borrow_arg(value);
		sa["value-type"].erase("mutable");
		json storage = sa["value-type"];
		string name = pinTemp(move(sa), value, body);
		addOwnedScopeVar(name, makeOwnedFreeStmt(name, storage));
		return {{"expr-type", "addr-of"}, {"mutable", value["mutable"]}, {"object", tempId(name)}};
	}
	json sa;
	if (isLocationExpr(value) && isCopiedByValue(operandValueType(value))) {
		// A variable's storage stays where it is; that of a field or element
		// is held by its address.
		if (value["expr-type"] == "id") return value;
		sa = sa_expression({{"expr-type", "addr-of"}, {"mutable", false}, {"object", value}});
		sa["value-type"].erase("mutable");
	} else {
		bool typed = paramVT && paramVT->value("type-kind", "") != "pntr";
		sa = sa_expression(value, typed ? registry_.fromJson(*paramVT) : nullptr);
		if (!sa.contains("value-type")) return value;
	}
	noteEscape(sa);
	bool owned = isExpiringOwned(sa);
	json type = sa["value-type"];
	string name = pinTemp(move(sa), value, body);
	if (name.empty()) return value;
	if (owned) addOwnedScopeVar(name, makeOwnedFreeStmt(name, type));
	return tempId(name);
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Rewrites a chained call `v -> f(args)` to `f(v, args)`, and `v -> x.f(args)`
// to `f(@!x, v, args)`. A chain whose value is evaluated into temps first -- a
// call returning several values, or a value given to a method-form call where
// it or the receiver has a call -- is left as a chain-call for
// sa_expr_chain_call.
json PlnSemanticAnalyzer::normalizeChainCall(const json& expr)
{
	json value = expr["value"];
	if (value.value("expr-type", "") == "chain-call")
		value = normalizeChainCall(value);
	const json& call = expr["call"];
	bool method = call["expr-type"] == "member-call" && isMethodForm(call);
	if (chainRetCount(value) >= 2 || (method && (hasCall(value) || hasCall(call["object"])))) {
		json out = expr;
		out["value"] = move(value);
		return out;
	}
	size_t at;
	json target = chainTarget(call, at);
	return chainInto(move(target), at, {move(value)});
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Evaluates a chained call's values into temps, appending their declarations
// to body, and returns the call with the temps in their place.
json PlnSemanticAnalyzer::chainAheadCall(const json& chain, json& body)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json value = chain["value"];
	if (value["expr-type"] == "chain-call")
		value = chainAheadCall(value, body);
	size_t at;
	json call = chainTarget(chain["call"], at);
	bool isCFunc;
	if (chainRetCount(value) < 2) {
		const json* paramVT = countCallees(call) > 1 ? nullptr : fixedParamType(findCallee(call, isCFunc), at);
		return chainInto(move(call), at, {chainValueAhead(value, paramVT, body)});
	}
	const json& rets = (*findCallee(value, isCFunc))["rets"];
	json loc = chain.value("loc", json::array());
	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", value}, {"loc", loc}};
	vector<json> temps;
	for (auto& r : rets) {
		string name = "__tap_" + to_string(tempVarCounter_++);
		decl["vars"].push_back({{"var-name", name}, {"var-type", r["var-type"]}});
		temps.push_back({{"expr-type", "id"}, {"name", name}, {"loc", loc}});
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	for (auto& s : sa_tapple_decl(decl))
		body.push_back(s);
	return chainInto(move(call), at, temps);
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_chain_call(const json& expr)
{
	json norm = normalizeChainCall(expr);
	if (norm["expr-type"] != "chain-call")
		return sa_expr_maybe_void(norm);

	enterScope();
	json body = json::array();
	json saCall = sa_expr_maybe_void(chainAheadCall(norm, body));
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
	return makeCall(expr, usableCallees(expr, cands), method);
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
	const json* pFunc = cands[0].def;
	if (cands.size() > 1) {
		// The receivers are declared before the arguments choose a function, so
		// those returning recvCount values must agree on what they return.
		vector<Callee> multi;
		for (auto& c : cands)
			if ((*c.def).value("rets", json::array()).size() == recvCount) multi.push_back(c);
		if (!multi.empty()) {
			pFunc = multi[0].def;
			for (auto& c : multi)
				if ((*c.def)["rets"] != (*pFunc)["rets"]) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AmbiguousCall,
						fname, calleesText(fname, multi)) << endl;
					exit(1);
				}
		}
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
