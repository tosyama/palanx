/// Palan Semantic Analyzer — expression analysis
///
/// @file PlnSaExpr.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <charconv>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

FieldChain PlnSemanticAnalyzer::resolveObjectChain(const json& obj, bool forWrite)
{
	if (obj.value("expr-type","") == "id") {
		string varName = obj["name"].get<string>();
		const json* vt = findVar(varName);
		if (!vt) {
			if (findCGlobal(varName) != nullptr) {
				cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_CGlobalNotAddressable, varName) << endl;
				exit(1);
			}
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_UndefinedVariable, varName) << endl;
			exit(1);
		}
		if (!isStructPntr(*vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(*vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		return {false, varName, 0, {}, (*vt)["base-type"]["type-name"].get<string>()};
	}
	if (obj.value("expr-type","") == "arr-index") {
		json sa_idx = sa_expr_arr_index(obj, forWrite);
		const json& vt = sa_idx["value-type"];
		if (vt.value("type-kind","") != "pntr" || vt["base-type"].value("type-kind","") != "struct") {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		// A pointer element carries its own permission; a struct stored in the
		// array is writable only as far as the array it is reached through.
		if (forWrite && isStructStorage(vt) && !isWritableThrough(sa_idx["array"]["value-type"])) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteToReadOnlyArrElem) << endl;
			exit(1);
		}
		string struct_name = vt["base-type"]["type-name"].get<string>();
		return {true, "", 0, move(sa_idx), struct_name};
	}
	if (obj.value("expr-type","") != "field-access") {
		// Reachable via a chain rooted in a non-addressable base: a call/tuple
		// grouping normalizes to "not-impl" (see storeLocToExpr and term's
		// '(' tapple_inner ')' rule), which has neither "object" nor "field".
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	FieldChain base = resolveObjectChain(obj["object"], forWrite);
	string fn = obj["field"].get<string>();
	const StructDef& def = requireCompleteStruct(base.structName, obj);
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
	if (it == def.fields.end()) {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_UnknownField, base.structName, fn, def.keyword()) << endl;
		exit(1);
	}
	if (it->typeKind == "prim") {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	if (forWrite && it->typeKind == "raw-ptr" && !it->isMutable) {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteToImmutablePtrField) << endl;
		exit(1);
	}
	if (it->typeKind == "raw-ptr" && it->elemKind == "prim") {
		// A pointer-to-primitive field (e.g. `@!int64 p;`) has nothing further to
		// chain into -- this hop must be the chain's terminal, typed by the
		// caller via fieldValueType, not resolveObjectChain's own pntr(struct(...)).
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
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

const FieldLayout& PlnSemanticAnalyzer::findFieldOrExit(const string& structName, const string& fieldName, const json& locNode)
{
	const StructDef& def = requireCompleteStruct(structName, locNode);
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fieldName; });
	if (it == def.fields.end()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownField, structName, fieldName, def.keyword()) << endl;
		exit(1);
	}
	return *it;
}

// `name` must already be a key in structDefs_ (checked by the caller, e.g. via
// isKnownTypeName/count(), or because it's a chain hop whose base type was
// normalized by a producer that already required it -- see resolveObjectChain).
// This only distinguishes "layout not yet known" (an incomplete/opaque struct,
// e.g. C's FILE) from "fully laid out": callers needing the former distinguished
// from "no such struct" must check that separately before calling this.
const StructDef& PlnSemanticAnalyzer::requireCompleteStruct(const string& structName, const json& locNode)
{
	const StructDef& def = structDefs_[structName];
	if (!def.isComplete) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompleteStructType, structName,
		                                                        def.incompleteReason, def.keyword()) << endl;
		exit(1);
	}
	return def;
}

void PlnSemanticAnalyzer::requireSupportedCFuncSig(const json& entry, const string& funcName, const json& locNode)
{
	auto it = entry.find("_unsupported-sig");
	if (it == entry.end()) return;
	cerr << locPrefix(locNode)
	     << PlnSaMessage::getMessage(E_UnsupportedCFuncSignature, funcName, it->get<string>()) << endl;
	exit(1);
}

void PlnSemanticAnalyzer::requireSupportedCGlobal(const json& entry, const string& globalName, const json& locNode)
{
	auto it = entry.find("_unsupported-global");
	if (it == entry.end()) return;
	cerr << locPrefix(locNode)
	     << PlnSaMessage::getMessage(E_UnsupportedCGlobalType, globalName, it->get<string>()) << endl;
	exit(1);
}

json PlnSemanticAnalyzer::sa_expr_addr_of(const json& expr)
{
	bool isMutable = expr.value("mutable", false);
	const json& obj = expr["object"];
	string obj_type = obj.value("expr-type", "");

	if (obj_type == "id") {
		string name = obj["name"].get<string>();
		const json* varType = findVar(name);
		if (varType == nullptr) {
			if (findCGlobal(name) != nullptr) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CGlobalNotAddressable, name) << endl;
				exit(1);
			}
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedVariable, name) << endl;
			exit(1);
		}
		// '@' on a borrow would name its variable's slot; only a primitive
		// pointer has a use for that (a C 'T **' out-param).
		if (isArrBorrowVar(*varType) || isStructBorrow(*varType)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfBorrowed, name) << endl;
			exit(1);
		}
		// A struct or array variable is already a pointer to its storage, so '@'
		// borrows it as it is rather than taking the address of the variable's slot.
		if (isStructStorage(*varType)) {
			json vt = *varType;
			vt["mutable"] = isMutable;
			json out = {{"expr-type","id"},{"name",name},{"var-type",*varType},{"value-type",vt}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			if (expr.contains("loc")) out["loc"] = expr["loc"];
			return out;
		}
		if (isInArrayScope(name) || varType->contains("arr-size")) {
			json out = {{"expr-type","id"},{"name",name},{"var-type",*varType},
			            {"value-type",withArrPermission(*varType, isMutable)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			if (expr.contains("loc")) out["loc"] = expr["loc"];
			return out;
		}
		if (!isLocalVar(name)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotLocalVar, name) << endl;
			exit(1);
		}
		string vtk = varType->value("type-kind", "");
		bool isPtrToPrim = vtk == "pntr" && (*varType)["base-type"].value("type-kind","") == "prim";
		if (vtk != "prim" && !isPtrToPrim) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitive, name) << endl;
			exit(1);
		}
		json pntr_type = {{"type-kind", "pntr"}, {"mutable", isMutable}, {"base-type", *varType}};
		json out = {{"expr-type", "addr-of"}, {"name", name}, {"mutable", isMutable}, {"value-type", pntr_type}};
		if (expr.contains("loc")) out["loc"] = expr["loc"];
		return out;
	}

	if (obj_type == "field-access") {
		FieldChain chain = resolveObjectChain(obj["object"], /*forWrite=*/isMutable);
		string fn = obj["field"].get<string>();
		const FieldLayout& fld = findFieldOrExit(chain.structName, fn, obj);
		if (fld.typeKind != "prim" && fld.typeKind != "embed" && fld.typeKind != "struct-ptr") {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitive, fn) << endl;
			exit(1);
		}
		// An embed field's own value-type (fieldValueType) is already
		// pntr(struct T) -- the field IS the struct's storage, same as a
		// struct-typed local variable -- so wrapping it again here would
		// produce pntr(pntr(struct T)), a double indirection nothing needs.
		// An owned struct field holds the pointer to its struct, so '@' loads
		// that pointer rather than computing the field's address.
		// A prim field's value-type is the bare prim, so it still needs the
		// usual pntr(...) wrap to become "address of this prim slot".
		bool addrOnly = fld.typeKind != "struct-ptr";
		json pntr_type = (fld.typeKind == "prim")
			? json{{"type-kind","pntr"},{"base-type",fieldValueType(fld)}}
			: fieldValueType(fld);
		pntr_type["mutable"] = isMutable;
		int off = chain.offset + fld.offset;
		json out = chain.isPointerBased
			? json{{"expr-type","field-access"},{"ptr-expr",chain.ptrExpr},{"offset",off},{"value-type",pntr_type},{"addr-only",addrOnly}}
			: json{{"expr-type","field-access"},{"var",chain.varName},{"offset",off},{"value-type",pntr_type},{"addr-only",addrOnly}};
		if (expr.contains("loc")) out["loc"] = expr["loc"];
		return out;
	}

	if (obj_type == "arr-index") {
		json sa_idx = sa_expr_arr_index(obj, isMutable);
		// A struct element or a row is already a pointer to its storage, so '@'
		// only marks it as a borrow, as for a whole array variable. Any other
		// element that is already an address computation or a pointer
		// (pointer-slot element) is rejected -- '@' keeps a single meaning
		// ("make a pointer to a storage slot"), and wrapping those would double
		// the indirection.
		bool isStructElem = isStructStorage(sa_idx["value-type"]);
		bool isRow = isArrLevel(sa_idx["value-type"]);
		if (!isStructElem && !isRow && (sa_idx.value("addr-only", false)
		                      || sa_idx["value-type"].value("type-kind","") != "prim")) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitiveElem) << endl;
			exit(1);
		}
		if (isMutable && !isWritableThrough(sa_idx["array"]["value-type"])) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		if (isStructElem) {
			sa_idx["value-type"]["mutable"] = isMutable;
		} else if (isRow) {
			sa_idx["value-type"] = withArrPermission(sa_idx["value-type"], isMutable);
		} else {
			sa_idx["addr-only"]  = true;
			sa_idx["value-type"] = {{"type-kind","pntr"},{"mutable",isMutable},{"base-type",sa_idx["value-type"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		}
		if (expr.contains("loc")) sa_idx["loc"] = expr["loc"];
		return sa_idx;
	}

	cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
	exit(1);
} // LCOV_EXCL_EXCEPTION_BR_LINE

static bool intLiteralFits(const string& value, PrimType::Name pn)
{
	bool neg = value[0] == '-';
	const char* first = value.data() + (neg ? 1 : 0);
	const char* last = value.data() + value.size();
	uint64_t mag;
	// The lexer only produces digit strings, so the only failure is overflow.
	if (from_chars(first, last, mag).ec != errc()) return false;

	int bits = 64;
	bool isSigned = true;
	switch (pn) {
		case PrimType::Name::Int8:   bits = 8;  break;
		case PrimType::Name::Int16:  bits = 16; break;
		case PrimType::Name::Int32:  bits = 32; break;
		case PrimType::Name::Int64:  bits = 64; break;
		case PrimType::Name::Uint8:  bits = 8;  isSigned = false; break;
		case PrimType::Name::Uint16: bits = 16; isSigned = false; break;
		case PrimType::Name::Uint32: bits = 32; isSigned = false; break;
		case PrimType::Name::Bool:   bits = 1;  isSigned = false; break;
		default:                     bits = 64; isSigned = false; break;
	}
	if (isSigned) {
		uint64_t posMax = (uint64_t(1) << (bits - 1)) - 1;
		return mag <= (neg ? posMax + 1 : posMax);
	}
	if (neg) return mag == 0;
	return bits == 64 || mag <= (uint64_t(1) << bits) - 1;
}

void PlnSemanticAnalyzer::checkIntLiteralRange(const json& lit)
{
	const PlnType* t = registry_.fromJson(lit["value-type"]);
	if (!isIntegerPrim(t)) return;
	string value = lit["value"];
	if (intLiteralFits(value, static_cast<const PrimType*>(t)->name)) return;
	cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_IntLiteralOutOfRange,
		value, typeDisplayName(lit["value-type"])) << endl;
	exit(1);
}

// C's integer promotion, applied to bool and enum only: an operator computes
// in at least int32 so a bool operand never yields a 1-byte result outside
// 0/1 (`b + 1` is 2), and an enum computes as its base type. Other narrow
// types keep their declared width.
// LCOV_EXCL_EXCEPTION_BR_START
static json promoteOperand(json operand)
{
	const json& vt = operand["value-type"];
	if (vt.value("type-kind", "") != "prim") return operand;
	if (vt.contains("enum")) {
		json base = vt;
		base.erase("enum");
		return wrapConvert(operand, base);
	}
	if (vt.value("type-name", "") != "bool") return operand;
	return wrapConvert(operand, {{"type-kind", "prim"}, {"type-name", "int32"}});
}
// LCOV_EXCL_EXCEPTION_BR_STOP

// A variable shadows a same-named const, so this can only be decided at
// the reference site, not by rewriting the AST up front.
json PlnSemanticAnalyzer::resolveConstRef(const json& expr) const
{
	if (expr.value("expr-type", "") != "id") return expr;
	string name = expr["name"].get<string>();
	if (findVar(name) != nullptr) return expr;
	auto cit = constDecls_.find(name);
	if (cit == constDecls_.end()) return expr;
	json lit = cit->second;
	if (expr.contains("loc")) lit["loc"] = expr["loc"];
	return lit;
}

json PlnSemanticAnalyzer::sa_expression(const json &rawExpr, const PlnType* expectedType)
{
	const json expr = resolveConstRef(rawExpr);
	json sa_expr = expr;
	string expr_type = expr["expr-type"];
	// A literal bound to an enum takes the base type, so the binding still
	// rejects it as needing Name(x) instead of silently labeling it.
	if (expectedType && expectedType->kind == PlnType::Kind::Enum)
		expectedType = static_cast<const EnumType*>(expectedType)->base;

	if (expr_type == "arr-lit") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArrLitContext) << endl;
		exit(1);
	}
	if (expr_type == "dict-lit") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
		exit(1);
	}

	if (expr_type == "lit-int") {
		// A lit-int already carrying a value-type is a macro constant folded
		// in by gen-ast (a plain source literal never has one) -- its type
		// was fixed by the C declaration, not by this expression's context,
		// so expectedType must not override it.
		if (expr.contains("value-type")) {
			resolveCEnumRefs(sa_expr["value-type"]);
		} else {
			if (expectedType && expectedType->kind == PlnType::Kind::Prim)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Int64));
			// A float constant has exactly one form, lit-flo, so codegen never
			// sees an integer literal that must be materialized as a float.
			if (isFloatPrim(expectedType))
				sa_expr["expr-type"] = "lit-flo";
			else
				checkIntLiteralRange(sa_expr);
		}

	} else if (expr_type == "lit-uint") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim) {
			auto pn = static_cast<const PrimType*>(expectedType)->name;
			if (pn == PrimType::Name::Uint8  || pn == PrimType::Name::Uint16 ||
			    pn == PrimType::Name::Uint32 || pn == PrimType::Name::Uint64 ||
			    pn == PrimType::Name::Bool)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		} else {
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		}
		checkIntLiteralRange(sa_expr);

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
			string name = expr["name"].get<string>();
			const json* cglobal = findCGlobal(name);
			if (cglobal != nullptr) {
				requireSupportedCGlobal(*cglobal, name, expr);
				sa_expr = {{"expr-type", "c-global"}, {"label", name}, {"value-type", (*cglobal)["var-type"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
				if (expr.contains("loc")) sa_expr["loc"] = expr["loc"];
			} else {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedVariable, expr["name"]) << endl;
				exit(1);
			}
		}

	} else if (expr_type == "addr-of") {
		return sa_expr_addr_of(expr);

	} else if (expr_type == "add" || expr_type == "sub"
	        || expr_type == "mul" || expr_type == "div" || expr_type == "mod"
	        || expr_type == "bitand" || expr_type == "bitor" || expr_type == "bitxor") {
		return sa_expr_arith(expr, expectedType);

	} else if (expr_type == "neg") {
		json operand = promoteOperand(sa_expression(expr["operand"], expectedType));
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = operand["value-type"];

	} else if (expr_type == "bitnot") {
		json operand = promoteOperand(sa_expression(expr["operand"], expectedType));
		const PlnType* t = registry_.fromJson(operand["value-type"]);
		if (!isIntegerPrim(t)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
			exit(1);
		}
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = operand["value-type"];

	} else if (expr_type == "cmp") {
		json left  = promoteOperand(sa_expression(expr["left"]));
		json right = promoteOperand(sa_expression(expr["right"]));
		const PlnType* leftType  = registry_.fromJson(left["value-type"]);
		const PlnType* rightType = registry_.fromJson(right["value-type"]);
		// A pointer/struct pair (usualArithConv returns nullptr) is left
		// unconverted -- pointer comparison (`p == NULL`, `p == q`) is a valid
		// use of `==`/`!=` that has no common numeric type to convert to.
		const PlnType* promoted = usualArithConv(leftType, rightType);
		if (promoted) {
			if (leftType != promoted)  left  = wrapConvert(left,  registry_.toJson(promoted));
			if (rightType != promoted) right = wrapConvert(right, registry_.toJson(promoted));
		}
		sa_expr["op"]         = expr["op"];
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_EXCEPTION_BR_LINE

	} else if (expr_type == "logical-and" || expr_type == "logical-or") {
		json left  = sa_expression(expr["left"]);
		json right = sa_expression(expr["right"]);
		for (const json* op : {&left, &right}) {
			const PlnType* t = registry_.fromJson((*op)["value-type"]);
			if (!isIntegerPrim(t)) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
				exit(1);
			}
		}
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_EXCEPTION_BR_LINE

	} else if (expr_type == "logical-not") {
		json operand = sa_expression(expr["operand"]);
		const PlnType* t = registry_.fromJson(operand["value-type"]);
		if (!isIntegerPrim(t)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
			exit(1);
		}
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_EXCEPTION_BR_LINE

	} else if (expr_type == "cast") {
		const PlnType* target  = registry_.fromJson(expr["target-type"]);
		json src               = sa_expression(expr["src"]);
		const PlnType* srcType = registry_.fromJson(src["value-type"]);
		TypeCompat compat      = typeCompat(srcType, target, registry_);
		if (compat == TypeCompat::Identical) {
			return src;
		} else if (compat == TypeCompat::ExplicitCast && isBoolPrim(target)) {
			// C's conversion to _Bool: any nonzero value is 1, not a truncation.
			// LCOV_EXCL_EXCEPTION_BR_START
			json zero = isFloatPrim(srcType)
				? json{{"expr-type", "lit-flo"}, {"value", "0.0"}, {"value-type", src["value-type"]}}
				: json{{"expr-type", "lit-int"}, {"value", "0"}, {"value-type", src["value-type"]}};
			return {{"expr-type", "cmp"}, {"op", "!="}, {"left", src}, {"right", zero},
			        {"value-type", expr["target-type"]}};
			// LCOV_EXCL_EXCEPTION_BR_STOP
		} else if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast) {
			return wrapConvert(src, registry_.toJson(target));
		} else {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_IncompatibleTypeCast,
				typeDisplayName(src["value-type"]),
				typeDisplayName(expr["target-type"])) << endl;
			exit(1);
		}

	} else if (expr_type == "call") {
		// gen-ast only knows the prim keywords as cast targets; an enum name
		// is a type only SA can see, so its cast arrives shaped as a call.
		const string& callName = expr["name"].get<string>();
		json enumType = enumTypeNamed(callName);
		if (!enumType.is_null() && !findCFunc(callName) && !findPlnFunc(callName)) {
			if (expr["args"].size() != 1) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CastArgCount, callName) << endl;
				exit(1);
			}
			json cast = {{"expr-type", "cast"}, {"target-type", enumType}, {"src", expr["args"][0]}};
			if (expr.contains("loc")) cast["loc"] = expr["loc"];
			return sa_expression(cast);
		}
		return sa_expr_call(expr);

	} else if (expr_type == "member-call") {
		return sa_expr_member_call(expr);

	} else if (expr_type == "field-access") {
		json enumerator = resolveEnumerator(expr);
		if (!enumerator.is_null()) return enumerator;
		return sa_expr_field_access(expr, /*forWrite=*/false);

	} else if (expr_type == "arr-index") {
		return sa_expr_arr_index(expr);
	}
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arith(const json& expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	// A lit-int operand takes the other operand's type, which takes
	// precedence over expectedType (e.g. `int64_var + 4` keeps int64); only
	// when both are literals does expectedType decide (`int32 x = (4+4)`).
	// Each literal is typed exactly once so its range check never fires on
	// a provisional type.
	if (isBoolPrim(expectedType)) expectedType = registry_.prim(PrimType::Name::Int32);
	json left, right;
	auto typeOf = [&](const json& other) {
		return other.contains("value-type") ? registry_.fromJson(other["value-type"]) : expectedType;
	};
	if (resolveConstRef(expr["left"])["expr-type"] == "lit-int") {
		right = promoteOperand(sa_expression(expr["right"], expectedType));
		left  = promoteOperand(sa_expression(expr["left"],  typeOf(right)));
	} else if (resolveConstRef(expr["right"])["expr-type"] == "lit-int") {
		left  = promoteOperand(sa_expression(expr["left"],  expectedType));
		right = promoteOperand(sa_expression(expr["right"], typeOf(left)));
	} else {
		left  = promoteOperand(sa_expression(expr["left"],  expectedType));
		right = promoteOperand(sa_expression(expr["right"], expectedType));
	}
	const PlnType* leftType  = registry_.fromJson(left["value-type"]);
	const PlnType* rightType = registry_.fromJson(right["value-type"]);
	if (expr_type == "bitand" || expr_type == "bitor" || expr_type == "bitxor") {
		// Checked on the operands themselves, not the post-promotion type: a
		// pointer/struct pair is Incompatible and would otherwise reach the
		// generic E_ArithOpNotNumeric below with a less specific message.
		for (const PlnType* t : {leftType, rightType}) {
			if (!isIntegerPrim(t)) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
				exit(1);
			}
		}
	}
	// Usual arithmetic conversions: both operands convert to one common type.
	// A pointer/struct operand (usualArithConv returns nullptr) is rejected --
	// Palan has no pointer arithmetic syntax, so `p + 1` has no meaning here.
	const PlnType* promoted = usualArithConv(leftType, rightType);
	if (!promoted) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArithOpNotNumeric) << endl;
		exit(1);
	}
	if (leftType != promoted)  left  = wrapConvert(left,  registry_.toJson(promoted));
	if (rightType != promoted) right = wrapConvert(right, registry_.toJson(promoted));
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

// Shared narrowing rule for every binding site. An integer/uint literal is
// exempt from the ExplicitCast rejection below, since its width/sign was
// never fixed by source syntax; pointer mutability is checked separately by
// each call site via ptrPermissionOk.
json PlnSemanticAnalyzer::convertForBinding(const json& locNode, json value,
		const PlnType* toType, const json& toTypeJson)
{
	if (!value.contains("value-type") || value["value-type"] == toTypeJson) return value;
	const PlnType* fromType = registry_.fromJson(value["value-type"]);
	TypeCompat compat = typeCompat(fromType, toType, registry_);
	if (compat == TypeCompat::ImplicitWiden) {
		value = wrapConvert(value, toTypeJson);
	} else if (compat == TypeCompat::ExplicitCast) {
		string et = value["expr-type"];
		bool untypedLit = (et == "lit-int" || et == "lit-uint") && fromType->kind != PlnType::Kind::Enum;
		if (!untypedLit || toType->kind == PlnType::Kind::Enum) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_InvalidNarrowingConv,
				typeDisplayName(value["value-type"]), typeDisplayName(toTypeJson)) << endl;
			exit(1);
		}
	} else if (compat == TypeCompat::Incompatible) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompatibleTypes,
			typeDisplayName(value["value-type"]), typeDisplayName(toTypeJson)) << endl;
		exit(1);
	}
	return value;
}

// Enforce ptrPermissionOk() at one call argument; the diagnostic differs by
// callee kind, since a C parameter's permission comes from const-qualification
// while a Palan parameter's comes from `@!T`.
void PlnSemanticAnalyzer::checkArgPtrPermission(const json& expr, const string& funcName,
		bool isCFunc, const json& saArg, const json& param, size_t argIdx)
{
	if (ptrPermissionOk(saArg["value-type"], param["var-type"]))
		return;
	if (isCFunc) {
		string paramName = param.value("name", "");
		if (paramName.empty()) paramName = "#" + to_string(argIdx + 1);
		cerr << locPrefix(expr)
		     << PlnSaMessage::getMessage(E_ReadOnlyPtrToNonConstCParam, funcName, paramName) << endl;
	} else {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
	}
	exit(1);
}

// A struct returned by a call is owned by nobody, and a borrow never frees
// what it points to.
void PlnSemanticAnalyzer::checkStructBorrowSource(const json& locNode, const json& saValue,
		const json& dstType)
{
	if (!isStructBorrow(dstType) || !isStructStorage(saValue["value-type"]))
		return;
	if (isExpiringStruct(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ExpiringStructToBorrow) << endl;
		exit(1);
	}
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StructBorrowNeedsAddrOf) << endl;
	exit(1);
}

void PlnSemanticAnalyzer::applyPlnCalleeSig(json& sa_expr, const json& pFunc)
{
	sa_expr["func-type"] = pFunc["func-type"];
	if (pFunc.contains("ret-type"))
		sa_expr["value-type"] = pFunc["ret-type"];
	if (pFunc.value("func-type", "") == "syscall")
		sa_expr["syscall-number"] = pFunc["syscall-number"];
}

json PlnSemanticAnalyzer::sa_expr_call(const json& expr)
{
	json sa_expr = expr;
	const json* funcParams;
	bool isCFunc = false;

	const json* cfunc = findCFunc(expr["name"]);
	if (cfunc) {
		isCFunc = true;
		sa_expr["func-type"] = "c";
		requireSupportedCFuncSig(*cfunc, expr["name"].get<string>(), expr);
		if (cfunc->contains("ret-type")
				&& (*cfunc)["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = (*cfunc)["ret-type"];
		funcParams = &(*cfunc)["parameters"];
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
			applyPlnCalleeSig(sa_expr, *pFunc);
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

	sa_expr["args"] = saCallArgs(expr, expr["args"], *funcParams, isCFunc, expr["name"].get<string>());
	return sa_expr;
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
	}

	json saArgs = json::array();
	size_t argIdx = 0;
	for (auto& arg : args) {
		const json* paramVT = argIdx < fixedCount ? &funcParams[argIdx]["var-type"] : nullptr;
		if (argIdx < fixedCount && funcParams[argIdx].value("_callback-param", false)) {
			// registry_.fromJson (below, and inside sa_expression) throws for a
			// pntr(func) type -- this parameter's slot never reaches either;
			// its only legal argument is a bare Palan function name.
			saArgs.push_back(sa_func_ref_arg(locNode, arg, funcName, funcParams[argIdx]));
			argIdx++;
			continue;
		}
		// A bare integer literal argument adopts the parameter's type here
		// instead of defaulting to int64/uint64, avoiding a spurious narrowing
		// rejection below.
		json saArg = sa_expression(arg, paramVT ? registry_.fromJson(*paramVT) : nullptr);
		if (saArg.contains("value-type")) {
			const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
			if (paramVT) {
				if (paramVT->contains("arr-size")) {
					checkArrBorrowBinding(locNode, arg, saArg, *paramVT);
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
							arrShapeName(argVT), arrShapeName(*paramVT)) << endl;
						exit(1);
					}
				}
				checkElemShape(locNode, saArg, *paramVT);
				saArg = convertCallArg(locNode, saArg, *paramVT);
				if (!isExpiringStruct(saArg))
					checkStructBorrowSource(locNode, saArg, *paramVT);
				checkArgPtrPermission(locNode, funcName, isCFunc, saArg, funcParams[argIdx], argIdx);
			} else if (isVariadic) {
				const PlnType* promoted = variadicPromote(fromType, registry_);
				if (promoted != fromType)
					saArg = wrapConvert(saArg, registry_.toJson(promoted));
			}
		}
		// The callee only borrows an argument, and an argument evaluated
		// conditionally (a '&&' operand, a loop condition) can't be released
		// by a statement around it, so the call itself releases it.
		if (isExpiringStruct(saArg)) {
			auto [fn, funcType] = structFreeFunc(saArg["value-type"]["base-type"]["type-name"].get<string>());
			saArg["release-after-call"] = {{"name",fn},{"func-type",funcType}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		saArgs.push_back(saArg);
		argIdx++;
	}
	return saArgs;
}

// Analyze the argument in a `_callback-param` slot (a C function pointer
// parameter, e.g. qsort's comparator): only a bare Palan function reference
// is legal, matched by exact ABI identity since glibc calls it directly with
// no Palan-side conversion step. Pointer permission is checked in the
// direction each value flows, so it's reversed for the return vs. each parameter.
json PlnSemanticAnalyzer::sa_func_ref_arg(const json& locNode, const json& arg,
                                           const string& cFuncName, const json& param)
{
	string spelling = arg.value("expr-type", "") == "id" ? arg["name"].get<string>() : "<expression>";
	const json* pFunc = nullptr;
	if (arg.value("expr-type", "") == "id" && findVar(spelling) == nullptr)
		pFunc = findPlnFunc(spelling);
	if (pFunc == nullptr) {
		cerr << locPrefix(locNode)
		     << PlnSaMessage::getMessage(E_CallbackArgRequiresFunc, cFuncName, spelling) << endl;
		exit(1);
	}

	auto mismatch = [&]() {
		cerr << locPrefix(locNode)
		     << PlnSaMessage::getMessage(E_CallbackSignatureMismatch, spelling, cFuncName) << endl;
		exit(1);
	};

	const json& cFuncType = param["var-type"]["base-type"]; // {"type-kind":"func",...}
	const json* cParams = cFuncType.contains("parameters") ? &cFuncType["parameters"] : nullptr;
	const json* pParams = pFunc->contains("parameters") ? &(*pFunc)["parameters"] : nullptr;
	size_t cCount = cParams ? cParams->size() : 0;
	size_t pCount = pParams ? pParams->size() : 0;
	if (cCount != pCount) mismatch();
	for (size_t i = 0; i < cCount; i++) {
		const json& cp = (*cParams)[i];
		const json& pp = (*pParams)[i];
		if (!cp.contains("var-type") || !pp.contains("var-type")) mismatch(); // e.g. C "..." marker
		const json& cvt = cp["var-type"];
		const json& pvt = pp["var-type"];
		if (typeCompat(registry_.fromJson(cvt), registry_.fromJson(pvt), registry_) != TypeCompat::Identical)
			mismatch();
		if (!ptrPermissionOk(cvt, pvt)) mismatch();
	}

	bool cVoidRet = !cFuncType.contains("ret-type")
		|| cFuncType["ret-type"].value("type-name", "") == "void";
	bool pMultiRet = pFunc->contains("rets") && (*pFunc)["rets"].size() > 1;
	bool pHasRet = pFunc->contains("ret-type");
	if (pMultiRet) mismatch();
	if (cVoidRet) {
		if (pHasRet) mismatch();
	} else {
		if (!pHasRet) mismatch();
		const json& cRetVT = cFuncType["ret-type"];
		const json& pRetVT = (*pFunc)["ret-type"];
		if (typeCompat(registry_.fromJson(pRetVT), registry_.fromJson(cRetVT), registry_) != TypeCompat::Identical)
			mismatch();
		if (!ptrPermissionOk(pRetVT, cRetVT)) mismatch();
	}

	json out = {{"expr-type", "func-ref"}, {"name", spelling}};
	if (locNode.contains("loc")) out["loc"] = locNode["loc"];
	return out;
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

json PlnSemanticAnalyzer::enumTypeNamed(const string& name) const
{
	json t = resolveTypeAlias({{"type-kind", "prim"}, {"type-name", name}});
	return t.contains("enum") ? t : json();
}

// `Name.X` where Name is an enum type (or an alias of one) not shadowed by a
// variable; null when the field access is an ordinary one.
json PlnSemanticAnalyzer::resolveEnumerator(const json& expr)
{
	const json& obj = expr["object"];
	if (obj.value("expr-type", "") != "id") return json();
	string typeName = obj["name"].get<string>();
	if (findVar(typeName) || findCGlobal(typeName)) return json();
	json enumType = enumTypeNamed(typeName);
	if (enumType.is_null()) return json();

	const auto& values = enumDefs_.at(enumType["enum"].get<string>()).values;
	string field = expr["field"].get<string>();
	auto it = values.find(field);
	if (it == values.end()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownEnumerator, field, typeName) << endl;
		exit(1);
	}
	json lit = {{"expr-type", "lit-int"}, {"value", to_string(it->second)}, {"value-type", enumType}};
	if (expr.contains("loc")) lit["loc"] = expr["loc"];
	return lit;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_field_access(const json& expr, bool forWrite)
{
	FieldChain chain = resolveObjectChain(expr["object"], forWrite);
	string fn = expr["field"].get<string>();
	const FieldLayout& fld = findFieldOrExit(chain.structName, fn, expr);
	if (fld.typeKind == "embed") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_InlineStructAsValue) << endl;
		exit(1);
	}
	return makeFieldAccess(chain, fld);
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::makeFieldAccess(const FieldChain& chain, const FieldLayout& fld)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json vt = fieldValueType(fld);
	int off = chain.offset + fld.offset;
	// embed/embed-arr/embed-ptr-arr fields are inline data (no pointer is
	// actually stored at this offset): the field's "value" is its own address,
	// computed as ptr+offset, not a load of the memory there.
	bool addrOnly = (fld.typeKind == "embed" || fld.typeKind == "embed-arr" || fld.typeKind == "embed-ptr-arr");
	if (!chain.isPointerBased)
		return {{"expr-type","field-access"},{"var",chain.varName},{"offset",off},{"value-type",vt},{"addr-only",addrOnly}};
	else
		return {{"expr-type","field-access"},{"ptr-expr",chain.ptrExpr},{"offset",off},{"value-type",vt},{"addr-only",addrOnly}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arr_index(const json& expr, bool forWrite)
{
	json sa_expr = expr;
	const json& arr = expr["array"];
	string arr_kind = arr.value("expr-type", "");
	json sa_array = !forWrite ? sa_expression(arr)
		: arr_kind == "field-access" ? sa_expr_field_access(arr, true)
		: arr_kind == "arr-index"    ? sa_expr_arr_index(arr, true)
		: sa_expression(arr);
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
		if (elem_type.value("type-kind", "") == "struct") {
			// [n]$T contiguous struct array: pts[i] -> pntr(struct(T)), inline address
			// (stride = T.totalSize, carried on array_type; arr-index itself does not deref)
			int64_t stride = array_type.value("stride", (int64_t)0);
			json row_pntr = {{"type-kind","pntr"},{"base-type",elem_type}};
			json elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			};
			sa_expr["array"]      = sa_array;
			sa_expr["index"]      = sa_index;
			sa_expr["elem-size"]  = elem_size_node;
			sa_expr["value-type"] = row_pntr;
			sa_expr["addr-only"]  = true;
			return sa_expr;
		}

		if (elem_type.value("type-kind", "") == "prim") {
			// [n]$T embedded array field, primitive leaf: data[i] -> scalar value
			int64_t stride = array_type.value("stride", (int64_t)0);
			json elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			};
			sa_expr["array"]      = sa_array;
			sa_expr["index"]      = sa_index;
			sa_expr["elem-size"]  = elem_size_node;
			sa_expr["value-type"] = elem_type;
			sa_expr["addr-only"]  = false;
			return sa_expr;
		}

		// [n]$[m]T row access: mat[i] -> the row, laid out in place.
		// Without a stride the row size is only known at run time, from __name_d1.
		json elem_size_node;
		if (array_type.contains("stride")) {
			elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(array_type["stride"].get<int64_t>())},{"value-type",uint64_type}
			};
		} else {
			int elem_sz = elemSizeBytes(elem_type["base-type"].value("type-name",""));
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
		sa_expr["value-type"] = elem_type;
		sa_expr["addr-only"]  = true;
		return sa_expr;
	}

	if (elem_type.value("type-kind", "") == "struct") {
		// `p[i]` on a struct pointee is an address computation (base +
		// i*sizeof(T)), not a load -- Palan has no register-sized struct value.
		int64_t stride = requireCompleteStruct(elem_type["type-name"].get<string>(), expr).totalSize;
		json elem_pntr = {{"type-kind","pntr"},{"mutable",array_type.value("mutable", true)},
		                  {"base-type",elem_type}};
		json elem_size_node = {
			{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
		};
		sa_expr["array"]      = sa_array;
		sa_expr["index"]      = sa_index;
		sa_expr["elem-size"]  = elem_size_node;
		sa_expr["value-type"] = elem_pntr;
		sa_expr["addr-only"]  = true;
		return sa_expr;
	}

	if (elem_type.value("type-kind","") == "prim" && elem_type.value("type-name","") == "void") {
		// `void` has no size; unlike E_UnknownStructType below (an unrecognized
		// name), this pointee name IS recognized -- it's just not indexable.
		// Covers read (p[i]), write (v -> p[i] via sa_arr_assign_stmt), and
		// element-address-of (@p[i], reached before its own guard runs).
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_DerefVoidPointer) << endl;
		exit(1);
	}
	int sz = (elem_type.value("type-kind","") == "pntr") ? 8
		: elemSizeBytes(elem_type.value("type-name",""));
	if (sz < 0) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownStructType, elem_type.value("type-name","")) << endl;
		exit(1);
	}
	json elem_size_node = {
		{"expr-type", "lit-uint"}, {"value", to_string(sz)}, {"value-type", uint64_type}
	};

	sa_expr["array"]      = sa_array;
	sa_expr["index"]      = sa_index;
	sa_expr["elem-size"]  = elem_size_node;
	sa_expr["value-type"] = elem_type;
	sa_expr["addr-only"]  = false;
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

	if (isCFunc) {
		sa_expr["func-type"] = "c";
		requireSupportedCFuncSig(*pFunc, method, expr);
		if (pFunc->contains("ret-type")
				&& (*pFunc)["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = (*pFunc)["ret-type"];
	} else {
		applyPlnCalleeSig(sa_expr, *pFunc);
	}

	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";

	sa_expr["args"] = saCallArgs(expr, expr["args"], (*pFunc)["parameters"], isCFunc, method);
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE
