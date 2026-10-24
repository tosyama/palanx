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

// A struct element gets the same representation as in the sized array: its own
// storage in `[]T`, laid out with its stride in `[]$T`.
json PlnSemanticAnalyzer::unsizedArrToPntr(const json& locNode, const json& type)
{
	if (type.value("type-kind","") != "arr" || type.value("specifier","") != "raw"
	    || !type["size-expr"].is_null())
		return type;
	if (isVarrAstType(type["base-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsizedVarrElemArr) << endl;
		exit(1);
	}
	if (type.value("embedded", false)) {
		json embed_bt = type["base-type"];  // [m]T or a struct
		json pntr = {{"type-kind","pntr"},{"embedded",true}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if (isStructType(embed_bt)) {
			string tname = embed_bt["type-name"].get<string>();
			// LCOV_EXCL_EXCEPTION_BR_START
			pntr["stride"]    = requireCompleteStruct(tname, locNode).totalSize;
			pntr["base-type"] = {{"type-kind","struct"},{"type-name",tname}};
			// LCOV_EXCL_EXCEPTION_BR_STOP
			return pntr;
		}
		bool isRow = embed_bt.value("type-kind","") == "arr";
		if (isRow && isStructType(embed_bt["base-type"])) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_Unsupported2DStructArr) << endl;
			exit(1);
		}
		json leaf = embed_bt.value("base-type", json{});
		json row = {{"type-kind","pntr"},{"base-type",leaf}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if (isRow && !embed_bt["size-expr"].is_null()) {
			const auto& sz = embed_bt["size-expr"];
			string et = sz.value("expr-type","");
			if (et == "lit-int" || et == "lit-uint") {
				int64_t m = stoll(sz["value"].get<string>());
				row["arr-size"] = m;
				pntr["stride"]  = m * elemSizeBytes(leaf.value("type-name",""));
			}
		}
		// []$[]T or []$[var]T: no stride -> validateEmbeddedParams reports it
		pntr["base-type"] = move(row);
		return pntr;
	}
	json bt = type["base-type"];
	if (bt.value("type-kind","") == "arr" && !bt["size-expr"].is_null()) {
		json leaf = bt["base-type"];
		string lk = leaf.value("type-kind","");
		if (lk == "arr" || lk == "pntr") {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsizedArrRowUnsupported) << endl;
			exit(1);
		}
		return {{"type-kind","pntr"}, {"base-type", sizedArrLevel(locNode, bt, nullopt)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
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
			if (lens && def.ownsFields) {
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
		const json& rowElem = row["base-type"];
		if (rowElem.value("type-kind","") != "prim") {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrBorrowUnsupportedElem) << endl;
			exit(1);
		}
		// A row of a length known only at run time has no stride.
		if (row.contains("arr-size"))
			out["stride"] = row["arr-size"].get<int64_t>() * elemSizeBytes(rowElem["type-name"].get<string>());
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
	if (leaf.value("type-kind","") != "prim" || (isStruct && out.value("embedded", false))) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrBorrowUnsupportedElem) << endl;
		exit(1);
	}
	// An owned struct element is built by its allocator, which needs its layout.
	if (lens && isStruct) requireCompleteStruct(tname, locNode);
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
		elem = varrRowType(locNode, base);
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

// The bytes an element takes when it is laid out in place: a struct, or a
// row of primitives, of slots, or of structs laid out in place in turn.
int64_t PlnSemanticAnalyzer::inPlaceSize(const json& elem) const
{
	if (elem.value("type-kind","") == "struct")
		return structDefs_.at(elem["type-name"].get<string>()).totalSize;
	if (elem.contains("stride")) return elem["arr-size"].get<int64_t>() * elem["stride"].get<int64_t>();
	const json& leaf = elem["base-type"];
	int64_t leafSize = leaf.value("type-kind","") == "prim" ? elemSizeBytes(leaf["type-name"].get<string>()) : 8;
	return elem["arr-size"].get<int64_t>() * leafSize;
}

// A row element of `[+][n]T` or `[+]$[n]T`: the same level as a row of
// `[m][n]T`. Only one fixed level is supported under `[+]`, and only
// primitive, struct and `[+]U` leaves.
json PlnSemanticAnalyzer::varrRowType(const json& locNode, const json& row)
{
	const json& leaf = row["base-type"];
	bool fixedLeaf = leaf.value("type-kind","") == "arr" && !isVarrAstType(leaf);
	if (fixedLeaf || leaf.value("type-kind","") == "pntr") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
		exit(1);
	}
	if (row.value("embedded", false) && structDefs_.count(leaf.value("type-name",""))) {
		string name = leaf["type-name"];
		if (requireCompleteStruct(name, locNode).ownsFields) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedOwningType, name) << endl;
			exit(1);
		}
	}
	return sizedArrLevel(locNode, row, nullopt);
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
	const json& base = vtype["base-type"];
	if (base.value("type-kind", "") == "arr" && isVarrAstType(base["base-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_VarrIn2DArr) << endl;
		exit(1);
	}
	bool fixedRows = base.value("type-kind", "") == "arr" && !isVarrAstType(base);
	if (fixedRows && base["base-type"].value("type-kind", "") == "arr" && !isVarrAstType(base["base-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
		exit(1);
	}
	if (vtype.value("embedded", false) && fixedRows && isStructType(base["base-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_Unsupported2DStructArr) << endl;
		exit(1);
	}
	st.type = sizedArrLevel(locNode, vtype, nullopt, &st.lens);
	return st;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The block a declared `[+]T` or one-level `[n]T` owns, or null for any other
// type: what a call result must be for the variable to take it over.
json PlnSemanticAnalyzer::takeOverType(const json& locNode, const json& varType)
{
	if (varType.value("type-kind","") != "arr") return json();
	if (isVarrAstType(varType)) {
		if (!varType["size-expr"].is_null()) return json();
	} else if (varType["size-expr"].is_null()
			|| (varType["base-type"].value("type-kind","") == "arr" && !isVarrAstType(varType["base-type"]))) {
		return json();
	}
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
	if (isVarrAstType(t)) t = varrStorageType(locNode, t);
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

// A family taking any element names it by the element's symbol, so two
// element types never share a key, whatever their struct names spell.
string allocShapeKey(const AllocShapeRef& shape)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	switch (shape.family) {
	case AllocFamily::Struct:       return shape.elem["type-name"].get<string>();
	case AllocFamily::Arr:          return "arr_" + symbolType(shape.elem);
	case AllocFamily::Varr:         return "varr_" + symbolType(shape.elem);
	case AllocFamily::EVarr:        return "evarr_" + symbolType(shape.elem);
	}
	return ""; // LCOV_EXCL_LINE -- every family is handled above
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The parameter types are those of the signatures build-mgr writes for each
// family, as palan-sa normalizes them when it analyzes the generated module.
string allocHelperSymbol(const AllocShapeRef& shape, const string& op)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	auto ptr = [](json base) { return json{{"type-kind","pntr"},{"base-type",move(base)}}; };
	auto mut = [](json base) { return json{{"type-kind","pntr"},{"mutable",true},{"base-type",move(base)}}; };
	auto readOnly = [](json base) { return json{{"type-kind","pntr"},{"mutable",false},{"base-type",move(base)}}; };
	const json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	AllocFamily family = shape.family;
	// An inner `[+]T` is its header struct's storage to the generated module,
	// down to a row's elements.
	function<json(const json&)> view = [&](const json& e) -> json {
		if (isVarrStorage(e))
			return ptr({{"type-kind","struct"},{"type-name","__pln_" + allocShapeKey(varrShape(e))}});
		if (!isArrLevel(e)) return e;
		json row = e;
		row["base-type"] = view(e["base-type"]);
		return row;
	};
	const json elem = view(shape.elem);
	// An owned struct's slot as the generated code fills it (`[]@!T`), and a
	// row's as a mutable run of such elements.
	function<json(const json&)> building = [&](const json& e) -> json {
		if (isStructStorage(e)) return mut(e["base-type"]);
		if (isArrLevel(e)) return mut(building(e["base-type"]));
		return e;
	};
	json owned, built;   // the storage as fini/free/copy take it, and as init fills it
	vector<json> dims;
	switch (family) {
	case AllocFamily::Struct:
		owned = ptr(elem); built = mut(elem);
		break;
	case AllocFamily::Arr:
		owned = ptr(elem); built = ptr(building(elem)); dims = {i64};
		if (isArrLevel(elem)) dims.push_back(i64);   // a row's length
		break;
	case AllocFamily::Varr:
	case AllocFamily::EVarr: {
		bool isRow = isArrLevel(elem);
		json rowRun = elem;   // a row's elements as a run (`[]T` / `[]$T`)
		rowRun.erase("arr-size");
		json value = isRow ? rowRun
		           : elem.value("type-kind","") == "struct" ? ptr(elem) : elem;   // what push takes
		// copy's source, a run of elements: their slots for owned elements (`[]T`),
		// and the slots' own elements for rows laid out in place.
		json run = isStructStorage(elem) ? ptr(elem) : readOnly(elem);
		if (isRow && family == AllocFamily::Varr) run = ptr(elem);
		else if (isRow) run = isOwnedBlock(shape.elem["base-type"]) ? rowRun : readOnly(elem["base-type"]);
		// The generated module sees the header as a struct of its own.
		json header = {{"type-kind","struct"},{"type-name","__pln_" + allocShapeKey(shape)}};
		owned = ptr(header); built = mut(header);
		if (op == "alloc" || op == "init") dims = {i64, i64};   // len, cap
		else if (op == "push") { owned = built; dims = {value}; }
		else if (op == "resize") { owned = built; dims = {i64}; }
		else if (op == "pop") owned = built;
		else if (op == "copy") { owned = built; dims = {run, i64}; }
		break;
	}
	}
	vector<json> params;
	if (op == "init") params.push_back(built);
	else if (op == "copy" && family != AllocFamily::Varr && family != AllocFamily::EVarr) params = {owned, owned};
	else if (op != "alloc") params.push_back(owned);   // fini, free, and the varr operations
	params.insert(params.end(), dims.begin(), dims.end());

	json jparams = json::array();
	for (auto& t : params) jparams.push_back({{"var-type", t}});
	string name = "__pln_" + op + "_" + allocShapeKey(shape);
	return moduleSymbol(PALAN_ALLOCATORS_MODULE) + ".." + funcSymbol(name, jparams);
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

json allocHelperCall(const AllocShapeRef& shape, const string& op, json args)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	return {{"expr-type","call"},{"name",allocHelperSymbol(shape, op)},
	        {"func-type","palan"},{"args",move(args)}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::recordAllocShape(const string& name)
{
	if (allocShapeNames_.count(name)) return;
	allocShapeNames_.insert(name);

	const StructDef& def = structDefs_[name];
	json owned = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "struct-ptr") continue;
		const StructDef& sub = structDefs_[f.typeName];
		// LCOV_EXCL_EXCEPTION_BR_START
		owned.push_back({
			{"name",              f.name},
			{"offset",            f.offset},
			{"struct-name",       f.typeName},
			{"struct-total-size", sub.totalSize}
		});
		// LCOV_EXCL_EXCEPTION_BR_STOP
		recordAllocShape(f.typeName);
	}
	json ownedArr = json::array();
	for (auto& f : def.fields) {
		if (f.typeKind != "arr-ptr") continue;
		// LCOV_EXCL_EXCEPTION_BR_START
		ownedArr.push_back({
			{"name",      f.name},
			{"offset",    f.offset},
			{"elem-kind", f.elemKind},
			{"leaf-name", f.typeName},
			{"count",     f.count},
			{"elem-size", f.stride}
		});
		// LCOV_EXCL_EXCEPTION_BR_STOP
		if (f.elemKind == "struct") {
			json elem = toStructPntrType({{"type-kind","prim"},{"type-name",f.typeName}}); // LCOV_EXCL_EXCEPTION_BR_LINE
			ownedArr.back()["shape-key"] = allocShapeKey(arrShape(elem));
			recordArrShape(elem);
		}
	}
	// LCOV_EXCL_EXCEPTION_BR_START
	sa["alloc-shapes"].push_back({
		{"shape-kind",         "struct"},
		{"shape-name",         name},
		{"total-size",         def.totalSize},
		{"owned-fields",       move(owned)},
		{"owned-array-fields", move(ownedArr)}
	});
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

// One element as the array's storage holds it, as build-mgr builds, frees and
// copies it. A struct's shape is recorded first, so build-mgr sees an
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
	// differently, so their helpers need different names. A row block's helpers
	// take its length, which `[+][n]T` knows and `[m][n]T` may know only at run
	// time, so the length is not in their name.
	if (isArrLevel(elem)) {
		json leaf = allocElemDesc(elem["base-type"], elem.value("embedded", false));
		if (!inPlace) {
			json desc = {{"kind","row"},{"key",symbolType(arrShape(elem).elem)},{"size",8},{"elem",move(leaf)}};
			if (elem.contains("arr-size")) desc["count"] = elem["arr-size"];
			return desc;
		}
		int64_t n = elem["arr-size"];
		return {{"kind","embed-row"},{"key","C" + symbolType(elem)},
		        {"count",n},{"size",n * leaf["size"].get<int64_t>()},{"elem",move(leaf)}};
	}
	if (isStructStorage(elem)) {
		string name = elem["base-type"]["type-name"];
		recordAllocShape(name);
		return {{"kind","struct"},{"key",symbolType(elem)},{"name",name},{"size",8}};
	}
	if (isVarrStorage(elem)) {
		recordVarrShape(elem);
		string key = allocShapeKey(varrShape(elem));
		return {{"kind","varr"},{"key",symbolType(elem)},{"name","__pln_" + key},{"shape-key",key},{"size",8}};
	}
	string name = elem["type-name"];
	recordAllocShape(name);
	return {{"kind","embed-struct"},{"key",symbolType(elem)},{"name",name},{"size",structDefs_.at(name).totalSize}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::recordArrShape(const json& elem)
{
	AllocShapeRef shape = arrShape(elem);
	string shape_key = allocShapeKey(shape);
	for (auto& s : sa["alloc-shapes"])
		if (s.value("shape-key","") == shape_key) return;
	// Recorded without a row's length, which every user passes on its own.
	json desc = allocElemDesc(shape.elem);
	sa["alloc-shapes"].push_back({ // LCOV_EXCL_EXCEPTION_BR_LINE
		{"shape-kind","arr"}, {"shape-key",shape_key}, {"elem",move(desc)}
	});
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::recordVarrShape(const json& vt)
{
	string shape_key = allocShapeKey(varrShape(vt));
	for (auto& s : sa["alloc-shapes"])
		if (s.value("shape-key","") == shape_key) return;
	json desc = allocElemDesc(vt["base-type"]["base-type"], isVarrEmbedded(vt));
	sa["alloc-shapes"].push_back({ // LCOV_EXCL_EXCEPTION_BR_LINE
		{"shape-kind","varr"}, {"shape-key",shape_key}, {"elem",move(desc)}
	});
	for (auto& def : varrBuiltinDefs(vt["base-type"]))
		varrBuiltins_[def["name"].get<string>()].push_back(def);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// push takes an element by value and pop returns one; a struct goes as its
// storage, copied in by push and owned by pop's caller.
json varrBuiltinDefs(const json& varr)
{
	// LCOV_EXCL_EXCEPTION_BR_START
	json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	const json& elem = varr["base-type"];
	bool embedded = varr.value("embedded", false);
	// A row is pushed from a borrow of one and popped as an owned array.
	json value = elem.value("type-kind","") == "struct" ? json{{"type-kind","pntr"},{"base-type",elem}}
	           : isArrLevel(elem) ? withArrPermission(elem, false) : elem;
	const json& popped = isArrLevel(elem) ? elem : value;
	AllocShapeRef shape = varrShape(varrPntrType(elem, embedded));
	auto self = [&](bool isMutable) {
		json vt = varrPntrType(elem, embedded);
		vt["mutable"] = isMutable;
		return json{{"name","v"},{"var-type",move(vt)}};
	};
	json defs = json::array();
	auto add = [&](const string& name, json params, const json* ret) {
		json def = {{"name",name},{"func-type","palan"},{"parameters",move(params)}};
		if (name == "len") def["inline"] = true;
		else def["symbol"] = allocHelperSymbol(shape, name);
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

json PlnSemanticAnalyzer::makeOwnedFreeStmt(const string& name, const json& pntrType)
{
	json var_id = {{"expr-type","id"},{"name",name},{"var-type",pntrType},{"value-type",pntrType}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	return makeOwnedValueFreeStmt(move(var_id));
} // LCOV_EXCL_EXCEPTION_BR_LINE

// What an owned struct, array or row holds, freed the way its owner's
// __pln_free_* would. Every free here accepts NULL. An array's helpers take
// its levels' lengths, `lens` when some are known only at run time.
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
	const json& elem = vt["base-type"];
	if (!ownsElems(vt)) return freeCall();
	recordArrShape(elem);
	if (lens.is_null()) {
		lens = json::array();
		for (int64_t len : arrShapeLens(vt)) lens.push_back(lenLit(len));
	}
	for (auto& len : lens) args.push_back(len);
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
	           : isVarrStorage(t) ? varrCopySourceMatch(st, t)
	           : copyShapeMatch(st, t);
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

	int64_t n = t["arr-size"];
	const json& elem = t["base-type"];
	if (t.value("embedded", false)) {
		return memcpyStmt(n * t["stride"].get<int64_t>());
	}
	// A row is copied as its owner holds it, whatever permission a borrow gives it.
	json owned = isArrLevel(elem) ? withoutArrPermission(elem) : elem;
	if (isArrLevel(owned)) {
		const json& leaf = owned["base-type"];
		if (isArrLevel(leaf) || isVarrPntr(leaf) || isPtrBorrow(leaf)) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_CopyUnsupportedShape, typeDisplayName(t)) << endl;
			exit(1);
		}
	}
	if (ownsElems(withoutArrPermission(t))) {
		recordArrShape(owned);
		// LCOV_EXCL_EXCEPTION_BR_START
		json args = json::array({dst, src});
		for (int64_t len : arrShapeLens(t)) args.push_back(lit(len));
		return copyStmt(arrShape(owned), move(args));
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	// Primitive elements, or '@T' slots whose pointers are copied as they are.
	int64_t elemBytes = isPtrBorrow(elem) ? 8 : elemSizeBytes(elem["type-name"]);
	return memcpyStmt(n * elemBytes);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The heap and frame versions of an aggregate variable share one var-type and
// differ only in how the outermost block is obtained and released, so each
// handler describes the heap version and the frame version is derived from it:
// __pln_alloc_K(args) becomes __pln_init_K(v, args) on the zeroed frame block,
// and __pln_free_K becomes __pln_fini_K, which leaves the block itself alone.
// An empty `heapInit` means the frame block's contents come from elsewhere.
json PlnSemanticAnalyzer::placeAggregateVar(const json& stmt, const string& name, const json& varType,
                                            json heapInit, json heapRelease, VarStorage storage,
                                            int64_t frameBytes, int frameAlign, bool zeroFill, json& post,
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
	bool helperInit = shape && heapInit.value("name", "") == helperName("alloc");
	if (zeroFill || helperInit) {
		json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
		json args = json::array({self,
			{{"expr-type","lit-int"},{"value","0"},{"value-type",{{"type-kind","prim"},{"type-name","int32"}}}},
			{{"expr-type","lit-uint"},{"value",to_string(frameBytes)},{"value-type",uint64_type}}});
		post.push_back({{"stmt-type","expr"},{"body",{
			{"expr-type","call"},{"name","memset"},{"func-type","c"},{"args",move(args)}}}});
	}
	if (helperInit) {
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

// A struct variable's heap block: zeroed, then built by its allocator when it
// owns fields.
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
		json one_arg  = {{"expr-type","lit-int"},{"value","1"},{"value-type",uint64_type}};
		call = {{"expr-type","call"},{"name","calloc"},{"func-type","c"},{"args",json::array({one_arg, size_arg})}};
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
	auto byteLit = [&](int64_t v) {
		return json{{"expr-type","lit-uint"},{"value",to_string(v)},{"value-type",uint64_type}};
	};
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
	} else {
		int64_t n = vt.value("arr-size", (int64_t)-1);
		const json& elem = vt["base-type"];
		if (ownsElems(vt)) {
			recordArrShape(elem);
			place.shape = arrShape(elem);
			json args = json::array();
			for (auto& len : lens)
				args.push_back(constArrSize(len) >= 0 ? lenLit(constArrSize(len)) : len);
			place.heapInit = allocHelperCall(*place.shape, "alloc", args);
			place.release = makeOwnedValueFreeStmt(self, move(args));
			place.frameBytes = n < 0 ? -1 : n * 8;
			place.frameAlign = 8;
		} else {
			int64_t elemBytes;
			json elemSize;
			if (!vt.value("embedded", false)) {
				elemBytes = isPtrBorrow(elem) ? 8 : elemSizeBytes(elem["type-name"].get<string>());
				place.frameAlign = elemBytes;
				elemSize = byteLit(elemBytes);
			} else if (elem.value("type-kind","") == "struct") {
				const StructDef& def = structDefs_.at(elem["type-name"].get<string>());
				elemBytes = def.totalSize;
				place.frameAlign = def.maxAlign;
				elemSize = byteLit(elemBytes);
			} else {
				// A row laid out in place, whose stride is missing when its
				// length is known only at run time.
				int leafBytes = elemSizeBytes(elem["base-type"]["type-name"].get<string>());
				place.frameAlign = leafBytes;
				elemBytes = vt.value("stride", (int64_t)-1);
				elemSize = elemBytes < 0 ? mul(lens[1], byteLit(leafBytes)) : byteLit(elemBytes);
			}
			json size = elemBytes == 1 ? lens[0] : mul(lens[0], move(elemSize));
			place.heapInit = {{"expr-type","call"},{"name","malloc"},{"func-type","c"},{"args",json::array({move(size)})}};
			place.frameBytes = n < 0 || elemBytes < 0 ? -1 : n * elemBytes;
		}
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
		                              place.frameBytes, place.frameAlign, false, post, place.shape);
		result.push_back({{"stmt-type","var-decl"},
			{"vars",json::array({{{"name",name},{"var-type",st.type},{"init",move(init)}}})}});
		for (auto& s : post) result.push_back(move(s));
	}
	return result;
	// LCOV_EXCL_EXCEPTION_BR_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE
