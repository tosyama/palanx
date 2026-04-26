#include <iostream>
#include <fstream>
#include <filesystem>
#include <map>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"

using namespace std;

// LCOV_EXCL_EXCEPTION_BR_START
static json wrapConvert(const json& expr, const json& to_type) {
	return {
		{"expr-type",  "convert"},
		{"value-type", to_type},
		{"from-type",  expr["value-type"]},
		{"src",        expr}
	};
}
// LCOV_EXCL_EXCEPTION_BR_STOP

// Returns element byte size for a primitive type name; -1 if unknown
static int elemSizeBytes(const string& typeName)
{
	if (typeName == "int8"  || typeName == "uint8")  return 1;
	if (typeName == "int16" || typeName == "uint16") return 2;
	if (typeName == "int32" || typeName == "uint32" || typeName == "flo32") return 4;
	if (typeName == "int64" || typeName == "uint64" || typeName == "flo64") return 8;
	return -1;
}

// Builds a synthetic free() expression statement for the named pointer variable
// LCOV_EXCL_EXCEPTION_BR_START
static json makeFreeStmt(const string& name, const json& pntrType)
{
	return {
		{"stmt-type", "expr"},
		{"body", {
			{"expr-type", "call"}, {"name", "free"}, {"func-type", "c"},
			{"args", json::array({{
				{"expr-type", "id"}, {"name", name},
				{"var-type", pntrType}, {"value-type", pntrType}
			}})}
		}}
	};
}
// LCOV_EXCL_EXCEPTION_BR_STOP

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

static const PlnType* variadicPromote(const PlnType* t, PlnTypeRegistry& reg)
{
	if (t->kind != PlnType::Kind::Prim) return t;
	const auto* p = static_cast<const PrimType*>(t);
	using N = PrimType::Name;
	switch (p->name) {
		case N::Int8:   case N::Int16:  return reg.prim(N::Int32);
		case N::Uint8:  case N::Uint16: return reg.prim(N::Uint32);
		case N::Float32:                return reg.prim(N::Float64);
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

static json unsizedArrToPntr(const json& type) {
	if (type.value("type-kind","") == "arr"
		&& type.value("specifier","") == "raw"
		&& type["size-expr"].is_null())
		return {{"type-kind","pntr"},{"base-type", unsizedArrToPntr(type["base-type"])}};
	return type;
}

static void normalizeUnsizedArrSig(json& funcDef) {
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			if (p.contains("var-type"))
				p["var-type"] = unsizedArrToPntr(p["var-type"]);
	if (funcDef.contains("ret-type"))
		funcDef["ret-type"] = unsizedArrToPntr(funcDef["ret-type"]);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
			if (r.contains("var-type"))
				r["var-type"] = unsizedArrToPntr(r["var-type"]);
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
		else if (t == "var-decl") { for (auto& s : sa_var_decl(stmt)) result.push_back(s); }
		else if (t == "assign")     result.push_back(sa_assign_stmt(stmt));
		else if (t == "arr-assign") { for (auto& s : sa_arr_assign_stmt(stmt)) result.push_back(s); }
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

json PlnSemanticAnalyzer::sa_expression(const json &expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	if (expr_type == "lit-int") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim)
			sa_expr["value-type"] = registry_.toJson(expectedType);
		else
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Int64));

	} else if (expr_type == "lit-flo") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim) {
			auto pn = static_cast<const PrimType*>(expectedType)->name;
			if (pn == PrimType::Name::Float32 || pn == PrimType::Name::Float64)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Float64));
		} else {
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Float64));
		}

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
			if (isInArrayScope(expr["name"].get<string>()))
				sa_expr["category"] = "owned";
		} else {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedVariable, expr["name"]) << endl;
			exit(1);
		}

	} else if (expr_type == "add" || expr_type == "sub"
	        || expr_type == "mul" || expr_type == "div" || expr_type == "mod") {
		return sa_expr_arith(expr, expectedType);

	} else if (expr_type == "neg") {
		json operand = sa_expression(expr["operand"]);
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = operand["value-type"];

	} else if (expr_type == "cmp") {
		json left  = sa_expression(expr["left"]);
		json right = sa_expression(expr["right"]);
		const PlnType* leftType  = registry_.fromJson(left["value-type"]);
		const PlnType* rightType = registry_.fromJson(right["value-type"]);
		if (typeCompat(leftType, rightType, registry_) == TypeCompat::ImplicitWiden) {
			left = wrapConvert(left, registry_.toJson(rightType));
		} else if (typeCompat(rightType, leftType, registry_) == TypeCompat::ImplicitWiden) {
			right = wrapConvert(right, registry_.toJson(leftType));
		}
		sa_expr["op"]         = expr["op"];
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "int32"}};

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
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_IncompatibleTypeCast,
				src["value-type"].value("type-name", src["value-type"]["type-kind"].get<string>()),
				expr["target-type"]["type-name"].get<string>()) << endl;
			exit(1);
		}

	} else if (expr_type == "call") {
		return sa_expr_call(expr);

	} else if (expr_type == "arr-index") {
		return sa_expr_arr_index(expr);
	}
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arith(const json& expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	// First pass: propagate expectedType so that lit-int leaves can adopt the
	// context type (e.g. both sides of `(4+4)` in `int32 x = (4+4)` → int32).
	json left  = sa_expression(expr["left"],  expectedType);
	json right = sa_expression(expr["right"], expectedType);
	// Second pass: if one side is a lit-int and the other has a concrete type
	// (from a variable), re-type the literal to match — this takes precedence
	// over expectedType (e.g. `int64_var + 4` keeps int64, not expectedType).
	if (expr["left"]["expr-type"] == "lit-int" && right.contains("value-type")) {
		left = sa_expression(expr["left"], registry_.fromJson(right["value-type"]));
	} else if (expr["right"]["expr-type"] == "lit-int" && left.contains("value-type")) {
		right = sa_expression(expr["right"], registry_.fromJson(left["value-type"]));
	}
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
	if (expr_type == "mod" && promoted->kind == PlnType::Kind::Prim) {
		auto pn = static_cast<const PrimType*>(promoted)->name;
		if (pn == PrimType::Name::Float32 || pn == PrimType::Name::Float64) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FloatModulo) << endl;
			exit(1);
		}
	}
	sa_expr["left"]       = left;
	sa_expr["right"]      = right;
	sa_expr["value-type"] = registry_.toJson(promoted);
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_call(const json& expr)
{
	json sa_expr = expr;
	const json* funcParams = nullptr;
	bool isVariadic = false;

	const json* cfunc = findCFunc(expr["name"]);
	if (cfunc) {
		sa_expr["func-type"] = "c";
		if (cfunc->contains("ret-type")
				&& (*cfunc)["ret-type"].value("type-name", "") != "void")
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
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedFunction,
				expr["name"].get<string>()) << endl;
			exit(1);
		}
	}

	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";

	if (expr.contains("args")) {
		sa_expr["args"] = json::array();
		size_t fixedCount = funcParams ? (funcParams->size() - (isVariadic ? 1 : 0)) : 0;
		size_t argIdx = 0;
		for (auto& arg : expr["args"]) {
			json saArg = sa_expression(arg);
			if (saArg.contains("value-type")) {
				const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
				if (funcParams && argIdx < fixedCount) {
					const PlnType* toType = nullptr;
					try { toType = registry_.fromJson((*funcParams)[argIdx]["var-type"]); }
					catch (const std::runtime_error&) {}
					if (toType && typeCompat(fromType, toType, registry_) == TypeCompat::ImplicitWiden)
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
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arr_index(const json& expr)
{
	json sa_expr = expr;
	json sa_array = sa_expression(expr["array"]);
	const json& array_type = sa_array["value-type"];
	if (array_type.value("type-kind", "") != "pntr") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_NotArrayType) << endl;
		exit(1);
	}
	const json& elem_type = array_type["base-type"];

	json sa_index = sa_expression(expr["index"]);
	const PlnType* idxType = registry_.fromJson(sa_index["value-type"]);
	if (idxType->kind == PlnType::Kind::Prim) {
		auto pn = static_cast<const PrimType*>(idxType)->name;
		if (pn == PrimType::Name::Float32 || pn == PrimType::Name::Float64) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArrayIndexNotInteger) << endl;
			exit(1);
		}
	}

	int sz = (elem_type.value("type-kind","") == "pntr") ? 8
		: elemSizeBytes(elem_type.value("type-name",""));
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	json elem_size_node = {
		{"expr-type", "lit-uint"}, {"value", to_string(sz)}, {"value-type", uint64_type}
	};

	sa_expr["array"]      = sa_array;
	sa_expr["index"]      = sa_index;
	sa_expr["elem-size"]  = elem_size_node;
	sa_expr["value-type"] = elem_type;
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expression_stmt(const json& stmt)
{
	return {
		{"stmt-type", "expr"},
		{"body", sa_expression(stmt["body"])}
	};
}

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	if (!stmt["vars"].empty()) {
		const json& vtype = stmt["vars"][0]["var-type"];
		string tk = vtype.value("type-kind", "");

		if (tk == "arr" && vtype.value("specifier", "") == "raw" && vtype["size-expr"].is_null()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnsizedArrVarDecl) << endl;
			exit(1);
		}
		if (tk == "arr" && vtype.value("specifier", "") == "raw" && !vtype["size-expr"].is_null())
			return sa_arr_var_decl(stmt);
	}

	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt["vars"]) {
		string name = var["name"];
		json sa_var = var;
		if (var.contains("init")) {
			// Evaluate init before declaring the variable so that the variable
			// itself is not in scope during its own initializer (e.g. `int32 z = z+1`
			// should be an error, not silently read uninitialized z).
			const PlnType* toType = registry_.fromJson(var["var-type"]);
			json init = sa_expression(var["init"], toType);
			if (!init.contains("value-type")) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
				exit(1);
			}
			const json& var_type = var["var-type"];
			if (init["value-type"] != var_type) {
				const PlnType* fromType = registry_.fromJson(init["value-type"]);
				TypeCompat compat = typeCompat(fromType, toType, registry_);
				if (compat == TypeCompat::ImplicitWiden) {
					init = wrapConvert(init, var_type);
				} else if (compat == TypeCompat::ExplicitCast) {
					string et = init["expr-type"];
					if (et != "lit-int" && et != "lit-uint") {
						cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_InvalidNarrowingInit) << endl;
						exit(1);
					}
				}
				// Incompatible: no action (ptr types pass through as-is)
			}
			sa_var["init"] = init;
		}
		declareVar(name, var["var-type"], &stmt);
		sa_stmt["vars"].push_back(sa_var);
	}
	return json::array({sa_stmt});
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_var_decl(const json& stmt)
{
	// Array var-decl: transform to pntr var-decl + malloc init.
	// All vars in the declaration share the same var-type, so compute
	// size-in-bytes only once.
	const json& vtype = stmt["vars"][0]["var-type"];
	const json& base_type = vtype["base-type"];

	// 2D array: [m][n]T → pntr(pntr(T)) with __pln_alloc_arr_arr_T init
	if (base_type.value("type-kind","") == "arr") {
		const json& leaf_type = base_type["base-type"];
		string leaf_name = leaf_type.value("type-name","");
		string shape_key = "arr_arr_" + leaf_name;

		// Register shape in sa["alloc-shapes"] (deduplicated by shape-key)
		bool found = false;
		for (auto& s : sa["alloc-shapes"])
			if (s["shape-key"] == shape_key) { found = true; break; }
		if (!found)
			sa["alloc-shapes"].push_back({
				{"shape-key", shape_key}, {"leaf-type", leaf_name}, {"depth", 2}
			});

		string alloc_func = "__pln_alloc_" + shape_key;
		string free_func  = "__pln_free_"  + shape_key;

		json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
		json pntr_type = {{"type-kind","pntr"},{"base-type",{
			{"type-kind","pntr"},{"base-type",leaf_type}
		}}};
		const PlnType* uint64Type = registry_.prim(PrimType::Name::Uint64);

		auto checkIntSize = [&](const json& sz) {
			if (!sz.contains("value-type")) return;
			const PlnType* t = registry_.fromJson(sz["value-type"]);
			bool is_int = t->kind == PlnType::Kind::Prim
				&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float32
				&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float64;
			if (!is_int) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArraySizeNotInteger) << endl;
				exit(1);
			}
		};

		json result = json::array();
		for (auto& var : stmt["vars"]) {
			string name = var["name"];
			string d0_name = "__" + name + "_d0";

			json d0_expr = sa_expression(vtype["size-expr"], uint64Type);
			checkIntSize(d0_expr);
			json n_expr  = sa_expression(base_type["size-expr"], uint64Type);
			checkIntSize(n_expr);

			json d0_id = {{"expr-type","id"},{"name",d0_name},
			              {"var-type",uint64_type},{"value-type",uint64_type}};
			json mat_id = {{"expr-type","id"},{"name",name},
			               {"var-type",pntr_type},{"value-type",pntr_type}};

			declareVar(d0_name, uint64_type, &stmt);
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",d0_name},{"var-type",uint64_type},{"init",d0_expr}
			}})}});

			json alloc_call = {
				{"expr-type","call"}, {"name",alloc_func}, {"func-type","palan"},
				{"args",json::array({d0_id, n_expr})}, {"value-type",pntr_type}
			};
			json free_stmt_json = {{"stmt-type","expr"},{"body",{
				{"expr-type","call"}, {"name",free_func}, {"func-type","palan"},
				{"args",json::array({mat_id, d0_id})}
			}}};

			declareVar(name, pntr_type, &stmt);
			arrayScopeVars_.back().push_back({name, free_stmt_json});
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",name},{"var-type",pntr_type},{"init",alloc_call}
			}})}});
		}
		return result;
	} // LCOV_EXCL_EXCEPTION_BR_LINE

	int elem_size;
	json sa_elem_type;
	if (base_type.value("type-kind","") == "pntr") {
		// [n]@![]T: elem is mutable pntr to unsized arr → size = 8, SA elem = pntr(T)
		elem_size = 8;
		sa_elem_type = unsizedArrToPntr(base_type["base-type"]);
	} else {
		elem_size = elemSizeBytes(base_type.value("type-name",""));
		sa_elem_type = base_type;
	}

	const PlnType* uint64Type = registry_.prim(PrimType::Name::Uint64);
	json sa_count = sa_expression(vtype["size-expr"], uint64Type);

	if (sa_count.contains("value-type")) {
		const PlnType* cntType = registry_.fromJson(sa_count["value-type"]);
		bool is_int = cntType->kind == PlnType::Kind::Prim
			&& static_cast<const PrimType*>(cntType)->name != PrimType::Name::Float32
			&& static_cast<const PrimType*>(cntType)->name != PrimType::Name::Float64;
		if (!is_int) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArraySizeNotInteger) << endl;
			exit(1);
		}
	}

	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	json size_bytes;
	if (elem_size == 1) {
		size_bytes = sa_count;
	} else {
		json elem_lit = {
			{"expr-type", "lit-uint"}, {"value", to_string(elem_size)}, {"value-type", uint64_type}
		};
		size_bytes = {
			{"expr-type", "mul"}, {"value-type", uint64_type},
			{"left", sa_count}, {"right", elem_lit}
		};
	}

	json pntr_type = {{"type-kind","pntr"},{"base-type",sa_elem_type}};

	// For multiple vars: emit a temp uint64 var holding size_bytes so it
	// is evaluated only once at runtime.  For a single var, inline it.
	json result = json::array();
	json size_arg;
	if (stmt["vars"].size() > 1) {
		string sz_name = "__arr_sz_" + to_string(tempVarCounter_++);
		declareVar(sz_name, uint64_type, &stmt);
		result.push_back({
			{"stmt-type", "var-decl"},
			{"vars", json::array({{{"name", sz_name}, {"var-type", uint64_type}, {"init", size_bytes}}})}
		});
		size_arg = {
			{"expr-type", "id"}, {"name", sz_name},
			{"var-type", uint64_type}, {"value-type", uint64_type}
		};
	} else {
		size_arg = size_bytes;
	}

	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt["vars"]) {
		string name = var["name"];
		json malloc_call = {
			{"expr-type", "call"}, {"name", "malloc"}, {"func-type", "c"},
			{"args", json::array({size_arg})}, {"value-type", pntr_type}
		};
		declareVar(name, pntr_type, &stmt);
		arrayScopeVars_.back().push_back({name, makeFreeStmt(name, pntr_type)});
		sa_stmt["vars"].push_back({{"name",name},{"var-type",pntr_type},{"init",malloc_call}});
	}
	result.push_back(sa_stmt);
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

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
}

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
}

json PlnSemanticAnalyzer::sa_continue_stmt(const json& stmt)
{
	if (loopDepth_ == 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ContinueOutsideLoop) << endl;
		exit(1);
	}
	return {{"stmt-type", "continue"}};
}

void PlnSemanticAnalyzer::sa_function(const json& funcDef)
{
	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes      = varScopes;
	auto savedCurrentFunc    = currentFunc_;
	auto savedArrayScopeVars = arrayScopeVars_;
	auto savedFuncBodyIdx    = funcBodyScopeIdx_;

	varScopes       = {{}};
	arrayScopeVars_ = {{}};  // scope[0] = params; no arrays expected here

	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			declareVar(p["name"], unsizedArrToPntr(p["var-type"]), &funcDef);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
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
		if (!funcEntry.contains("ret-type") && funcEntry.contains("rets") && funcEntry["rets"].size() == 1)
			funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"];
		registerPlnFunc(funcEntry["name"], funcEntry, &f);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = funcDef;
	normalizeUnsizedArrSig(saFunc);
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
	const string& fname = callExpr["name"].get<string>();

	// Resolve function — fall back to not-impl if not a known multi-return Palan func
	const json* pFunc = findPlnFunc(fname);
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
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ImportFileNotFound, imp_path.string()) << endl;
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

