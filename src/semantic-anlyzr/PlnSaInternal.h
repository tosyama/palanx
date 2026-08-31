/// Inline helpers shared across PlnSemanticAnalyzer implementation files.
///
/// @file PlnSaInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"

using json = nlohmann::json;
using namespace std;

// LCOV_EXCL_EXCEPTION_BR_START
inline json wrapConvert(const json& expr, const json& to_type) {
	return {
		{"expr-type",  "convert"},
		{"value-type", to_type},
		{"from-type",  expr["value-type"]},
		{"src",        expr}
	};
}
// LCOV_EXCL_EXCEPTION_BR_STOP

// True if a value may be written through this pointer-typed value-type
// (i.e. it is not a `@T` read-only pointer). A missing "mutable" key means
// writable: it is the default for every pntr value-type SA synthesizes for
// struct/array variables, which carry no read-only concept of their own.
inline bool isWritableThrough(const json& vt) { return vt.value("mutable", true); }

// True unless binding `from` to `to` would upgrade a read-only pointer to a
// mutable one (assigning `@T` to a `@!T`-typed destination). Non-pointer
// types, and narrowing a mutable pointer to read-only, are always fine.
inline bool ptrPermissionOk(const json& from, const json& to)
{
	if (from.value("type-kind","") != "pntr" || to.value("type-kind","") != "pntr")
		return true;
	return isWritableThrough(from) || !isWritableThrough(to);
}

// LCOV_EXCL_EXCEPTION_BR_START
inline json fieldValueType(const FieldLayout& f)
{
	if (f.typeKind == "prim")
		return {{"type-kind","prim"},{"type-name",f.typeName}};
	if (f.typeKind == "embed-arr") {
		json bt = (f.elemKind == "struct")
			? json{{"type-kind","struct"},{"type-name",f.typeName}}
			: json{{"type-kind","prim"},{"type-name",f.typeName}};
		return {{"type-kind","pntr"},{"embedded",true},{"stride",f.stride},{"base-type",bt}};
	}
	if (f.typeKind == "embed-ptr-arr") {
		json bt = (f.elemKind == "struct")
			? json{{"type-kind","struct"},{"type-name",f.typeName}}
			: json{{"type-kind","prim"},{"type-name",f.typeName}};
		json elem_pntr = {{"type-kind","pntr"},{"base-type",bt},{"mutable",f.isMutable}};
		return {{"type-kind","pntr"},{"base-type",elem_pntr}};
	}
	if (f.typeKind == "arr-ptr") {
		// Primitive leaf: field is a plain pointer to inline malloc'd values
		// (pntr(prim)); pts[i] loads the value directly.
		// Struct leaf: field cascades to __pln_alloc_arr_T, matching the
		// variable-level owned struct array shape (sa_owned_struct_arr_var_decl) —
		// a pointer to an array of struct-pointers (pntr(pntr(struct))); pts[i]
		// loads the i-th element's pointer.
		if (f.elemKind == "struct") {
			json struct_type = {{"type-kind","struct"},{"type-name",f.typeName}};
			json elem_pntr   = {{"type-kind","pntr"},{"base-type",struct_type}};
			return {{"type-kind","pntr"},{"base-type",elem_pntr}};
		}
		json bt = {{"type-kind","prim"},{"type-name",f.typeName}};
		return {{"type-kind","pntr"},{"base-type",bt}};
	}
	json bt = {{"type-kind","struct"},{"type-name",f.typeName}};
	json pntr = {{"type-kind","pntr"},{"base-type",bt}};
	if (f.typeKind == "raw-ptr")
		pntr["mutable"] = f.isMutable;
	return pntr;
}
// LCOV_EXCL_EXCEPTION_BR_STOP

// Returns element byte size for a primitive type name; -1 if unknown
inline int elemSizeBytes(const string& typeName)
{
	if (typeName == "int8"  || typeName == "uint8")  return 1;
	if (typeName == "int16" || typeName == "uint16") return 2;
	if (typeName == "int32" || typeName == "uint32" || typeName == "flo32") return 4;
	if (typeName == "int64" || typeName == "uint64" || typeName == "flo64") return 8;
	return -1;
}

// Builds a synthetic free() expression statement for the named pointer variable
// LCOV_EXCL_EXCEPTION_BR_START
inline json makeFreeStmt(const string& name, const json& pntrType)
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

// Builds a synthetic pln-function free call for the named pointer variable
// LCOV_EXCL_EXCEPTION_BR_START
inline json makePlanFreeStmt(const string& varName, const json& varType,
                              const string& freeFn)
{
	json var_id = {{"expr-type","id"},{"name",varName},
	               {"var-type",varType},{"value-type",varType}};
	return {
		{"stmt-type", "expr"},
		{"body", {
			{"expr-type","call"},{"name",freeFn},{"func-type","pln"},
			{"args", json::array({var_id})}
		}}
	};
}
// LCOV_EXCL_EXCEPTION_BR_STOP

inline const PlnType* variadicPromote(const PlnType* t, PlnTypeRegistry& reg)
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

inline json unsizedArrToPntr(const json& type);

inline json unsizedArrToPntr(const json& type) {
	if (type.value("type-kind","") == "arr"
		&& type.value("specifier","") == "raw"
		&& type["size-expr"].is_null()) {
		// []$[m]T parameter form: arr with embed base-type (from gen-ast)
		if (type["base-type"].value("type-kind","") == "embed") {
			// base-type is {type-kind:"embed", base-type: {arr, size-expr:m, base-type:T}}
			const auto& embed_bt = type["base-type"]["base-type"];  // [m]T part
			json pntr = {{"type-kind","pntr"},{"embedded",true}};
			if (embed_bt.value("type-kind","") == "arr" && !embed_bt["size-expr"].is_null()) {
				const auto& sz = embed_bt["size-expr"];
				string et = sz.value("expr-type","");
				if (et == "lit-int" || et == "lit-uint")
					pntr["inner-size"] = stoll(sz["value"].get<string>());
				// Variable inner-size: no inner-size field; validateEmbeddedParams catches it
			}
			// []$[]T or []$[var]T: no inner-size → validateEmbeddedParams reports error
			pntr["base-type"] = embed_bt.value("base-type", json{});
			return pntr;
		}
		return {{"type-kind","pntr"},{"base-type", unsizedArrToPntr(type["base-type"])}};
	}
	return type;
} // LCOV_EXCL_EXCEPTION_BR_LINE

inline void normalizeUnsizedArrSig(json& funcDef) {
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

inline json normalizeCStructKind(const json& type);

// c2ast tags a C struct reference "strct" (a syntactic fact -- it saw the
// `struct` keyword -- available with no linking/resolution performed, since
// c2ast never sees structDefs_). SA's own canonical post-resolution tag for
// the same nominal type is "struct" (see PlnSemanticAnalyzer::normalizeStructSig,
// used for native Palan function signatures). Native functions get rewritten
// at registration time; this is the same rewrite for cinclude'd C function
// signatures, so PlnTypeRegistry::fromJson only ever needs to understand the
// single canonical "struct" tag, never c2ast's raw "strct" one.
inline json normalizeCStructKind(const json& type) {
	if (type.value("type-kind","") == "pntr") {
		json t = type;
		t["base-type"] = normalizeCStructKind(type["base-type"]);
		return t;
	}
	if (type.value("type-kind","") == "strct") {
		json t = type;
		t["type-kind"] = "struct";
		return t;
	}
	return type;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// C function entries (from c2ast) always carry a single "ret-type", never
// Palan's native multi/named-return "rets" list, so there's no "rets" case
// to handle here unlike normalizeUnsizedArrSig above.
inline void normalizeCFuncStructSig(json& funcDef) {
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			if (p.contains("var-type"))
				p["var-type"] = normalizeCStructKind(p["var-type"]);
	if (funcDef.contains("ret-type"))
		funcDef["ret-type"] = normalizeCStructKind(funcDef["ret-type"]);
}
