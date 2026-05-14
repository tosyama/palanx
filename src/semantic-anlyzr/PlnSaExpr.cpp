/// Palan Semantic Analyzer — expression analysis
///
/// @file PlnSaExpr.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

json PlnSemanticAnalyzer::sa_expression(const json &expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	if (expr_type == "lit-int") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim)
			sa_expr["value-type"] = registry_.toJson(expectedType);
		else
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Int64));

	} else if (expr_type == "lit-uint") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim) {
			auto pn = static_cast<const PrimType*>(expectedType)->name;
			if (pn == PrimType::Name::Uint8  || pn == PrimType::Name::Uint16 ||
			    pn == PrimType::Name::Uint32 || pn == PrimType::Name::Uint64)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		} else {
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		}

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

	} else if (expr_type == "logical-and" || expr_type == "logical-or") {
		json left  = sa_expression(expr["left"]);
		json right = sa_expression(expr["right"]);
		for (const json* op : {&left, &right}) {
			const PlnType* t = registry_.fromJson((*op)["value-type"]);
			bool isFloat = t->kind == PlnType::Kind::Prim &&
				(static_cast<const PrimType*>(t)->name == PrimType::Name::Float32 ||
				 static_cast<const PrimType*>(t)->name == PrimType::Name::Float64);
			if (t->kind != PlnType::Kind::Prim || isFloat) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
				exit(1);
			}
		}
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "int32"}};

	} else if (expr_type == "logical-not") {
		json operand = sa_expression(expr["operand"]);
		const PlnType* t = registry_.fromJson(operand["value-type"]);
		bool isFloat = t->kind == PlnType::Kind::Prim &&
			(static_cast<const PrimType*>(t)->name == PrimType::Name::Float32 ||
			 static_cast<const PrimType*>(t)->name == PrimType::Name::Float64);
		if (t->kind != PlnType::Kind::Prim || isFloat) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
			exit(1);
		}
		sa_expr["operand"]    = operand;
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

	} else if (expr_type == "member-call") {
		return sa_expr_member_call(expr);

	} else if (expr_type == "field-access") {
		string varName   = expr["object"]["name"].get<string>();
		string fieldName = expr["field"].get<string>();
		const json* varType = findVar(varName);
		if (varType == nullptr) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedVariable, varName) << endl;
			exit(1);
		}
		if (varType->value("type-kind","") != "pntr"
				|| (*varType)["base-type"].value("type-kind","") != "struct") {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		string structName = (*varType)["base-type"]["type-name"].get<string>();
		const StructDef& def = structDefs_[structName];
		auto it = find_if(def.fields.begin(), def.fields.end(),
		                  [&](const FieldLayout& f){ return f.name == fieldName; });
		if (it == def.fields.end()) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownField, structName, fieldName) << endl;
			exit(1);
		}
		// LCOV_EXCL_EXCEPTION_BR_START
		return {{"expr-type","field-access"},
		        {"var", varName},
		        {"offset", it->offset},
		        {"value-type",{{"type-kind","prim"},{"type-name",it->typeName}}}};
		// LCOV_EXCL_EXCEPTION_BR_STOP

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
		const string& callName = expr["name"].get<string>();
		const json* pFunc = findPlnFunc(callName);
		if (pFunc == nullptr) {
			pFunc = findImportFunc(callName);
			if (pFunc != nullptr && pFunc->empty()) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AmbiguousCall, callName) << endl;
				exit(1);
			}
		}
		if (pFunc != nullptr) {
			sa_expr["func-type"] = "palan";
			if (pFunc->contains("ret-type"))
				sa_expr["value-type"] = (*pFunc)["ret-type"];
			if (pFunc->contains("parameters"))
				funcParams = &(*pFunc)["parameters"];
		} else {
			for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
				for (auto& [als, bucket] : *it)
					if (als != "" && bucket.count(callName)) {
						cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnqualifiedAliasCall, callName) << endl;
						exit(1);
					}
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedFunction, callName) << endl;
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
					const json& paramVT = (*funcParams)[argIdx]["var-type"];
					if (paramVT.value("embedded", false)) {
						const json& argVT = saArg["value-type"];
						bool argEmbedded = argVT.value("embedded", false);
						bool paramHasSize = paramVT.contains("inner-size");
						bool argHasSize   = argVT.contains("inner-size");
						if (!argEmbedded || !argHasSize || !paramHasSize
							|| paramVT["inner-size"] != argVT["inner-size"]) {
							string expected = paramHasSize
								? to_string(paramVT["inner-size"].get<int64_t>()) : "?";
							string actual = (argEmbedded && argHasSize)
								? to_string(argVT["inner-size"].get<int64_t>()) : "variable";
							cerr << locPrefix(expr)
							     << PlnSaMessage::getMessage(E_EmbeddedArrInnerSizeMismatch,
							            expected, actual) << endl;
							exit(1);
						}
					}
					const PlnType* toType = nullptr;
					try { toType = registry_.fromJson(paramVT); }
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

	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};

	if (array_type.value("embedded", false)) {
		// Embedded 2D array row access: mat[i] → pntr(T), non-owning
		// elem-size = stride = inner-size * sizeof(T)  (or __name_d1 * sizeof(T) if variable)
		int elem_sz = elemSizeBytes(elem_type.value("type-name",""));
		json row_pntr = {{"type-kind","pntr"},{"base-type",elem_type}};
		json elem_size_node;

		if (array_type.contains("inner-size")) {
			int64_t stride = array_type["inner-size"].get<int64_t>() * elem_sz;
			elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			};
		} else {
			string arr_name = sa_array["name"].get<string>();
			json d1_id = {{"expr-type","id"},{"name","__"+arr_name+"_d1"},
			              {"var-type",uint64_type},{"value-type",uint64_type}};
			json sz_lit = {
				{"expr-type","lit-uint"},{"value",to_string(elem_sz)},{"value-type",uint64_type}
			};
			elem_size_node = {
				{"expr-type","mul"},{"value-type",uint64_type},
				{"left",d1_id},{"right",sz_lit}
			};
		}
		sa_expr["array"]      = sa_array;
		sa_expr["index"]      = sa_index;
		sa_expr["elem-size"]  = elem_size_node;
		sa_expr["value-type"] = row_pntr;
		return sa_expr;
	}

	int sz = (elem_type.value("type-kind","") == "pntr") ? 8
		: elemSizeBytes(elem_type.value("type-name",""));
	json elem_size_node = {
		{"expr-type", "lit-uint"}, {"value", to_string(sz)}, {"value-type", uint64_type}
	};

	sa_expr["array"]      = sa_array;
	sa_expr["index"]      = sa_index;
	sa_expr["elem-size"]  = elem_size_node;
	sa_expr["value-type"] = elem_type;
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_member_call(const json& expr)
{
	const string& alias = expr["object"]["name"].get<string>();
	const string& method = expr["method"].get<string>();

	bool aliasFound = false;
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		if (it->count(alias)) { aliasFound = true; break; }
	if (!aliasFound) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownAlias, alias) << endl;
		exit(1);
	}

	const json* pFunc = findImportFuncByAlias(alias, method);
	if (pFunc == nullptr) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedFunction, method) << endl;
		exit(1);
	}

	bool isCFunc = pFunc->value("_c-func", false);

	json sa_expr = expr;
	sa_expr["expr-type"] = "call";
	sa_expr["name"] = method;
	sa_expr.erase("object");
	sa_expr.erase("method");
	sa_expr["func-type"] = isCFunc ? "c" : "palan";

	if (isCFunc) {
		if (pFunc->contains("ret-type")
				&& (*pFunc)["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = (*pFunc)["ret-type"];
	} else {
		if (pFunc->contains("ret-type"))
			sa_expr["value-type"] = (*pFunc)["ret-type"];
	}

	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";

	const json* funcParams = pFunc->contains("parameters") ? &(*pFunc)["parameters"] : nullptr;
	bool isVariadic = false;
	if (isCFunc && funcParams)
		for (auto& p : *funcParams)
			if (p.value("name", "") == "...") { isVariadic = true; break; }

	if (expr.contains("args")) {
		sa_expr["args"] = json::array();
		size_t fixedCount = funcParams ? (funcParams->size() - (isVariadic ? 1 : 0)) : 0;
		size_t argIdx = 0;
		for (auto& arg : expr["args"]) {
			json saArg = sa_expression(arg);
			if (saArg.contains("value-type") && funcParams && argIdx < fixedCount) {
				const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
				const PlnType* toType = nullptr;
				try { toType = registry_.fromJson((*funcParams)[argIdx]["var-type"]); }
				catch (const std::runtime_error&) {}
				if (toType && typeCompat(fromType, toType, registry_) == TypeCompat::ImplicitWiden)
					saArg = wrapConvert(saArg, registry_.toJson(toType));
			}
			sa_expr["args"].push_back(saArg);
			argIdx++;
		}
	}
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE
