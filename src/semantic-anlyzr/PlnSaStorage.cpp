/// Palan Semantic Analyzer — storage types, allocation, release and copy
///
/// @file PlnSaStorage.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <functional>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"
#include "../common/PlnDefs.h"

// LCOV_EXCL_EXCEPTION_BR_START
static json lenLit(int64_t v)
{
	return {{"expr-type","lit-int"},{"value",to_string(v)},{"value-type",{{"type-kind","prim"},{"type-name","int64"}}}};
}

static json byteLit(int64_t v)
{
	return {{"expr-type","lit-uint"},{"value",to_string(v)},{"value-type",{{"type-kind","prim"},{"type-name","uint64"}}}};
}

static json mulLen(json l, json r)
{
	int64_t a = constArrSize(l), b = constArrSize(r);
	if (a >= 0 && b >= 0) return lenLit(a * b);
	return {{"expr-type","mul"},{"value-type",{{"type-kind","prim"},{"type-name","uint64"}}},
	        {"left",move(l)},{"right",move(r)}};
}
// LCOV_EXCL_EXCEPTION_BR_STOP

json PlnSemanticAnalyzer::toStructPntrType(const json& type) const
{
	if (!isStructType(type)) return type;
	// LCOV_EXCL_EXCEPTION_BR_START
	string name = type["type-name"].get<string>();
	return {{"type-kind","pntr"},
	        {"base-type",{{"type-kind","struct"},{"type-name",name}}}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::normalizeSigType(const json& type) const
{
	return isStructType(type) ? toStructPntrType(type) : deepNormalizePrimToStruct(type);
}

// The innermost element under an array type's levels as written.
static const json& astLeaf(const json& type)
{
	const json* t = &type;
	while (t->value("type-kind","") == "arr" && !isVarrAstType(*t)) t = &(*t)["base-type"];
	return *t;
}

// A struct element gets the same representation as in the sized array: its own
// storage in `[]T`, laid out with its stride in `[]$T`.
json PlnSemanticAnalyzer::unsizedArrToPntr(const json& locNode, const json& type)
{
	if (type.value("type-kind","") != "arr" || type.value("specifier","") != "raw"
	    || !type["size-expr"].is_null())
		return type;
	if (isVarrAstType(astLeaf(type["base-type"]))) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsizedVarrElemArr) << endl;
		exit(1);
	}
	json bt = type["base-type"];
	bool sizedRow = bt.value("type-kind","") == "arr" && !bt["size-expr"].is_null();
	if (sizedRow && astLeaf(bt).value("type-kind","") == "pntr") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsizedArrRowUnsupported) << endl;
		exit(1);
	}
	if (type.value("embedded", false)) {
		json pntr = {{"type-kind","pntr"},{"embedded",true}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if (isStructType(bt)) {
			string tname = bt["type-name"].get<string>();
			// LCOV_EXCL_EXCEPTION_BR_START
			pntr["stride"]    = requireCompleteStruct(tname, locNode).totalSize;
			pntr["base-type"] = {{"type-kind","struct"},{"type-name",tname}};
			// LCOV_EXCL_EXCEPTION_BR_STOP
			return pntr;
		}
		string et = sizedRow ? bt["size-expr"].value("expr-type","") : "";
		if (et == "lit-int" || et == "lit-uint") {
			json row = sizedArrLevel(locNode, bt, nullopt);
			pntr["stride"] = inPlaceSize(row);
			pntr["base-type"] = move(row);
			return pntr;
		}
		// []$[]T or []$[var]T: no stride -> validateEmbeddedParams reports it
		pntr["base-type"] = {{"type-kind","pntr"},{"base-type",bt.value("base-type", json{})}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		return pntr;
	}
	if (sizedRow)
		return {{"type-kind","pntr"}, {"base-type", sizedArrLevel(locNode, bt, nullopt)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	json elem = isStructType(bt) ? toStructPntrType(bt)
	          : bt.value("type-kind","") == "pntr" ? ptrSlotElemType(locNode, bt)
	          : unsizedArrToPntr(locNode, type["base-type"]);
	return {{"type-kind","pntr"}, {"base-type", elem}}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A slot to an unsized array ('@![]T') points at the array's elements, so it
// is that array's pointer carrying the slot's permission.
json PlnSemanticAnalyzer::ptrSlotElemType(const json& locNode, const json& type)
{
	const json& target = type["base-type"];
	json elem = target.value("type-kind","") == "arr"
		? unsizedArrToPntr(locNode, target)
		: json{{"type-kind","pntr"},{"base-type",target}};
	elem["mutable"] = type.value("mutable", false);
	return deepNormalizePrimToStruct(elem);
} // LCOV_EXCL_EXCEPTION_BR_LINE

int64_t PlnSemanticAnalyzer::constLevelSize(const json& locNode, const json& sizeExprAst, bool isBorrow)
{
	int64_t n = sizeExprAst.is_null() ? -1 : constArrSize(sa_arr_size_expr(locNode, sizeExprAst));
	if (n < 0) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(
			isBorrow ? E_ArrBorrowSizeNotConst : E_UnsizedArrRowSizeNotConst) << endl;
		exit(1);
	}
	return n;
}

// A sized array level as the variable holding it is typed, which a borrow of it
// shares except that every level carries the borrow's permission. Given
// `lens`, the levels are a variable's own: a length may be known only at run
// time, which the type then leaves out, and each level's length is appended
// to `lens`, outermost first.
json PlnSemanticAnalyzer::sizedArrLevel(const json& locNode, const json& arr, optional<bool> isMutable,
                                        vector<json>* lens)
{
	json out = {{"type-kind","pntr"}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	if (lens) {
		json size = sa_arr_size_expr(locNode, arr["size-expr"]);
		setArrSize(out, size);
		lens->push_back(move(size));
	} else {
		out["arr-size"] = constLevelSize(locNode, arr["size-expr"], isMutable.has_value());
	}
	if (isMutable) out["mutable"] = *isMutable;
	json leaf = arr["base-type"];
	// [n][+]T: each element stays the `[+]T` the array owns, as a struct element does.
	if (isVarrAstType(leaf)) {
		json elem = varrElemType(locNode, leaf);
		if (arr.value("embedded", false)) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedOwningType, typeDisplayName(elem)) << endl;
			exit(1);
		}
		out["base-type"] = move(elem);
		return out;
	}
	if (arr.value("embedded", false)) {
		out["embedded"] = true;
		if (leaf.value("type-kind","") != "arr") {
			// [n]$T: the structs themselves are laid out in the array, as in the variable.
			string tname = leaf.value("type-name","");
			const StructDef& def = requireCompleteStruct(tname, locNode);
			if (!isMutable && def.ownsFields) {
				cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedOwningType, tname) << endl;
				exit(1);
			}
			// LCOV_EXCL_EXCEPTION_BR_START
			out["stride"]    = def.totalSize;
			out["base-type"] = {{"type-kind","struct"},{"type-name",tname}};
			// LCOV_EXCL_EXCEPTION_BR_STOP
			return out;
		}
		json row = sizedArrLevel(locNode, leaf, isMutable, lens);
		// A row of a length known only at run time has no stride.
		int64_t stride = inPlaceSize(row);
		if (stride >= 0) out["stride"] = stride;
		out["base-type"] = move(row);
		return out;
	} else if (leaf.value("type-kind","") == "arr") {
		out["base-type"] = sizedArrLevel(locNode, leaf, isMutable, lens);
		return out;
	}
	if (lens && leaf.value("type-kind","") == "pntr") {
		out["base-type"] = ptrSlotElemType(locNode, leaf);
		return out;
	}
	// [n]@T / [n]@!T: the element pointer keeps its own permission.
	json ptrElem;
	if (leaf.value("type-kind","") == "pntr" && !out.value("embedded", false)) {
		ptrElem = {{"type-kind","pntr"},{"mutable",leaf.value("mutable", true)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		json pointee = leaf["base-type"];
		leaf = move(pointee);
	}
	string tname = leaf.value("type-name","");
	bool isStruct = structDefs_.count(tname) > 0;
	if (leaf.value("type-kind","") != "prim") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrBorrowUnsupportedElem) << endl;
		exit(1);
	}
	// An owned struct element is built by its allocator, which needs its layout.
	if (!isMutable && isStruct) requireCompleteStruct(tname, locNode);
	// LCOV_EXCL_EXCEPTION_BR_START
	if (!ptrElem.is_null()) {
		ptrElem["base-type"] = isStruct ? json{{"type-kind","struct"},{"type-name",tname}} : leaf;
		out["base-type"] = move(ptrElem);
	} else {
		out["base-type"] = isStruct ? toStructPntrType(leaf) : leaf;
	}
	// LCOV_EXCL_EXCEPTION_BR_STOP
	return out;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A type is resolved before the variables of its block are declared, so a
// field's length can only name a const.
int64_t PlnSemanticAnalyzer::fieldArrSize(const json& locNode, const json& sizeExpr)
{
	if (const json* id = firstNonConstName(sizeExpr)) {
		if (id->contains("c-macro")) undefinedNameError(locNode, *id, E_UndefinedVariable);
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrFieldSizeNotConstant,
			(*id)["name"].get<string>()) << endl;
		exit(1);
	}
	int64_t n = constArrSize(sa_arr_size_expr(locNode, sizeExpr));
	if (n < 0) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrFieldSizeNotConstant) << endl;
		exit(1);
	}
	return n;
}

// An owned array field holds its block as a variable of its type does. Its
// levels are checked first, so that their errors are the field's own.
json PlnSemanticAnalyzer::ownedFieldType(const json& locNode, const json& vtype)
{
	auto unsupported = [&] {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
		exit(1);
	};
	if (isVarrAstType(vtype)) unsupported();
	requireKnownTypeNames(locNode, vtype);
	const json* t = &vtype;
	for (; t->value("type-kind","") == "arr" && !isVarrAstType(*t); t = &(*t)["base-type"]) {
		if ((*t)["size-expr"].is_null()) unsupported();
		fieldArrSize(locNode, (*t)["size-expr"]);
	}
	if (t->value("type-kind","") == "pntr") unsupported();
	return sizedArrLevel(locNode, vtype, nullopt);
}

json PlnSemanticAnalyzer::normalizeArrBorrowType(const json& locNode, const json& type)
{
	if (type.value("type-kind","") != "pntr" || type["base-type"].value("type-kind","") != "arr")
		return type;
	const json& arr = type["base-type"];
	if (arr.value("specifier","") == "variable")
		return varrBorrowType(locNode, arr, type.value("mutable", false));
	return sizedArrLevel(locNode, arr, type.value("mutable", false));
}

// Registering the shape here gives the builtins and the generated functions
// to a function or module that takes or borrows a `[+]T` without declaring one.
json PlnSemanticAnalyzer::varrStorageType(const json& locNode, const json& arr)
{
	const json& base = arr["base-type"];
	string name = base.value("type-kind","") == "prim" ? base.value("type-name","") : "";
	bool embedded = arr.value("embedded", false);
	json elem;
	if (isVarrAstType(base)) {
		elem = varrElemType(locNode, base);
		if (embedded) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedOwningType, typeDisplayName(elem)) << endl;
			exit(1);
		}
	} else if (base.value("type-kind","") == "arr") {
		if (astLeaf(base).value("type-kind","") == "pntr") {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
			exit(1);
		}
		elem = sizedArrLevel(locNode, base, nullopt);
	} else if (!embedded && elemSizeBytes(name) >= 0) {
		elem = base;
	} else if (structDefs_.count(name)) {
		const StructDef& def = requireCompleteStruct(name, locNode);
		if (embedded && def.ownsFields) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedOwningType, name) << endl;
			exit(1);
		}
		// LCOV_EXCL_EXCEPTION_BR_START
		elem = {{"type-kind","struct"},{"type-name",name}};
		if (!embedded) elem = {{"type-kind","pntr"},{"base-type",elem}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	} else {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
		exit(1);
	}
	json vt = varrPntrType(elem, embedded);
	recordVarrShape(vt);
	return vt;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The bytes an element takes when it is laid out in place: a primitive or a
// slot, a struct, or a row of them, whose own rows may be laid out in place
// in turn; -1 when a row's length is known only at run time.
int64_t PlnSemanticAnalyzer::inPlaceSize(const json& elem) const
{
	string kind = elem.value("type-kind","");
	if (kind == "struct") return structDefs_.at(elem["type-name"].get<string>()).totalSize;
	if (kind == "prim") return elemSizeBytes(elem["type-name"].get<string>());
	if (!isArrLevel(elem)) return 8;
	if (!elem.contains("arr-size")) return -1;
	const json& child = elem["base-type"];
	int64_t childSize = !elem.value("embedded", false) && isArrLevel(child) ? 8 : inPlaceSize(child);
	return childSize < 0 ? -1 : elem["arr-size"].get<int64_t>() * childSize;
}

// The same as an expression, for an element at level `level` of an array
// whose level k is lenAt(k) long when its type does not say.
json PlnSemanticAnalyzer::inPlaceSizeExpr(const json& elem, size_t level,
                                          const function<json(size_t)>& lenAt) const
{
	int64_t size = inPlaceSize(elem);
	if (size >= 0) return byteLit(size);
	json n = elem.contains("arr-size") ? lenLit(elem["arr-size"].get<int64_t>()) : lenAt(level);
	const json& child = elem["base-type"];
	json childSize = !elem.value("embedded", false) && isArrLevel(child) ? byteLit(8)
	               : inPlaceSizeExpr(child, level + 1, lenAt);
	return mulLen(move(n), move(childSize));
}

// Its alignment: that of the first element laid out in it.
int PlnSemanticAnalyzer::inPlaceAlign(const json& elem) const
{
	string kind = elem.value("type-kind","");
	if (kind == "struct") return structDefs_.at(elem["type-name"].get<string>()).maxAlign;
	if (kind == "prim") return elemSizeBytes(elem["type-name"].get<string>());
	if (!isArrLevel(elem) || (!elem.value("embedded", false) && isArrLevel(elem["base-type"]))) return 8;
	return inPlaceAlign(elem["base-type"]);
}

// A `[+]U` held as an element starts empty, so it takes no size.
json PlnSemanticAnalyzer::varrElemType(const json& locNode, const json& varr)
{
	if (!varr["size-expr"].is_null()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_VarrElemSized) << endl;
		exit(1);
	}
	return varrStorageType(locNode, varr);
}

json PlnSemanticAnalyzer::varrBorrowType(const json& locNode, const json& arr, bool isMutable)
{
	if (!arr["size-expr"].is_null()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_VarrBorrowSized) << endl;
		exit(1);
	}
	json vt = varrStorageType(locNode, arr);
	vt["mutable"] = isMutable;
	return vt;
}

// A declared array variable's storage: its type, and the lengths of its own
// levels (the array and, for `[n][m]T`, its rows; a `[+n]T`'s capacity),
// outermost first.
Storage PlnSemanticAnalyzer::arrVarStorage(const json& locNode, const json& vtype)
{
	Storage st;
	if (isVarrAstType(vtype)) {
		st.type = varrStorageType(locNode, vtype);
		if (!vtype["size-expr"].is_null()) st.lens.push_back(sa_arr_size_expr(locNode, vtype["size-expr"]));
		return st;
	}
	if (vtype["size-expr"].is_null()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsizedArrVarDecl) << endl;
		exit(1);
	}
	st.type = sizedArrLevel(locNode, vtype, nullopt, &st.lens);
	return st;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The block a declared `[+]T` or `[n]T` owns, or null for any other type:
// what a call result must be for the variable to take it over.
json PlnSemanticAnalyzer::takeOverType(const json& locNode, const json& varType)
{
	if (varType.value("type-kind","") != "arr") return json();
	bool sized = !varType["size-expr"].is_null();
	if (isVarrAstType(varType) ? sized : !sized) return json();
	Storage st = arrVarStorage(locNode, varType);
	for (auto& len : st.lens)
		if (constArrSize(len) < 0) return json();
	return st.type;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A parameter's or return value's type as the function and its callers see it.
// Only a parameter may borrow an array: a returned borrow would outlive the
// storage it borrows, so it stays for validateNativeSig to reject.
json PlnSemanticAnalyzer::sigType(const json& locNode, const json& type, bool isParam)
{
	json t = unsizedArrToPntr(locNode, type);
	if (isParam) t = normalizeArrBorrowType(locNode, t);
	if (isVarrAstType(t)) {
		t = varrStorageType(locNode, t);
	} else if (t.value("type-kind","") == "arr") {
		t = takeOverType(locNode, t);
		if (t.is_null()) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_SigArrSizeNotConst) << endl;
			exit(1);
		}
	}
	return normalizeSigType(t);
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::normalizeSig(json& funcDef)
{
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			p["var-type"] = sigType(funcDef, p["var-type"], true);
	if (funcDef.contains("rets"))
		for (auto& r : funcDef["rets"])
			r["var-type"] = sigType(funcDef, r["var-type"], false);
	if (funcDef.contains("ret-type"))
		funcDef["ret-type"] = sigType(funcDef, funcDef["ret-type"], false);
}

// An element's type as its generated functions see it: they take the lengths
// of its levels as arguments, so one layout shares them whatever the lengths.
// A struct laid out in place keeps its stride, the struct's size.
json shapeElem(const json& elem)
{
	json out = elem;
	if (isArrLevel(out)) {
		out.erase("arr-size");
		if (isArrLevel(out["base-type"])) out.erase("stride");
	}
	if (out.contains("base-type") && out["base-type"].is_object())
		out["base-type"] = shapeElem(out["base-type"]);
	return out;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The levels of an element, outermost first and on through the elements of
// a `[+]U`, whose lengths its generated functions take after their own
// arguments.
static vector<const json*> helperLevels(const json& elem)
{
	vector<const json*> levels;
	for (const json* t = &elem;;) {
		if (isVarrStorage(*t)) {
			t = &(*t)["base-type"]["base-type"];
		} else if (isArrLevel(*t)) {
			levels.push_back(t);
			t = &(*t)["base-type"];
		} else {
			return levels;
		}
	}
}

json helperLens(const json& elem)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json lens = json::array();
	for (const json* level : helperLevels(elem))
		lens.push_back(lenLit((*level)["arr-size"].get<int64_t>()));
	return lens;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A family taking any element names it by the symbol of the element's layout
// (layoutElem), so element types laid out alike share their helpers.
string allocShapeKey(const AllocShapeRef& shape)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	switch (shape.family) {
	case AllocFamily::Struct:       return shape.elem["type-name"].get<string>();
	case AllocFamily::Arr:          return "arr_" + symbolType(shapeElem(shape.elem));
	case AllocFamily::Varr:         return "varr_" + symbolType(shapeElem(shape.elem));
	case AllocFamily::EVarr:        return "evarr_" + symbolType(shapeElem(shape.elem));
	}
	return ""; // LCOV_EXCL_LINE -- every family is handled above
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

namespace {
// One element's types in the signatures build-mgr writes for its functions
// (its ElemOps), as palan-sa normalizes them: the slot as the drop and copy
// helpers read it (own) and as the build helpers fill it (build), what push
// takes (value), the source run copy takes (run), and a run of the slots a
// row block holds (slots), filled through `fill`. A row laid out in place
// stands for its elements' slots.
struct GenElem { json type, own, build, value, run, slots, fill; bool owned = false; };

json ptrTo(json base) { return {{"type-kind","pntr"},{"base-type",move(base)}}; }
json permPtrTo(json base, bool isMutable)
{
	return {{"type-kind","pntr"},{"mutable",isMutable},{"base-type",move(base)}};
}

// vt's element is laid out already.
AllocShapeRef laidOutVarrShape(const json& vt)
{
	return {isVarrEmbedded(vt) ? AllocFamily::EVarr : AllocFamily::Varr, vt["base-type"]["base-type"]};
}

// stride is the size of a struct laid out in place, which its level carries.
GenElem genElem(const json& e, bool inPlace, int64_t stride)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	GenElem g;
	if (isArrLevel(e) && inPlace) {
		const json* cur = &e;
		while (cur->value("embedded", false) && isArrLevel((*cur)["base-type"])) cur = &(*cur)["base-type"];
		GenElem l = genElem((*cur)["base-type"], cur->value("embedded", false), cur->value("stride", (int64_t)0));
		g = l;
		g.value = l.slots;
		g.run = l.owned ? ptrTo(l.own) : permPtrTo(l.type, false);
		return g;
	}
	if (isArrLevel(e)) {
		GenElem l = genElem(e["base-type"], e.value("embedded", false), e.value("stride", (int64_t)0));
		g.owned = true;
		g.own = g.value = l.slots;
		g.build = permPtrTo(l.fill, true);
	} else if (isVarrStorage(e)) {
		g.owned = true;
		g.type = {{"type-kind","struct"},{"type-name","__pln_" + allocShapeKey(laidOutVarrShape(e))}};
		g.own = g.value = ptrTo(g.type);
		g.build = permPtrTo(g.type, true);
	} else if (isStructStorage(e)) {
		g.owned = true;
		g.type = e["base-type"];
		g.own = g.value = e;
		g.build = permPtrTo(g.type, true);
	} else if (e.value("type-kind","") == "struct") {
		g.type = g.own = e;
		g.value = ptrTo(e);
		g.build = permPtrTo(e, true);
		g.slots = {{"type-kind","pntr"},{"embedded",true},{"stride",stride},{"base-type",e}};
	} else {
		g.type = g.own = g.value = e;
		g.build = permPtrTo(e, true);
	}
	if (g.slots.is_null()) g.slots = ptrTo(g.own);
	g.run = g.owned ? ptrTo(g.own) : permPtrTo(g.type, false);
	g.fill = g.owned ? g.build : g.type;
	return g;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE
} // namespace

string allocHelperSymbol(const AllocShapeRef& shape, const string& op)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	vector<json> params;
	if (shape.family == AllocFamily::Struct) {
		if (op == "init") params.push_back(permPtrTo(shape.elem, true));
		else if (op == "copy") params = {ptrTo(shape.elem), ptrTo(shape.elem)};
		else if (op != "alloc") params.push_back(ptrTo(shape.elem));
	} else {
		bool isVarr = shape.family != AllocFamily::Arr;
		GenElem g = genElem(shapeElem(shape.elem), shape.family == AllocFamily::EVarr, 0);
		if (!isVarr) {
			// The array's own slots, and their count.
			if (op == "init") params.push_back(ptrTo(g.build));
			else if (op == "copy") params = {ptrTo(g.own), ptrTo(g.own)};
			else if (op != "alloc") params.push_back(ptrTo(g.own));
			params.push_back(i64);
		} else {
			// The generated module sees the header as a struct of its own.
			json header = {{"type-kind","struct"},{"type-name","__pln_" + allocShapeKey(shape)}};
			if (op == "fini" || op == "free") params.push_back(ptrTo(header));
			else if (op != "alloc") params.push_back(permPtrTo(header, true));
			if (op == "alloc" || op == "init") params.insert(params.end(), {i64, i64});   // len, cap
			else if (op == "push") params.push_back(g.value);
			else if (op == "resize") params.push_back(i64);
			else if (op == "copy") params.insert(params.end(), {g.run, i64});
		}
		for (size_t i = 0; i < helperLevels(shape.elem).size(); i++) params.push_back(i64);
	}

	json jparams = json::array();
	for (auto& t : params) jparams.push_back({{"var-type", t}});
	string name = "__pln_" + op + "_" + allocShapeKey(shape);
	return moduleSymbol(PALAN_ALLOCATORS_MODULE) + ".." + funcSymbol(name, jparams);
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A `[+]T`'s functions also take the lengths its element's type gives.
json allocHelperCall(const AllocShapeRef& shape, const string& op, json args)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	if (shape.family == AllocFamily::Varr || shape.family == AllocFamily::EVarr)
		for (auto& len : helperLens(shape.elem)) args.push_back(len);
	return {{"expr-type","call"},{"name",allocHelperSymbol(shape, op)},
	        {"func-type","palan"},{"args",move(args)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::recordAllocShape(const string& name)
{
	if (allocShapeNames_.count(name)) return;
	allocShapeNames_.insert(name);

	// Only declared, for the helpers of its elements.
	auto bytes = byteTypes_.find(name);
	if (bytes != byteTypes_.end()) {
		sa["alloc-shapes"].push_back({ // LCOV_EXCL_EXCEPTION_BR_LINE
			{"shape-kind","bytes"}, {"shape-name",name}, {"total-size",bytes->second}
		});
		return;
	}
	const StructDef& def = structDefs_[name];
	// An owned field is a slot holding an element, built, dropped and copied
	// as an array's is; its helpers take its lengths.
	json owned = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "owned") continue;
		// LCOV_EXCL_EXCEPTION_BR_START
		json lens = json::array();
		for (const json* level : helperLevels(f.type))
			lens.push_back((*level)["arr-size"]);
		owned.push_back({
			{"name",   f.name},
			{"offset", f.offset},
			{"elem",   allocElemDesc(layoutElem(f.type))},
			{"lens",   move(lens)}
		});
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	sa["alloc-shapes"].push_back({
		{"shape-kind",   "struct"},
		{"shape-name",   name},
		{"total-size",   def.totalSize},
		{"owned-fields", move(owned)}
	});
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// One element as the array's storage lays it out, as build-mgr builds, frees
// and copies it. A struct's shape is recorded first, so build-mgr sees an
// element's helpers before the array's.
json PlnSemanticAnalyzer::allocElemDesc(const json& elem, bool inPlace)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	if (elem.value("type-kind","") == "prim") {
		string name = elem["type-name"];
		return {{"kind","prim"},{"key",symbolType(elem)},{"type",name},{"size",elemSizeBytes(name)}};
	}
	// A row is its own block (`[+][n]T`, `[m][n]T`) or laid out in the slot
	// (`[+]$[n]T`), whose elements are then the slot's; the two build
	// differently, so their helpers need different names. Rows laid out in
	// place in turn (`[+]$[n]$[m]T`) are one run of `depth` levels. The
	// helpers take each level's length, so it is not in their name.
	if (isArrLevel(elem)) {
		if (!inPlace) {
			json leaf = allocElemDesc(elem["base-type"], elem.value("embedded", false));
			return {{"kind","row"},{"key",symbolType(shapeElem(elem))},{"size",8},{"elem",move(leaf)}};
		}
		int depth = 1;
		const json* cur = &elem;
		for (; cur->value("embedded", false) && isArrLevel((*cur)["base-type"]); depth++) cur = &(*cur)["base-type"];
		json leaf = allocElemDesc((*cur)["base-type"], cur->value("embedded", false));
		return {{"kind","embed-row"},{"key","C" + symbolType(shapeElem(elem))},{"depth",depth},{"elem",move(leaf)}};
	}
	if (isStructStorage(elem)) {
		string name = elem["base-type"]["type-name"];
		recordAllocShape(name);
		return {{"kind","struct"},{"key",symbolType(elem)},{"name",name},{"size",8}};
	}
	if (isVarrStorage(elem)) {
		AllocShapeRef shape = laidOutVarrShape(elem);
		recordElemShape(shape);
		string key = allocShapeKey(shape);
		return {{"kind","varr"},{"key",symbolType(shapeElem(elem))},{"name","__pln_" + key},{"shape-key",key},{"size",8}};
	}
	string name = elem["type-name"];
	recordAllocShape(name);
	// A struct laid out in place owns nothing, so it is bytes.
	return {{"kind","embed-struct"},{"key",symbolType(elem)},{"name",name},{"size",byteTypes_.at(name)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The type an element is laid out as, which names its helpers, so that types
// laid out alike share them. An integer is stored and passed by its width
// alone: whoever reads one extends it as its own type says. A struct owning
// nothing is only bytes to allocate, copy and free.
json PlnSemanticAnalyzer::layoutElem(const json& elem)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	static const map<string, string> sameWidthInt = {
		{"uint8","int8"}, {"bool","int8"}, {"uint16","int16"}, {"uint32","int32"}, {"uint64","int64"}
	};
	json out = elem;
	string kind = out.value("type-kind","");
	if (kind == "prim") {
		out.erase("enum");
		auto it = sameWidthInt.find(out["type-name"].get<string>());
		if (it != sameWidthInt.end()) out["type-name"] = it->second;
		return out;
	}
	if (kind == "struct") {
		const StructDef& def = structDefs_.at(out["type-name"].get<string>());
		if (def.ownsFields) return out;
		string bytes = "__pln_B" + to_string(def.totalSize);
		byteTypes_[bytes] = def.totalSize;
		out["type-name"] = bytes;
		return out;
	}
	if (out.contains("base-type") && out["base-type"].is_object())
		out["base-type"] = layoutElem(out["base-type"]);
	return out;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

AllocShapeRef PlnSemanticAnalyzer::varrShape(const json& vt)
{
	return laidOutVarrShape(layoutElem(vt));
}

void PlnSemanticAnalyzer::recordElemShape(const AllocShapeRef& shape)
{
	string shape_key = allocShapeKey(shape);
	for (auto& s : sa["alloc-shapes"])
		if (s.value("shape-key","") == shape_key) return;
	json desc = allocElemDesc(shape.elem, shape.family == AllocFamily::EVarr);
	sa["alloc-shapes"].push_back({ // LCOV_EXCL_EXCEPTION_BR_LINE
		{"shape-kind", shape.family == AllocFamily::Arr ? "arr" : "varr"}, {"shape-key",shape_key}, {"elem",move(desc)}
	});
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::recordArrShape(const json& elem)
{
	recordElemShape(arrShape(elem));
}

// `[+][3]T` and `[+][4]T` share their shape but not their builtins, which
// take different rows and pass different lengths; `[+]uint32` shares that of
// `[+]int32` but has builtins of its own types.
void PlnSemanticAnalyzer::recordVarrShape(const json& vt)
{
	if (!varrBuiltinTypes_.insert(symbolType(vt)).second) return;
	AllocShapeRef shape = varrShape(vt);
	for (auto& def : varrBuiltinDefs(vt["base-type"], &shape))
		varrBuiltins_[def["name"].get<string>()].push_back(def);
	recordElemShape(shape);
}

// push takes an element by value and pop returns one; a struct goes as its
// storage, copied in by push and owned by pop's caller.
json varrBuiltinDefs(const json& varr, const AllocShapeRef* impl)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	const json& elem = varr["base-type"];
	bool embedded = varr.value("embedded", false);
	// A row is pushed from a borrow of one and popped as an owned array.
	json value = elem.value("type-kind","") == "struct" ? json{{"type-kind","pntr"},{"base-type",elem}}
	           : isArrLevel(elem) ? withArrPermission(elem, false) : elem;
	const json& popped = isArrLevel(elem) ? elem : value;
	auto self = [&](bool isMutable) {
		json vt = varrPntrType(elem, embedded);
		vt["mutable"] = isMutable;
		return json{{"name","v"},{"var-type",move(vt)}};
	};
	json defs = json::array();
	auto add = [&](const string& name, json params, const json* ret) {
		json def = {{"name",name},{"func-type","palan"},{"parameters",move(params)}};
		if (name == "len") {
			def["inline"] = true;
		} else if (impl) {
			def["symbol"] = allocHelperSymbol(*impl, name);
			def["len-args"] = helperLens(impl->elem);
		}
		if (ret) def["ret-type"] = *ret;
		defs.push_back(move(def));
	};
	add("len", json::array({self(false)}), &i64);
	add("push", json::array({self(true), {{"name","x"},{"var-type",value}}}), nullptr);
	add("resize", json::array({self(true), {{"name","n"},{"var-type",i64}}}), nullptr);
	add("pop", json::array({self(true)}), &popped);
	return defs;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The lengths of an array's levels, outermost first and on through its
// elements' types: `known` for its own levels, which may be known only at run
// time, and the type's for the rest.
static json arrLens(const json& vt, const vector<json>& known = {})
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json lens = json::array();
	const json* t = &vt;
	for (auto& len : known) {
		int64_t n = constArrSize(len);
		lens.push_back(n >= 0 ? lenLit(n) : len);
		t = &(*t)["base-type"];
	}
	for (auto& len : helperLens(*t)) lens.push_back(len);
	return lens;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// An owned array's elements as its helpers take them, and the lengths they
// take: rows laid out in place are one run of their elements' slots, so
// `[2]$[3]S` is a run of 6 struct slots.
static pair<json, json> ownedRun(const json& vt, const json& lens)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json* t = &vt;
	json n = lens[0];
	size_t k = 1;
	for (; t->value("embedded", false) && isArrLevel((*t)["base-type"]); k++) {
		t = &(*t)["base-type"];
		n = mulLen(move(n), lens[k]);
	}
	json args = json::array({move(n)});
	for (; k < lens.size(); k++) args.push_back(lens[k]);
	return {(*t)["base-type"], move(args)};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::makeOwnedFreeStmt(const string& name, const json& pntrType)
{
	json var_id = {{"expr-type","id"},{"name",name},{"var-type",pntrType},{"value-type",pntrType}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return makeOwnedValueFreeStmt(move(var_id));
} // LCOV_EXCL_EXCEPTION_BR_LINE

// What an owned struct, array or row holds, freed the way its owner's
// __pln_free_* would. Every free here accepts NULL. An array's helpers take
// its levels' lengths, `lens` (as arrLens gives them) when some are known
// only at run time.
json PlnSemanticAnalyzer::makeOwnedValueFreeStmt(json value, json lens)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json vt = value["value-type"];
	json args = json::array({move(value)});
	auto freeCall = [&] {
		return json{{"stmt-type","expr"},{"body",{
			{"expr-type","call"},{"name","free"},{"func-type","c"},{"args",move(args)}}}};
	};
	if (isStructStorage(vt)) {
		string name = vt["base-type"]["type-name"];
		if (!structDefs_.at(name).ownsFields) return freeCall();
		recordAllocShape(name);
		return {{"stmt-type","expr"},{"body",allocHelperCall(structShape(name), "free", move(args))}};
	}
	if (isVarrStorage(vt))
		return {{"stmt-type","expr"},{"body",allocHelperCall(varrShape(vt), "free", move(args))}};
	if (!ownsElems(vt)) return freeCall();
	auto [elem, lenArgs] = ownedRun(vt, lens.is_null() ? arrLens(vt) : lens);
	recordArrShape(elem);
	for (auto& len : lenArgs) args.push_back(len);
	return {{"stmt-type","expr"},{"body",allocHelperCall(arrShape(elem), "free", move(args))}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::makeCopyStmt(const json& locNode, const json& dst, const json& src)
{
	const json& t = dst["value-type"];
	const json& st = src["value-type"];
	// A lone struct may be copied from a '@T' as well as from another struct's
	// storage; a `[+]T` takes any run of its elements and their count.
	bool match = isStructStorage(t) ? isStructPntr(st) && st["base-type"] == t["base-type"]
	           : copySourceMatch(st, t);
	if (!match) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyShapeMismatch,
			typeDisplayName(t), typeDisplayName(st)) << endl;
		exit(1);
	}

	// A struct returned by a Palan call is owned by nobody yet: receive it in a
	// temp, copy from it like from a named value, and free it.
	if (isExpiringOwned(src)) {
		// LCOV_EXCL_EXCEPTION_BR_START
		string temp = "__cpy_" + to_string(tempVarCounter_++);
		json decl = {{"stmt-type","var-decl"},
		             {"vars",json::array({{{"name",temp},{"var-type",st},{"init",src}}})}};
		json tempId = {{"expr-type","id"},{"name",temp},{"var-type",st},{"value-type",st}};
		json body = json::array({decl, makeCopyStmt(locNode, dst, tempId), makeOwnedFreeStmt(temp, st)});
		return {{"stmt-type","block"},{"body",move(body)}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	auto lit = [&](int64_t v) -> json {
		return {{"expr-type","lit-uint"},{"value",to_string(v)},{"value-type",uint64_type}};
	};
	auto copyStmt = [&](const AllocShapeRef& shape, json args) -> json {
		return {{"stmt-type","expr"},{"body",allocHelperCall(shape, "copy", move(args))}};
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

	if (isVarrStorage(t)) {
		json args = isVarrPntr(st) ? json::array({dst, varrData(src), varrLen(src)})
		                           : json::array({dst, src, lit(st["arr-size"].get<int64_t>())});
		return copyStmt(varrShape(t), move(args));
	}
	if (isStructStorage(t)) {
		string name = t["base-type"]["type-name"];
		if (!structHasOwned(name))
			return memcpyStmt(structDefs_[name].totalSize);
		recordAllocShape(name);
		return copyStmt(structShape(name), json::array({dst, src})); // LCOV_EXCL_EXCEPTION_BR_LINE
	}

	// An array is copied as its owner holds it, whatever permission a borrow gives it.
	json owned = withoutArrPermission(t);
	if (!ownsElems(owned))
		return memcpyStmt(inPlaceSize(owned));
	// LCOV_EXCL_EXCEPTION_BR_START
	auto [elem, lens] = ownedRun(owned, arrLens(owned));
	const json* leaf = &elem;
	while (isArrLevel(*leaf)) leaf = &(*leaf)["base-type"];
	if (isPtrBorrow(*leaf)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyUnsupportedShape, typeDisplayName(t)) << endl;
		exit(1);
	}
	recordArrShape(elem);
	json args = json::array({dst, src});
	for (auto& len : lens) args.push_back(len);
	return copyStmt(arrShape(elem), move(args));
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The heap and frame versions of an aggregate variable share one var-type and
// differ only in how the outermost block is obtained and released, so each
// handler describes the heap version and the frame version is derived from it:
// __pln_alloc_K(args) becomes __pln_init_K(v, args) on the frame block,
// and __pln_free_K becomes __pln_fini_K, which leaves the block itself alone.
// An empty `heapInit` means the frame block's contents come from elsewhere.
json PlnSemanticAnalyzer::placeAggregateVar(const json& stmt, const string& name, const json& varType,
                                            json heapInit, json heapRelease, VarStorage storage,
                                            int64_t frameBytes, int frameAlign, json& post,
                                            optional<AllocShapeRef> shape)
{
	if (storage == VarStorage::Heap) {
		addOwnedScopeVar(name, move(heapRelease));
		return heapInit;
	}
	if (frameBytes < 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_FrameArrSizeNotConst, name) << endl;
		exit(1);
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	json self = {{"expr-type","id"},{"name",name},{"var-type",varType},{"value-type",varType}};
	auto helperName = [&](const char* op) {
		return allocHelperSymbol(*shape, op);
	};
	if (shape && heapInit.value("name", "") == helperName("alloc")) {
		json args = json::array({self});
		for (auto& a : heapInit["args"]) args.push_back(a);
		post.push_back({{"stmt-type","expr"},{"body",allocHelperCall(*shape, "init", move(args))}});
	}
	json release;
	if (shape && heapRelease["body"].value("name", "") == helperName("free")) {
		release = move(heapRelease);
		release["body"]["name"] = helperName("fini");
	}
	arrayScopeVars_.back().push_back({name, move(release), false});
	return {{"expr-type","frame-alloc"},{"size",frameBytes},{"align",frameAlign},{"value-type",varType}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A struct variable's heap block, built by its allocator when it owns fields.
json PlnSemanticAnalyzer::structAllocExpr(const string& name)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json pntrType = {{"type-kind","pntr"},{"base-type",{{"type-kind","struct"},{"type-name",name}}}};
	const StructDef& def = structDefs_.at(name);
	json call;
	if (def.ownsFields) {
		recordAllocShape(name);
		call = allocHelperCall(structShape(name), "alloc", json::array());
	} else {
		json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
		json size_arg = {{"expr-type","lit-int"},{"value",to_string(def.totalSize)},{"value-type",uint64_type}};
		call = {{"expr-type","call"},{"name","malloc"},{"func-type","c"},{"args",json::array({size_arg})}};
	}
	call["value-type"] = move(pntrType);
	return call;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// How an array variable's own block is obtained and released, given its
// levels' lengths as its code reads them. Owned elements go through the
// generated helpers; anything else is laid out in the block itself.
AggregatePlace PlnSemanticAnalyzer::arrPlace(const json& self, const vector<json>& lens, bool litLen)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	const json& vt = self["value-type"];
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	auto mul = [&](json l, json r) {
		return json{{"expr-type","mul"},{"value-type",uint64_type},{"left",move(l)},{"right",move(r)}};
	};
	AggregatePlace place;
	if (isVarrStorage(vt)) {
		json zero = lenLit(0);
		json cap = lens.empty() ? zero : lens[0];
		place.shape = varrShape(vt);
		place.heapInit = allocHelperCall(*place.shape, "alloc", json::array({litLen ? cap : zero, cap}));
		place.frameBytes = 24;
		place.frameAlign = 8;
	} else if (ownsElems(vt)) {
		json levelLens = arrLens(vt, lens);
		auto [elem, args] = ownedRun(vt, levelLens);
		recordArrShape(elem);
		place.shape = arrShape(elem);
		int64_t n = constArrSize(args[0]);
		place.heapInit = allocHelperCall(*place.shape, "alloc", move(args));
		place.release = makeOwnedValueFreeStmt(self, move(levelLens));
		place.frameBytes = n < 0 ? -1 : n * 8;
		place.frameAlign = 8;
	} else {
		// Laid out in the block, which a length known only at run time sizes
		// at run time.
		int64_t n = vt.value("arr-size", (int64_t)-1);
		const json& elem = vt["base-type"];
		int64_t elemBytes = inPlaceSize(elem);
		json elemSize = inPlaceSizeExpr(elem, 1, [&](size_t k) { return lens[k]; });
		place.frameAlign = inPlaceAlign(elem);
		json size = elemBytes == 1 ? lens[0] : mul(lens[0], move(elemSize));
		place.heapInit = {{"expr-type","call"},{"name","malloc"},{"func-type","c"},{"args",json::array({move(size)})}};
		place.frameBytes = n < 0 || elemBytes < 0 ? -1 : n * elemBytes;
	}
	place.heapInit["value-type"] = vt;
	if (place.release.is_null()) place.release = makeOwnedValueFreeStmt(self);
	return place;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_var_decl(const json& stmt, VarStorage storage)
{
	return sa_arr_var_decl(stmt, storage, arrVarStorage(stmt, stmt["vars"][0]["var-type"]), false);
}

// The lengths are evaluated once for all the variables of a statement. Each
// variable keeps one known only at run time in __<name>_d<k> (level k), which
// a row laid out in place without a stride is also indexed by. With an array
// literal (`litLen`), a `[+]T`'s declared size is its length.
json PlnSemanticAnalyzer::sa_arr_var_decl(const json& stmt, VarStorage storage, const Storage& st, bool litLen)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	auto id = [](const string& name, const json& type) {
		return json{{"expr-type","id"},{"name",name},{"var-type",type},{"value-type",type}};
	};
	json result = json::array();
	string first;
	for (auto& var : stmt["vars"]) {
		string name = var["name"];
		vector<json> lens;
		for (size_t k = 0; k < st.lens.size(); k++) {
			if (constArrSize(st.lens[k]) >= 0) {
				lens.push_back(st.lens[k]);
				continue;
			}
			string lenName = "__" + name + "_d" + to_string(k);
			json init = first.empty() ? st.lens[k] : id("__" + first + "_d" + to_string(k), uint64_type);
			declareVar(lenName, uint64_type, &stmt);
			result.push_back({{"stmt-type","var-decl"},
				{"vars",json::array({{{"name",lenName},{"var-type",uint64_type},{"init",move(init)}}})}});
			lens.push_back(id(lenName, uint64_type));
		}
		if (first.empty()) first = name;
		AggregatePlace place = arrPlace(id(name, st.type), lens, litLen);
		declareVar(name, st.type, &stmt);
		json post = json::array();
		json init = placeAggregateVar(stmt, name, st.type, move(place.heapInit), move(place.release), storage,
		                              place.frameBytes, place.frameAlign, post, place.shape);
		result.push_back({{"stmt-type","var-decl"},
			{"vars",json::array({{{"name",name},{"var-type",st.type},{"init",move(init)}}})}});
		for (auto& s : post) result.push_back(move(s));
	}
	return result;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE
