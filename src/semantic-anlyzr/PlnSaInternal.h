/// Inline helpers shared across PlnSemanticAnalyzer implementation files.
///
/// @file PlnSaInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"
// FieldLayout/StructDef live in PlnSemanticAnalyzer.h, not here; every SA .cpp
// already includes it before this header, but classifySysVStructRet below
// needs it too, and a standalone consumer (sa-unit-tester) would otherwise
// have to know that ordering by convention rather than by include graph.
#include "PlnSemanticAnalyzer.h"

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

// True if `t` is a non-float primitive type (int8..uint64). Shared by every
// operator that requires integer operands (logical &&/||/!, bitwise &/|/^/~)
// so the "not a float, and not a pointer/struct" check has one definition.
inline bool isIntegerPrim(const PlnType* t) {
	if (t->kind != PlnType::Kind::Prim) return false;
	auto pn = static_cast<const PrimType*>(t)->name;
	return pn != PrimType::Name::Float32 && pn != PrimType::Name::Float64
	    && pn != PrimType::Name::Void;
}

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

// SysV AMD64 ABI (§3.2.3) eightbyte classes for a struct return value.
enum class EightbyteClass { Integer, Sse };
struct EightbyteRet { EightbyteClass cls; int size; };  // size in {1,2,4,8}; only the last eightbyte may be <8

// Recursively marks the eightbytes covered by one field as INTEGER; SSE is
// `cls`'s initial value, so an all-float field simply leaves it unmarked
// (mixing within one eightbyte follows the "any INTEGER wins" merge rule).
// `base` is the field's struct-relative offset already shifted by every
// enclosing embed/embed-arr, so recursion always marks against the same
// top-level `cls` vector.
inline void classifySysVWalk(const StructDef& d, int base, int nEightbytes,
                              const map<string, StructDef>& defs,
                              vector<EightbyteClass>& cls)
{
	auto mark = [&](int offset, int size, EightbyteClass fieldCls) {
		if (fieldCls != EightbyteClass::Integer) return;  // Sse is the default; nothing to mark
		for (int b = offset; b < offset + size; b++) {
			int eb = b / 8;
			if (eb < nEightbytes) cls[eb] = EightbyteClass::Integer;
		}
	};
	for (auto& f : d.fields) {
		int off = base + f.offset;
		if (f.typeKind == "prim") {
			bool isFloat = (f.typeName == "flo32" || f.typeName == "flo64");
			mark(off, f.size, isFloat ? EightbyteClass::Sse : EightbyteClass::Integer);
		} else if (f.typeKind == "raw-ptr" || f.typeKind == "struct-ptr" || f.typeKind == "arr-ptr") {
			mark(off, 8, EightbyteClass::Integer);
		} else if (f.typeKind == "embed-ptr-arr") {
			for (int64_t i = 0; i < f.count; i++)
				mark(off + (int)(i * 8), 8, EightbyteClass::Integer);
		} else if (f.typeKind == "embed") {
			classifySysVWalk(defs.at(f.typeName), off, nEightbytes, defs, cls);
		} else if (f.typeKind == "embed-arr") {
			bool leafFloat = (f.typeName == "flo32" || f.typeName == "flo64");
			for (int64_t i = 0; i < f.count; i++) {
				int elemOff = off + (int)(i * f.stride);
				if (f.elemKind == "struct")
					classifySysVWalk(defs.at(f.typeName), elemOff, nEightbytes, defs, cls);
				else
					mark(elemOff, f.stride, leafFloat ? EightbyteClass::Sse : EightbyteClass::Integer);
			}
		}
	}
}

// Classifies a struct's return value per the System V AMD64 ABI (§3.2.3):
// a struct over 16 bytes is MEMORY class, returned via a caller-supplied
// hidden pointer, represented here as an empty vector; 16 bytes or less is
// returned in up to two eightbytes, each INTEGER (%rax/%rdx) or SSE
// (%xmm0/%xmm1). Only the return-value rules are implemented (not full
// argument classification, which additionally has a MEMORY class for
// eightbytes containing unaligned fields -- unreached by every struct
// this compiler can construct, since buildStructDef itself enforces
// natural alignment).
//
// Sets `ok` to false, and returns an empty vector, if any eightbyte's
// natural width isn't 1/2/4/8 bytes (e.g. a trailing `char a[3]` field) --
// callers should reject that shape with a dedicated diagnostic rather than
// guess at a partial-eightbyte store.
inline vector<EightbyteRet> classifySysVStructRet(const StructDef& def,
                                                   const map<string, StructDef>& defs,
                                                   bool& ok)
{
	ok = true;
	if (def.totalSize > 16) return {};  // MEMORY class

	int nEightbytes = (def.totalSize + 7) / 8;
	int tailWidth = def.totalSize - 8 * (nEightbytes - 1);
	if (tailWidth != 1 && tailWidth != 2 && tailWidth != 4 && tailWidth != 8) {
		ok = false;
		return {};
	}

	vector<EightbyteClass> cls(nEightbytes, EightbyteClass::Sse);
	classifySysVWalk(def, 0, nEightbytes, defs, cls);

	vector<EightbyteRet> result;
	for (int i = 0; i < nEightbytes; i++)
		result.push_back({cls[i], (i == nEightbytes - 1) ? tailWidth : 8});
	return result;
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

inline json normalizeCType(const json& type);

// c2ast tags a C struct reference "strct" (a syntactic fact -- it saw the
// `struct` keyword -- available with no linking/resolution performed, since
// c2ast never sees structDefs_). SA's own canonical post-resolution tag for
// the same nominal type is "struct" (see PlnSemanticAnalyzer::normalizeStructSig,
// used for native Palan function signatures). Native functions get rewritten
// at registration time; this is the same rewrite for cinclude'd C function
// signatures, so PlnTypeRegistry::fromJson only ever needs to understand the
// single canonical "struct" tag, never c2ast's raw "strct" one.
//
// Also the single point where a C pointer's write permission is decided:
// c2ast records const-ness as a syntactic fact on the *pointee* ("const" on
// base-type), the same vocabulary a Palan `@T`/`@!T` pointer never uses --
// SA's own pointer permission vocabulary is "mutable" on the `pntr` node
// itself (see ptrPermissionOk/isWritableThrough above). Folding const into
// mutable here means every consumer of a `pntr` value-type (ptrPermissionOk,
// codegen) only ever needs to understand "mutable", whether the pointer came
// from Palan syntax or a cincluded C signature. This recurses into a `func`
// type-kind's own ret-type/parameters too, so a callback parameter's inner
// pointers (e.g. qsort's `int (*)(const void*, const void*)`) get "mutable"
// the same as any other pointer -- without this, isWritableThrough's
// absent-key default (writable) would silently invert the callback's
// pointer permissions.
inline json normalizeCType(const json& type) {
	if (type.value("type-kind","") == "pntr") {
		json t = type;
		json base = normalizeCType(type["base-type"]);
		t["mutable"] = !base.value("const", false);
		t["base-type"] = move(base);
		return t;
	}
	if (type.value("type-kind","") == "strct") {
		json t = type;
		t["type-kind"] = "struct";
		return t;
	}
	if (type.value("type-kind","") == "func") {
		json t = type;
		t["ret-type"] = normalizeCType(type["ret-type"]);
		if (t.contains("parameters"))
			for (auto& p : t["parameters"])
				if (p.contains("var-type"))
					p["var-type"] = normalizeCType(p["var-type"]);
		return t;
	}
	return type;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// C function entries (from c2ast) always carry a single "ret-type", never
// Palan's native multi/named-return "rets" list, so there's no "rets" case
// to handle here unlike normalizeUnsizedArrSig above.
//
// Also the single point where a C function's signature is checked for a type
// PlnTypeRegistry::fromJson cannot represent (a variadic "..." parameter
// entry has no "var-type" and is skipped, same as every other pass here).
// Unlike a native Palan signature (see validateNativeSig in
// PlnSemanticAnalyzer.cpp), an unresolved C "prim" type-name (e.g. "flt128"
// for `long double`) IS genuinely unrepresentable here, not a forward
// reference to be resolved later -- c2ast's typedef registration already
// resolves anything resolvable before a reference site is emitted. A hit is
// recorded on the entry as "_unsupported-sig" (checked at call time by
// requireSupportedCFuncSig) rather than rejected here, so cinclude'ing a
// header that happens to declare an unsupported function is not itself an
// error -- only calling it is.
//
// A top-level by-value struct parameter is also flagged here: unlike a
// pntr-wrapped struct base-type, unrepresentableTypeName alone cannot tell
// "by value" from "behind a pointer" apart (both are a bare
// {"type-kind":"struct",...} node once normalizeCType has stripped the
// wrapping pntr, or none was ever there), and only this function sees a
// parameter's position in the signature. By-value struct return is exempt --
// it stays representable here and is classified by classifySysVStructRet at
// the call site (IT-2026-09-12-3004) instead of rejected.
inline void normalizeCFuncSig(json& funcDef) {
	string bad;
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			if (p.contains("var-type")) {
				p["var-type"] = normalizeCType(p["var-type"]);
				if (bad.empty()) {
					if (p["var-type"].value("type-kind", "") == "struct")
						bad = "by-value struct parameter";
					else
						bad = unrepresentableTypeName(p["var-type"]);
				}
			}
	if (funcDef.contains("ret-type")) {
		funcDef["ret-type"] = normalizeCType(funcDef["ret-type"]);
		if (bad.empty()) bad = unrepresentableTypeName(funcDef["ret-type"]);
	}
	if (!bad.empty()) funcDef["_unsupported-sig"] = bad;
}

// Symmetric counterpart to normalizeCFuncSig for a single cinclude'd C global
// object declaration (see the "Global variable model" in ASTSpec.md). Same
// deferral policy: an unrepresentable type is recorded as "_unsupported-global"
// rather than rejected here, so cinclude'ing a header with one unsupported
// global is not itself an error -- only referencing that global is (checked
// at reference time by requireSupportedCGlobal).
inline void normalizeCGlobal(json& g) {
	g["var-type"] = normalizeCType(g["var-type"]);
	string bad = unrepresentableTypeName(g["var-type"]);
	if (!bad.empty()) g["_unsupported-global"] = bad;
}
