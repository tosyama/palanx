/// Palan Semantic Analyzer — expression analysis
///
/// @file PlnSaExpr.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <charconv>
#include <cmath>
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
			undefinedNameError(obj, obj, E_UndefinedVariable);
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
		json saBase = sa_expression(obj);
		requireOwnedBase(saBase, obj);
		json vt = saBase.value("value-type", json::object());
		if (!isStructPntr(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		string structName = vt["base-type"]["type-name"].get<string>();
		return {true, "", 0, move(saBase), structName};
	}
	FieldChain base = resolveObjectChain(obj["object"], forWrite);
	string fn = obj["field"].get<string>();
	const FieldLayout* it = &findFieldOrExit(base.structName, fn, obj);
	if (it->typeKind == "prim") {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	if (forWrite && it->typeKind == "raw-ptr" && !it->isMutable) {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteToImmutablePtrField) << endl;
		exit(1);
	}
	if (it->typeKind == "raw-ptr" && (it->elemKind != "struct" || !it->innerPtrMutable.empty())) {
		// A pointer-to-primitive (e.g. `@!int64 p;`) or pointer-to-pointer field
		// has nothing further to chain into -- this hop must be the chain's
		// terminal, typed by the caller via fieldValueType, not
		// resolveObjectChain's own pntr(struct(...)).
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

// A call result can be indexed or have its fields accessed only when something
// else owns what it points to: nothing would release an owned struct or array
// a call returns, and a borrow of a temp given to the call outlives that temp.
void PlnSemanticAnalyzer::requireOwnedBase(const json& saBase, const json& locNode)
{
	if (holdsCallTemps(saBase)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
	json vt = saBase.value("value-type", json::object());
	if (vt.value("type-kind", "") == "struct"
	    || (saBase.value("category", "") == "expiring" && !isPtrBorrow(vt))) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnownedCallResult) << endl;
		exit(1);
	}
}

const FieldLayout& PlnSemanticAnalyzer::findFieldOrExit(const string& structName, const string& fieldName, const json& locNode)
{
	const StructDef& def = requireCompleteStruct(structName, locNode);
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fieldName; });
	if (it == def.fields.end()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownField, structName, fieldName, def.keyword()) << endl;
		exit(1);
	}
	if (!it->unsupported.empty()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsupportedCFieldType, structName, fieldName, it->unsupported) << endl;
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

	// A literal operand is accepted only as a call argument (sa_lit_arg).
	if (isAggregateLit(obj))
		sa_expression(obj);

	if (obj_type == "id") {
		string name = obj["name"].get<string>();
		const json* varType = findVar(name);
		if (varType == nullptr) {
			if (findCGlobal(name) != nullptr) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CGlobalNotAddressable, name) << endl;
				exit(1);
			}
			undefinedNameError(expr, obj, E_UndefinedVariable);
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
		if (isOwnedVar(name) || varType->contains("arr-size")) {
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
		bool isArrField = fld.typeKind == "arr-ptr" || fld.typeKind == "embed-arr" || fld.typeKind == "embed-ptr-arr";
		if (fld.typeKind != "prim" && fld.typeKind != "embed" && fld.typeKind != "struct-ptr" && !isArrField) {
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
		// An array field is borrowed as an array, like an array variable: an
		// owned one ([n]T) holds the pointer to its elements, an embedded one
		// is the elements.
		bool addrOnly = fld.typeKind != "struct-ptr" && fld.typeKind != "arr-ptr";
		json pntr_type;
		if (isArrField) {
			pntr_type = withArrPermission(fieldValueType(fld), isMutable);
		} else {
			pntr_type = (fld.typeKind == "prim")
				? json{{"type-kind","pntr"},{"base-type",fieldValueType(fld)}}
				: fieldValueType(fld);
			pntr_type["mutable"] = isMutable;
		}
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

	// A call operand is accepted only as a call argument (sa_call_borrow_arg):
	// elsewhere nothing would release the returned struct.
	if (isCallBorrow(expr) && isExpiringStruct(sa_expression(obj))) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ExpiringStructToBorrow) << endl;
		exit(1);
	}
	cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
	exit(1);
} // LCOV_EXCL_EXCEPTION_BR_LINE

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

// The source type that declares an owned array of the levels of saType (a
// borrowed array parameter); null when an element is not one a literal gives.
static json astArrType(const json& saType)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json out = {{"type-kind","arr"},{"specifier","raw"},
		{"size-expr",{{"expr-type","lit-uint"},{"value",to_string(saType["arr-size"].get<int64_t>())}}}};
	if (saType.value("embedded", false))
		out["embedded"] = true;
	const json& base = saType["base-type"];
	json elem;
	if (isArrLevel(base) && base.contains("arr-size"))
		elem = astArrType(base);
	else if (base.value("type-kind","") == "struct")
		elem = {{"type-kind","prim"},{"type-name",base["type-name"]}};
	else if (isStructStorage(base))
		elem = {{"type-kind","prim"},{"type-name",base["base-type"]["type-name"]}};
	else if (base.value("type-kind","") == "prim")
		elem = base;
	if (elem.is_null())
		return nullptr;
	out["base-type"] = move(elem);
	return out;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A literal argument ('[..]', '{..}', or one under '@'/'@!') becomes a temp of
// the parameter's struct or array type, initialized by body; the argument is
// then analyzed as the temp written in its place ('@__lit_N'), returned with
// argAst set to that form. A null paramVT is a C function's parameter.
json PlnSemanticAnalyzer::sa_lit_arg(const json& arg, const json* paramVT, const string& paramName,
                                     const string& funcName, json& argAst, json& body, json& release)
{
	bool isAddrOf = arg.value("expr-type", "") == "addr-of";
	const json& lit = isAddrOf ? arg["object"] : arg;
	auto rejectParamType = [&](PlnSaMessageCode code) {
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

static const set<string> foldableOps = {
	"add", "sub", "mul", "div", "mod", "bitand", "bitor", "bitxor", "shl", "shr", "neg", "bitnot", "sizeof"
};

// A folded result is untyped like a source literal, so the context types it
// and checkIntLiteralRange rejects what does not fit there.
json PlnSemanticAnalyzer::foldConstExpr(const json& expr)
{
	string et = expr.value("expr-type", "");
	json lit;
	if (et == "id") {
		string name = expr["name"].get<string>();
		auto cit = constDecls_.find(name);
		if (cit == constDecls_.end()) return expr;
		lit = cit->second;
	} else if (foldableOps.count(et)) {
		optional<ConstVal> v = evalConst(expr);
		if (!v) return expr;
		switch (v->kind) {
			case ConstVal::Kind::Int:  lit = {{"expr-type", "lit-int"},  {"value", to_string(v->i)}}; break;
			case ConstVal::Kind::Uint: lit = {{"expr-type", "lit-uint"}, {"value", to_string(v->u)}}; break;
			case ConstVal::Kind::Flo: {
				char buf[32];
				auto r = to_chars(buf, buf + sizeof(buf), v->f);
				string s(buf, r.ptr);
				if (s.find_first_of(".e") == string::npos) s += ".0";
				lit = {{"expr-type", "lit-flo"}, {"value", s}};
				break;
			}
		}
	} else {
		return expr;
	}
	if (expr.contains("loc")) lit["loc"] = expr["loc"];
	return lit;
}

// Evaluates in int64, or uint64 once a 'u' operand joins, like the usual
// arithmetic conversions; a C macro constant's own type is dropped here.
// The right operand goes first: a left-associative chain over a variable
// (`a + b + c`) then fails at each level without walking the whole chain.
optional<ConstVal> PlnSemanticAnalyzer::evalConst(const json& expr)
{
	using K = ConstVal::Kind;
	string et = expr.value("expr-type", "");
	auto overflow = [&]() {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprOverflow) << endl;
		exit(1);
	};
	if (et == "lit-int" || et == "lit-uint") {
		string text = expr["value"].get<string>();
		ConstVal v{et == "lit-int" ? K::Int : K::Uint};
		const char* first = text.data();
		const char* last = text.data() + text.size();
		bool ok = v.kind == K::Int ? from_chars(first, last, v.i).ec == errc()
		                           : from_chars(first, last, v.u).ec == errc();
		if (!ok) overflow();
		return v;
	}
	if (et == "lit-flo") {
		ConstVal v{K::Flo};
		string text = expr["value"].get<string>();
		from_chars(text.data(), text.data() + text.size(), v.f);
		return v;
	}
	if (et == "sizeof") {
		return ConstVal{K::Uint, 0, (uint64_t)typeByteSize(expr, resolveKnownType(expr, expr["type"]))};
	}
	if (et == "id") {
		json lit = foldConstExpr(expr);
		if (lit.value("expr-type", "") == "id" || lit.value("expr-type", "") == "lit-str") return nullopt;
		return evalConst(lit);
	}
	if (et == "neg" || et == "bitnot") {
		optional<ConstVal> v = evalConst(expr["operand"]);
		if (!v) return nullopt;
		if (et == "bitnot") {
			if (v->kind == K::Flo) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
				exit(1);
			}
			v->i = ~v->i;
			v->u = ~v->u;
		} else if (v->kind == K::Flo) {
			v->f = -v->f;
		} else if (v->kind == K::Uint) {
			if (v->u != 0) overflow();
		} else {
			if (__builtin_sub_overflow((int64_t)0, v->i, &v->i)) overflow();
		}
		return v;
	}
	if (!foldableOps.count(et)) return nullopt;

	optional<ConstVal> r = evalConst(expr["right"]);
	if (!r) return nullopt;
	optional<ConstVal> l = evalConst(expr["left"]);
	if (!l) return nullopt;

	bool isShift = et == "shl" || et == "shr";
	bool isBit = et == "bitand" || et == "bitor" || et == "bitxor" || isShift;
	if (isShift && l->kind != K::Flo && r->kind != K::Flo) {
		// Bits shifted out are not an overflow; the count is taken modulo the
		// 64-bit width an untyped constant is computed in.
		int count = (r->kind == K::Int ? (uint64_t)r->i : r->u) & 63;
		uint64_t bits = l->kind == K::Int ? (uint64_t)l->i : l->u;
		bits = et == "shl" ? bits << count : bits >> count;
		l->i = (int64_t)bits;
		l->u = bits;
		return l;
	}
	if (l->kind == K::Flo || r->kind == K::Flo) {
		if (isBit) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
			exit(1);
		}
		if (et == "mod") {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FloatModulo) << endl;
			exit(1);
		}
		auto toF = [](const ConstVal& v) {
			return v.kind == K::Flo ? v.f : v.kind == K::Uint ? (double)v.u : (double)v.i;
		};
		double a = toF(*l), b = toF(*r);
		ConstVal v{K::Flo};
		v.f = et == "add" ? a + b : et == "sub" ? a - b : et == "mul" ? a * b : a / b;
		if (!isfinite(v.f)) {
			if (et == "div" && b == 0) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprDivByZero) << endl;
				exit(1);
			}
			overflow();
		}
		return v;
	}

	if ((et == "div" || et == "mod") && (r->kind == K::Int ? r->i == 0 : r->u == 0)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprDivByZero) << endl;
		exit(1);
	}
	if (l->kind == K::Uint || r->kind == K::Uint) {
		for (ConstVal* v : {&*l, &*r}) {
			if (v->kind != K::Int) continue;
			if (v->i < 0) overflow();
			v->u = v->i;
		}
		uint64_t a = l->u, b = r->u;
		ConstVal v{K::Uint};
		if      (et == "add") { if (__builtin_add_overflow(a, b, &v.u)) overflow(); }
		else if (et == "sub") { if (__builtin_sub_overflow(a, b, &v.u)) overflow(); }
		else if (et == "mul") { if (__builtin_mul_overflow(a, b, &v.u)) overflow(); }
		else if (et == "div") v.u = a / b;
		else if (et == "mod") v.u = a % b;
		else if (et == "bitand") v.u = a & b;
		else if (et == "bitor")  v.u = a | b;
		else                     v.u = a ^ b;
		return v;
	}
	int64_t a = l->i, b = r->i;
	ConstVal v{K::Int};
	if      (et == "add") { if (__builtin_add_overflow(a, b, &v.i)) overflow(); }
	else if (et == "sub") { if (__builtin_sub_overflow(a, b, &v.i)) overflow(); }
	else if (et == "mul") { if (__builtin_mul_overflow(a, b, &v.i)) overflow(); }
	else if (et == "div") { if (a == INT64_MIN && b == -1) overflow(); v.i = a / b; }
	else if (et == "mod") v.i = b == -1 ? 0 : a % b;
	else if (et == "bitand") v.i = a & b;
	else if (et == "bitor")  v.i = a | b;
	else                     v.i = a ^ b;
	return v;
}

json PlnSemanticAnalyzer::sa_expression(const json &rawExpr, const PlnType* expectedType)
{
	const json expr = foldConstExpr(rawExpr);
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
	// gen-ast lowers an assignment written as a statement, so one reaching
	// here is nested in another expression.
	if (expr_type == "assign-expr" || expr_type == "arr-assign-expr"
			|| expr_type == "field-assign-expr" || expr_type == "tapple-assign-expr") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AssignInExpr) << endl;
		exit(1);
	}
	if (expr_type == "not-impl") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ExprNotImplemented) << endl;
		exit(1);
	}

	if (expr_type == "lit-int") {
		// A lit-int already carrying a value-type is a macro constant folded
		// in by gen-ast (a plain source literal never has one) -- its type
		// was fixed by the C declaration, not by this expression's context,
		// so expectedType must not override it.
		if (expr.contains("value-type")) {
			resolveCTypeRefs(sa_expr["value-type"]);
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
			if (isOwnedVar(expr["name"].get<string>()))
				sa_expr["category"] = "owned";
		} else {
			string name = expr["name"].get<string>();
			const json* cglobal = findCGlobal(name);
			if (cglobal != nullptr) {
				requireSupportedCGlobal(*cglobal, name, expr);
				sa_expr = {{"expr-type", "c-global"}, {"label", name}, {"value-type", (*cglobal)["var-type"]}}; // LCOV_EXCL_EXCEPTION_BR_LINE
				if (expr.contains("loc")) sa_expr["loc"] = expr["loc"];
			} else {
				json ref = sa_func_ref(expr);
				if (!ref.is_null()) return ref;
				undefinedNameError(expr, expr, E_UndefinedVariable);
			}
		}

	} else if (expr_type == "addr-of") {
		return sa_expr_addr_of(expr);

	} else if (expr_type == "add" || expr_type == "sub"
	        || expr_type == "mul" || expr_type == "div" || expr_type == "mod"
	        || expr_type == "bitand" || expr_type == "bitor" || expr_type == "bitxor") {
		return sa_expr_arith(expr, expectedType);

	} else if (expr_type == "shl" || expr_type == "shr") {
		return sa_expr_shift(expr, expectedType);

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
		return sa_expr_cast(expr);

	} else if (expr_type == "call") {
		const string& callName = expr["name"].get<string>();
		if (!findCFunc(callName) && !findPlnFunc(callName)) {
			json cast = aliasCast(expr, callName);
			if (!cast.is_null()) return cast;
		}
		return sa_expr_call(expr);

	} else if (expr_type == "member-call") {
		string qualified = typeNameWritten(expr["object"]);
		if (!qualified.empty()) {
			json cast = aliasCast(expr, qualified + "." + expr["method"].get<string>());
			if (!cast.is_null()) return cast;
		}
		json field = funcPtrField(expr);
		if (!field.is_null())
			return sa_expr_call_ptr(expr, sa_expression(field), expr["method"].get<string>());
		json call = normalizeMethodCall(expr);
		return call["expr-type"] == "call" ? sa_expr_call(call) : sa_expr_member_call(expr);

	} else if (expr_type == "chain-call") {
		return sa_expr_chain_call(expr);

	} else if (expr_type == "field-access") {
		json enumerator = resolveEnumerator(expr);
		if (!enumerator.is_null()) return enumerator;
		return sa_expr_field_access(expr, /*forWrite=*/false);

	} else if (expr_type == "arr-index") {
		return sa_expr_arr_index(expr);
	}
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The count does not join the usual arithmetic conversions, so the result
// keeps the left operand's width. Converted to that type, the count keeps the
// low bits that a count modulo the width needs.
json PlnSemanticAnalyzer::sa_expr_shift(const json& expr, const PlnType* expectedType)
{
	if (isBoolPrim(expectedType)) expectedType = registry_.prim(PrimType::Name::Int32);
	json left  = promoteOperand(sa_expression(expr["left"], expectedType));
	json right = promoteOperand(sa_expression(expr["right"]));
	const PlnType* leftType  = registry_.fromJson(left["value-type"]);
	const PlnType* rightType = registry_.fromJson(right["value-type"]);
	if (!isIntegerPrim(leftType) || !isIntegerPrim(rightType)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
		exit(1);
	}
	if (rightType != leftType) right = wrapConvert(right, left["value-type"]);
	json sa_expr = expr;
	sa_expr["left"]       = left;
	sa_expr["right"]      = right;
	sa_expr["value-type"] = left["value-type"];
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
	if (foldConstExpr(expr["left"])["expr-type"] == "lit-int") {
		right = promoteOperand(sa_expression(expr["right"], expectedType));
		left  = promoteOperand(sa_expression(expr["left"],  typeOf(right)));
	} else if (foldConstExpr(expr["right"])["expr-type"] == "lit-int") {
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
// each call site via checkPtrPermission.
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

void PlnSemanticAnalyzer::checkPtrPermission(const json& locNode, const json& from, const json& to)
{
	switch (ptrPermission(from, to)) {
	case PtrPermission::Ok:
		return;
	case PtrPermission::Upgrade:
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
		break;
	case PtrPermission::InnerNarrowing:
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_PtrInnerPermissionNarrowing,
			typeDisplayName(from), typeDisplayName(to)) << endl;
		break;
	case PtrPermission::FuncSigMismatch:
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompatibleTypes,
			typeDisplayName(from), typeDisplayName(to)) << endl;
		break;
	}
	exit(1);
}

// A struct returned by a call is owned by nobody, and a borrow never frees
// what it points to.
void PlnSemanticAnalyzer::checkBorrowSource(const json& locNode, const json& saValue,
		const json& dstType)
{
	if (holdsCallTemps(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
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

// A function named as a value: its address, typed as the `@func` it can be
// called through. Null if `expr` names no function.
json PlnSemanticAnalyzer::sa_func_ref(const json& expr)
{
	const string& name = expr["name"].get<string>();
	auto reject = [&](const string& reason) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNotValue, name, reason) << endl;
		exit(1);
	};
	json func = {{"type-kind","func"},{"parameters",json::array()}};
	const json* cfunc = findCFunc(name);
	const json* pFunc = cfunc;
	if (cfunc) {
		requireSupportedCFuncSig(*cfunc, name, expr);
		func["ret-type"] = (*cfunc)["ret-type"];
	} else {
		pFunc = findPlnFunc(name);
		if (!pFunc) pFunc = findImportFunc(name);
		if (!pFunc) return json();
		if (pFunc->empty()) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AmbiguousCall, name) << endl;
			exit(1);
		}
		if (pFunc->value("func-type", "") == "syscall") reject("it is a syscall");
		if (pFunc->contains("rets") && (*pFunc)["rets"].size() > 1) reject("it returns multiple values");
		func["ret-type"] = pFunc->contains("ret-type") ? (*pFunc)["ret-type"]
		                 : pFunc->contains("rets") ? (*pFunc)["rets"][0]["var-type"]
		                 : json{{"type-kind","prim"},{"type-name","void"}};
	}
	for (auto& p : (*pFunc)["parameters"]) {
		if (p.value("name", "") == "...") reject("it takes variable arguments");
		func["parameters"].push_back({{"var-type", p["var-type"]}});
	}
	const json& ret = func["ret-type"];
	bool retOk = ret.value("type-name", "") == "void" || isFuncSigType(ret);
	for (auto& p : func["parameters"])
		if (!isFuncSigType(p["var-type"]))
			reject("its parameter type '" + typeDisplayName(p["var-type"]) + "' cannot be passed through one");
	if (!retOk)
		reject("its return type '" + typeDisplayName(ret) + "' cannot be returned through one");
	json out = {{"expr-type", "func-ref"}, {"name", name},
	            {"value-type", {{"type-kind","pntr"},{"mutable",false},{"base-type",move(func)}}}};
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
	if (findCFunc(name) || findPlnFunc(name) || findImportFunc(name)) {
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
	auto def = structDefs_.find(vt["base-type"]["type-name"].get<string>());
	if (def == structDefs_.end()) return json();
	auto& fields = def->second.fields;
	auto f = find_if(fields.begin(), fields.end(), [&](const FieldLayout& f) { return f.name == method; });
	if (f == fields.end() || !f->unsupported.empty() || !isFuncPtr(fieldValueType(*f))) return json();
	bool isCFunc;
	const json* pFunc = findCalleeByName(method, isCFunc);
	if (pFunc && !(*pFunc)["parameters"].empty() && (*pFunc)["parameters"][0]["var-type"].value("mutable", false)) {
		cerr << locPrefix(memberCall) << PlnSaMessage::getMessage(E_AmbiguousFuncPtrCall, method) << endl;
		exit(1);
	}
	json access = {{"expr-type", "field-access"}, {"object", obj}, {"field", method}};
	if (memberCall.contains("loc")) access["loc"] = memberCall["loc"];
	return access;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A call through a function pointer. Its arguments follow a Palan function's
// rules whatever the pointer came from: the pointer's type is all that is
// known of the callee.
json PlnSemanticAnalyzer::sa_expr_call_ptr(const json& expr, json callee, const string& funcName)
{
	const json& func = callee["value-type"]["base-type"];
	json sa_expr = {{"expr-type", "call-ptr"}, {"callee", callee}};
	if (expr.contains("loc")) sa_expr["loc"] = expr["loc"];
	if (func["ret-type"].value("type-name", "") != "void")
		sa_expr["value-type"] = func["ret-type"];
	sa_expr["args"] = saCallArgs(expr, expr["args"], func["parameters"], false, funcName);
	return sa_expr;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_call(const json& expr)
{
	json sa_expr = expr;
	const json* funcParams;
	bool isCFunc = false;

	if (findFuncPtrVar(expr)) {
		json id = {{"expr-type", "id"}, {"name", expr["name"]}};
		if (expr.contains("loc")) id["loc"] = expr["loc"];
		return sa_expr_call_ptr(expr, sa_expression(id), expr["name"].get<string>());
	}

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
			undefinedNameError(expr, expr, E_UndefinedFunction);
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
		if (!saArg.contains("value-type")) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
			exit(1);
		}
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
			saArg["struct-arg"] = {{"size", def.totalSize}, {"eightbytes", eightbytesJson(def, structDefs_)}};
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

json PlnSemanticAnalyzer::enumTypeNamed(const string& name) const
{
	const json* t = findTypeBinding(name);
	return t && t->contains("enum") ? *t : json();
} // LCOV_EXCL_EXCEPTION_BR_LINE

// C's sizeof of the type as Palan lays it out: an array holds its elements
// inline, but only a '$' element is stored in place -- a struct or row
// element without '$' is a pointer slot.
int64_t PlnSemanticAnalyzer::typeByteSize(const json& locNode, json t)
{
	string tk = t.value("type-kind", "");
	if (tk == "pntr")
		return 8;
	if (tk == "embed") {
		json base = t["base-type"];
		if (base.value("type-kind", "") == "prim" && !isKnownTypeName(base.value("type-name", ""))) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownType, base.value("type-name", "")) << endl;
			exit(1);
		}
		if (!isStructType(base)) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedElemNotStruct, typeDisplayName(base)) << endl;
			exit(1);
		}
		return typeByteSize(locNode, base);
	}
	if (tk == "arr") {
		int64_t n = t["size-expr"].is_null() ? -1 : constArrSize(sa_arr_size_expr(locNode, t["size-expr"]));
		if (n < 0) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_SizeofArrSizeNotConst) << endl;
			exit(1);
		}
		json elem = t["base-type"];
		bool inPlace = t.value("embedded", false)
		            || (elem.value("type-kind", "") == "prim" && !isStructType(elem));
		return n * (inPlace ? typeByteSize(locNode, elem) : 8);
	}
	string name = t["type-name"].get<string>();
	if (structDefs_.count(name))
		return requireCompleteStruct(name, locNode).totalSize;
	return elemSizeBytes(name);
}

// `Name.X` where Name is an enum type (or an alias of one) not shadowed by a
// variable; null when the field access is an ordinary one.
// The type name `T` or `V.T` an expression spells, if it names no variable;
// empty otherwise.
string PlnSemanticAnalyzer::typeNameWritten(const json& obj) const
{
	string et = obj.value("expr-type", "");
	if (et == "field-access") {
		string qualifier = typeNameWritten(obj["object"]);
		return qualifier.empty() || qualifier.find('.') != string::npos
		       ? "" : qualifier + "." + obj["field"].get<string>();
	}
	if (et != "id") return "";
	string name = obj["name"].get<string>();
	return findVar(name) || findCGlobal(name) ? "" : name;
}

// gen-ast only knows the prim keywords as cast targets; an alias (an enum
// included) is a type only SA can see, so its cast arrives shaped as a call.
// Null when `typeName` is no alias.
json PlnSemanticAnalyzer::aliasCast(const json& expr, const string& typeName)
{
	json written = {{"type-kind", "prim"}, {"type-name", typeName}};
	requireWritableTypeName(expr, typeName);
	const json* target = findTypeBinding(typeName);
	if (!target || *target == written) return json();
	if (expr["args"].size() != 1) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CastArgCount, typeName) << endl;
		exit(1);
	}
	json cast = {{"expr-type", "cast"}, {"target-type", written}, {"src", expr["args"][0]}};
	if (expr.contains("loc")) cast["loc"] = expr["loc"];
	return sa_expression(cast);
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expr_cast(const json& expr)
{
	const json& written = expr["target-type"];
	json resolved = resolveKnownType(expr, written);
	string tk = resolved.value("type-kind", "");
	if (written.value("type-kind", "") == "arr") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CastToOwnedArr) << endl;
		exit(1);
	}
	if (tk == "arr" || isStructType(resolved)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_InvalidCastTarget, typeDisplayName(written)) << endl;
		exit(1);
	}
	json src = sa_expression(expr["src"]);
	if (!src.contains("value-type")) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	const json& from = src["value-type"];
	// A struct value is held as a pointer to its storage, but it is not one.
	bool fromPntr = from.value("type-kind", "") == "pntr" && !isStructStorage(from);
	auto incompatible = [&](const json& target) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_IncompatibleTypeCast,
			typeDisplayName(from), typeDisplayName(target)) << endl;
		exit(1);
	};

	// A pointer cast changes no bits: it may retarget the pointee, change its
	// permission, or view the memory as a borrowed array, unchecked as `p[i]` is.
	// A pointer only fits a 64-bit integer, so narrower integers are rejected
	// rather than silently dropping the upper half.
	auto isWord = [](const json& t) {
		string n = t.value("type-kind", "") == "prim" && !t.contains("enum") ? t.value("type-name", "") : "";
		return n == "int64" || n == "uint64";
	};
	if (tk == "pntr") {
		json target = deepNormalizePrimToStruct(normalizeArrBorrowType(expr, resolved));
		if (from == target) return src;
		bool toArr = target.contains("arr-size");
		if (!fromPntr && (toArr || !isWord(from))) incompatible(target);
		return wrapConvert(src, target);
	}
	if (fromPntr) {
		if (!isWord(resolved)) incompatible(resolved);
		return wrapConvert(src, resolved);
	}

	const PlnType* target  = registry_.fromJson(resolved);
	const PlnType* srcType = registry_.fromJson(from);
	TypeCompat compat      = typeCompat(srcType, target, registry_);
	if (compat == TypeCompat::Identical) {
		return src;
	} else if (compat == TypeCompat::ExplicitCast && isBoolPrim(target)) {
		// C's conversion to _Bool: any nonzero value is 1, not a truncation.
		// LCOV_EXCL_EXCEPTION_BR_START
		json zero = isFloatPrim(srcType)
			? json{{"expr-type", "lit-flo"}, {"value", "0.0"}, {"value-type", from}}
			: json{{"expr-type", "lit-int"}, {"value", "0"}, {"value-type", from}};
		return {{"expr-type", "cmp"}, {"op", "!="}, {"left", src}, {"right", zero},
		        {"value-type", resolved}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	} else if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast) {
		return wrapConvert(src, registry_.toJson(target));
	}
	incompatible(resolved);
	return json(); // LCOV_EXCL_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::resolveEnumerator(const json& expr)
{
	string typeName = typeNameWritten(expr["object"]);
	if (typeName.empty()) return json();
	requireWritableTypeName(expr, typeName);
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
	return makeFieldAccess(chain, findFieldOrExit(chain.structName, fn, expr));
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
	requireOwnedBase(sa_array, arr);
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
		// The element is the struct's storage, like a struct stored in an
		// array; writes are checked against p's own permission.
		int64_t stride = requireCompleteStruct(elem_type["type-name"].get<string>(), expr).totalSize;
		json elem_pntr = {{"type-kind","pntr"},{"base-type",elem_type}};
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
		// `void` has no size; unlike E_UnknownType below (an unrecognized
		// name), this pointee name IS recognized -- it's just not indexable.
		// Covers read (p[i]), write (v -> p[i] via sa_arr_assign_stmt), and
		// element-address-of (@p[i], reached before its own guard runs).
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_DerefVoidPointer) << endl;
		exit(1);
	}
	int sz = (elem_type.value("type-kind","") == "pntr") ? 8
		: elemSizeBytes(elem_type.value("type-name",""));
	if (sz < 0) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownType, elem_type.value("type-name","")) << endl;
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
	const json* pFunc = findCFunc(name);
	isCFunc = pFunc != nullptr;
	if (!pFunc) pFunc = findPlnFunc(name);
	if (!pFunc) pFunc = findImportFunc(name);
	return pFunc && !pFunc->empty() ? pFunc : nullptr;
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
	const json* pFunc = findImportFuncByAlias(call["object"]["name"].get<string>(), call["method"].get<string>());
	if (pFunc) isCFunc = pFunc->value("_c-func", false);
	return pFunc;
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
	return sa_expression(obj).value("value-type", json());
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
		if (vt.is_null()) return value;
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
		return sa_expression(norm);

	enterScope();
	json body = json::array();
	json saCall = sa_expression(chainMultiCall(norm, body));
	size_t idx = arrayScopeVars_.size() - 1;
	json release = collectFreeStmts(idx, idx + 1);
	leaveScope();
	return makeTempExpr(move(body), move(saCall), move(release));
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
