/// Palan Semantic Analyzer — declaration analysis
///
/// @file PlnSaDecl.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <functional>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"
#include "../common/PlnDefs.h"

static int alignUp(int val, int align) { return (val + align - 1) & ~(align - 1); }

// The non-pointer type a pointer field finally points to, and the mutability
// of each pointer level passed on the way (excluding the field's own).
struct PointeeLeaf { const json* type; vector<bool> innerPtrMutable; };
static PointeeLeaf pointeeLeaf(const json& pntr)
{
	PointeeLeaf leaf{&pntr["base-type"], {}};
	while (leaf.type->value("type-kind", "") == "pntr") {
		leaf.innerPtrMutable.push_back(leaf.type->value("mutable", false));
		leaf.type = &(*leaf.type)["base-type"];
	}
	return leaf;
}

// Only a cinclude'd struct can have such a field: a native function type is
// checked where it is written.
static string unsupportedPointee(const PointeeLeaf& leaf)
{
	return leaf.type->value("type-kind", "") == "func" && !unrepresentableTypeName(*leaf.type).empty()
		? "function pointer" : "";
}

static string pointeeElemKind(const PointeeLeaf& leaf)
{
	if (leaf.type->value("type-kind", "") == "func") return "func";
	return isPrimPointeeName(leaf.type->value("type-name", "")) ? "prim" : "struct";
}

static json pointeeFuncType(const PointeeLeaf& leaf)
{
	return leaf.type->value("type-kind", "") == "func" ? *leaf.type : json();
}

StructDef PlnSemanticAnalyzer::buildStructDef(const string& name, const json& fields,
                                              const json& locNode, bool isUnion, bool packed, int aligned)
{
	string errPrefix = locPrefix(locNode);
	StructDef def;
	def.name = name;
	def.isUnion = isUnion;
	int offset = 0, maxAlign = aligned;
	bool fieldPacked = false;
	int fieldAligned = 1;
	// Returns the new field's offset; a union overlays every field at 0 and
	// sizes to its largest member instead of advancing past each one.
	// GCC's packed/aligned attributes (C structs only) override `align`.
	auto place = [&](int64_t size, int align) {
		int naturalAlign = align;
		if (packed || fieldPacked) align = 1;
		align = max(align, fieldAligned);
		maxAlign = max(maxAlign, align);
		if (isUnion) {
			offset = max<int64_t>(offset, size);
			return 0;
		}
		int at = alignUp(offset, align);
		if (at % naturalAlign) def.hasUnalignedField = true;
		offset = at + size;
		return at;
	};
	for (auto& f : fields) {
		fieldPacked = f.value("packed", false);
		fieldAligned = f.value("aligned", 1);
		json vtype = f["var-type"];
		string tk = vtype.value("type-kind", "");
		string fieldName = f.value("name", "");
		// A field is laid out in the struct already, so `$[n]T` places the
		// array there as `[n]$T` does.
		if (tk == "embed" && vtype["base-type"].value("type-kind", "") == "arr") {
			if (isVarrAstType(vtype["base-type"])) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_EmbedOwningType,
				                                  typeDisplayName(ownedFieldType(locNode, vtype["base-type"]))) << endl;
				exit(1);
			}
			vtype = json(vtype["base-type"]);
			vtype["embedded"] = true;
			tk = "arr";
		}

		if (tk == "prim") {
			string tname = vtype["type-name"].get<string>();
			if (structDefs_.count(tname)) {
				const StructDef& target = structDefs_.at(tname);
				if (!target.isComplete) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_IncompleteStructType, tname,
					                                  target.incompleteReason, target.keyword()) << endl;
					exit(1);
				}
				int at = place(8, 8);
				def.fields.push_back({.name=fieldName, .typeKind="owned", .isMutable=false,
				                      .offset=at, .size=8, .type=toStructPntrType(vtype)});
				def.ownsFields = true;
			} else {
				int sz = elemSizeBytes(tname);
				if (sz < 0) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_UnknownType, tname) << endl;
					exit(1);
				}
				int at = place(sz, sz);
				def.fields.push_back({.name=fieldName, .typeKind="prim",
				                      .typeName=tname, .isMutable=false,
				                      .offset=at, .size=sz, .enumName=vtype.value("enum", "")});
			}
		} else if (tk == "embed") {
			if (vtype["base-type"].value("type-kind", "") != "prim") {
				cerr << errPrefix << PlnSaMessage::getMessage(E_EmbedElemNotStruct,
				                                  typeDisplayName(vtype["base-type"])) << endl;
				exit(1);
			}
			string structName = vtype["base-type"]["type-name"].get<string>();
			if (structName == name) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_RecursiveStruct, name) << endl;
				exit(1);
			}
			if (!structDefs_.count(structName)) {
				PlnSaMessageCode code = elemSizeBytes(structName) >= 0 ? E_EmbedElemNotStruct : E_UnknownType;
				cerr << errPrefix << PlnSaMessage::getMessage(code, structName) << endl;
				exit(1);
			}
			const StructDef& sub = structDefs_.at(structName);
			if (!sub.isComplete) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_IncompleteStructType, structName,
				                                  sub.incompleteReason, sub.keyword()) << endl;
				exit(1);
			}
			if (sub.ownsFields) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_EmbedOwningType, structName) << endl;
				exit(1);
			}
			int align = sub.maxAlign;
			int at = place(sub.totalSize, align);
			if (sub.hasUnalignedField) def.hasUnalignedField = true;
			if (!f.contains("name")) {
				// A C11 anonymous member: promote its fields so every consumer
				// finds s.a as an ordinary field of the owner. sub was registered
				// first, so its own anonymous members are already promoted.
				for (FieldLayout fl : sub.fields) {
					fl.offset += at;
					def.fields.push_back(fl);
				}
				continue;
			}
			def.fields.push_back({.name=fieldName, .typeKind="embed",
			                      .typeName=structName, .isMutable=false,
			                      .offset=at, .size=sub.totalSize});
		} else if (tk == "pntr") {
			int sz = 8, align = 8;
			PointeeLeaf leaf = pointeeLeaf(vtype);
			string baseName = leaf.type->value("type-name", "");
			bool isMut = vtype.value("mutable", false);
			int at = place(sz, align);
			def.fields.push_back({.name=fieldName, .typeKind="raw-ptr",
			                      .typeName=baseName, .isMutable=isMut,
			                      .offset=at, .size=sz, .elemKind=pointeeElemKind(leaf),
			                      .enumName=leaf.type->value("enum", ""),
			                      .innerPtrMutable=leaf.innerPtrMutable,
			                      .unsupported=unsupportedPointee(leaf),
			                      .funcType=pointeeFuncType(leaf)});
		} else if (isVarrAstType(vtype)) {
			int at = place(8, 8);
			def.fields.push_back({.name=fieldName, .typeKind="owned", .isMutable=false,
			                      .offset=at, .size=8, .type=ownedFieldType(locNode, vtype)});
			def.ownsFields = true;
		} else if (tk == "arr") {
			const json& size_expr = vtype["size-expr"];
			if (size_expr.is_null()) {
				// []T (unsized array) -- not a valid struct field form
				cerr << errPrefix << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
				exit(1);
			}
			const json& base = vtype["base-type"];
			bool embedded = vtype.value("embedded", false);
			if (!embedded && base.value("type-kind", "") != "pntr") {
				int at = place(8, 8);
				def.fields.push_back({.name=fieldName, .typeKind="owned", .isMutable=false,
				                      .offset=at, .size=8, .type=ownedFieldType(locNode, vtype)});
				def.ownsFields = true;
				continue;
			}
			int64_t count = fieldArrSize(locNode, size_expr);

			if (!embedded) {
				// [n]@T / [n]@!T: embedded array of n non-owning pointer slots (8B each)
				bool isMut = base.value("mutable", false);
				PointeeLeaf leaf = pointeeLeaf(base);
				string leaf_name = leaf.type->value("type-name", "");

				int align = 8;
				int at = place(count*8, align);
				def.fields.push_back({.name=fieldName, .typeKind="embed-ptr-arr",
				                      .typeName=leaf_name, .isMutable=isMut,
				                      .offset=at, .size=(int)(count*8),
				                      .count=count, .elemKind=pointeeElemKind(leaf), .stride=8,
				                      .enumName=leaf.type->value("enum", ""),
				                      .innerPtrMutable=leaf.innerPtrMutable,
				                      .unsupported=unsupportedPointee(leaf),
				                      .funcType=pointeeFuncType(leaf)});
				continue;
			}

			string base_kind = base.value("type-kind", "");
			string leaf_name = base.value("type-name", "");

			int stride = 0;
			string elemKind;
			int align;
			if (base_kind == "prim" && leaf_name == name) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_RecursiveStruct, name) << endl;
				exit(1);
			} else if (base_kind == "prim" && !structDefs_.count(leaf_name)) {
				// primitive leaf
				stride = elemSizeBytes(leaf_name);
				if (stride < 0) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_UnknownType, leaf_name) << endl;
					exit(1);
				}
				elemKind = "prim";
				align = stride;
			} else if (base_kind == "prim") {
				// struct leaf ([n]$Point)
				const StructDef& leafDef = structDefs_.at(leaf_name);
				if (!leafDef.isComplete) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_IncompleteStructType, leaf_name,
					                                  leafDef.incompleteReason, leafDef.keyword()) << endl;
					exit(1);
				}
				if (leafDef.ownsFields) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_EmbedOwningType, leaf_name) << endl;
					exit(1);
				}
				stride = leafDef.totalSize;
				elemKind = "struct";
				align = leafDef.maxAlign;
				if (leafDef.hasUnalignedField) def.hasUnalignedField = true;
			} else {
				// base_kind == "arr" ([n]$[m]T nested) -- not supported
				cerr << errPrefix << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
				exit(1);
			}

			int at = place(count*stride, align);
			def.fields.push_back({.name=fieldName, .typeKind="embed-arr",
			                      .typeName=leaf_name, .isMutable=false,
			                      .offset=at, .size=(int)(count*stride),
			                      .count=count, .elemKind=elemKind, .stride=stride,
			                      .enumName=base.value("enum", "")});
		} else {
			cerr << errPrefix << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
			exit(1);
		}
	}
	def.totalSize  = alignUp(offset, maxAlign);
	def.maxAlign   = maxAlign;
	def.isComplete = true;
	return def;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_expression_stmt(const json& stmt)
{
	return discardedValueStmt(stmt, sa_expr_maybe_void(stmt["body"]));
}

json PlnSemanticAnalyzer::discardedValueStmt(const json& stmt, json body)
{
	// A temp holding a call (a chained call's return values) becomes a block,
	// since only a call is lowered as an expression statement.
	if (body.value("expr-type", "") == "temp") {
		// LCOV_EXCL_EXCEPTION_BR_START
		json stmts = body["body"];
		stmts.push_back(discardedValueStmt(stmt, body["value"]));
		for (auto& r : body["release"])
			stmts.push_back(r);
		return {{"stmt-type","block"},{"body",move(stmts)}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// A bare `struct` value-type only comes from a C function's by-value
	// struct return; discarding it as a statement has no codegen lowering,
	// so it's rejected here rather than adding one.
	if (body.contains("value-type") && body["value-type"].value("type-kind","") == "struct") {
		string structName = body["value-type"].value("type-name", "");
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ByvalStructRetDiscarded, structName) << endl;
		exit(1);
	}
	// A discarded struct returned by a Palan call has no other owner.
	if (isExpiringOwned(body)) {
		// LCOV_EXCL_EXCEPTION_BR_START
		string temp = "__tmp_" + to_string(tempVarCounter_++);
		const json& st = body["value-type"];
		json decl = {{"stmt-type","var-decl"},
		             {"vars",json::array({{{"name",temp},{"var-type",st},{"init",body}}})}};
		return {{"stmt-type","block"},{"body",json::array({decl, makeOwnedFreeStmt(temp, st)})}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	return {
		{"stmt-type", "expr"},
		{"body", body}
	};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	json stmt2 = stmt;
	vector<VarStorage> storage;
	for (auto& var : stmt2["vars"]) {
		storage.push_back(takeVarStorage(var["var-type"]));
		var["var-type"] = resolveKnownType(stmt2, var["var-type"]);
	}
	return sa_resolved_var_decl(stmt2, move(storage));
}

json PlnSemanticAnalyzer::sa_resolved_var_decl(const json& stmt2, vector<VarStorage> storage)
{
	storage.resize(stmt2["vars"].size(), VarStorage::Heap);
	auto isArrInit = [](const json& var) {
		return var["var-type"].value("type-kind", "") == "arr" && var.contains("init");
	};
	auto isLitInit = [](const json& var) { return var.contains("init") && isAggregateLit(var["init"]); };
	auto isStructInit = [&](const json& var) {
		return var["var-type"].value("type-kind", "") == "prim" && var.contains("init")
			&& structDefs_.count(var["var-type"].value("type-name", ""));
	};
	auto ownRun = [&](const json& var) { return isArrInit(var) || isLitInit(var) || isStructInit(var); };

	// The lowering paths below apply one var-type to every var in a statement,
	// so the declaration is split into runs of equal var-type. The comparison
	// includes loc, so only vars inheriting the same source type node (`[f()]int32 a, b`)
	// share one evaluation of its size expression. Each array literal fixes its own
	// variable's size (`[]int32 a = [1,2], b = [1,2,3]`), so it always gets its own run,
	// as does an array initialized by copying another one or a struct literal.
	// An initialized struct gets its own run too: a call it takes over runs in its
	// var-decl, while copies and C struct-ret calls run after the var-decl, so a
	// shared run would break left-to-right order (`Pair a = b, c = mutate(@!b);`).
	json result = json::array();
	const json& vars = stmt2["vars"];
	size_t begin = 0;
	for (size_t i = 1; i <= vars.size(); i++) {
		if (i < vars.size() && vars[i]["var-type"] == vars[i-1]["var-type"]
				&& storage[i] == storage[i-1] && !ownRun(vars[i]) && !ownRun(vars[i-1]))
			continue;
		json run = stmt2;
		run["vars"] = json(vars.begin() + begin, vars.begin() + i);
		json lowered;
		if (isLitInit(vars[begin]) && !isArrInit(vars[begin]))
			lowered = sa_struct_lit_var_decl(run, storage[begin]);
		else if (!isArrInit(vars[begin]))
			lowered = sa_var_decl_group(run, storage[begin]);
		else if (vars[begin]["init"].value("expr-type", "") == "arr-lit")
			lowered = sa_arr_lit_var_decl(run, storage[begin]);
		else
			lowered = sa_arr_copy_var_decl(run, storage[begin]);
		for (auto& s : lowered)
			result.push_back(move(s));
		begin = i;
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_copy_var_decl(const json& stmt, VarStorage storage)
{
	json var = stmt["vars"][0];
	string name = var["name"];
	// Evaluated before the declaration so the variable is not in scope in its
	// own initializer.
	json src = sa_expression(var["init"]);
	// A heap `[+]T` or `[n]T` takes over a returned block instead of copying from it.
	if (storage == VarStorage::Heap && isExpiringOwned(src)) {
		json vt = takeOverType(stmt, var["var-type"]);
		if (!vt.is_null() && src["value-type"] == vt) {
			declareVar(name, vt, &stmt);
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, vt));
			return json::array({{{"stmt-type","var-decl"}, // LCOV_EXCL_EXCEPTION_BR_LINE
				{"vars",json::array({{{"name",name},{"var-type",vt},{"init",move(src)}}})}}});
		}
	}
	json declStmt = stmt;
	declStmt["vars"][0].erase("init");
	json result = sa_var_decl_group(declStmt, storage);
	const json& vt = *findVar(name);
	json dst = {{"expr-type","id"},{"name",name},{"var-type",vt},{"value-type",vt}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	result.push_back(makeCopyStmt(stmt, dst, src));
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl_group(const json& stmt2, VarStorage storage)
{
	const json& vtype = stmt2["vars"][0]["var-type"];
	string tk = vtype.value("type-kind", "");

	string spec = vtype.value("specifier", "");
	if (tk == "arr" && (spec == "variable" || spec == "raw"))
		return sa_arr_var_decl(stmt2, storage);
	if (tk == "prim" && structDefs_.count(vtype.value("type-name", "")))
		return sa_struct_var_decl(stmt2, storage);
	if (storage == VarStorage::Frame) {
		cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_FrameVarType, typeDisplayName(vtype)) << endl;
		exit(1);
	}

	json borrowType = normalizeArrBorrowType(stmt2, vtype);
	bool isArrBorrow = borrowType.contains("arr-size");

	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt2["vars"]) {
		string name = var["name"];
		json sa_var = var;
		json varType = deepNormalizePrimToStruct(isArrBorrow || isVarrPntr(borrowType) ? borrowType : var["var-type"]);
		sa_var["var-type"] = varType;
		if (var.contains("init")) {
			// Evaluate init before declaring the variable so that the variable
			// itself is not in scope during its own initializer (e.g. `int32 z = z+1`
			// should be an error, not silently read uninitialized z).
			const PlnType* toType = registry_.fromJson(varType);
			json init = sa_expression(var["init"], toType);
			if (isArrBorrow)
				checkArrBorrowBinding(stmt2, var["init"], init, varType);
			init = convertForBinding(stmt2, init, toType, varType);
			checkBorrowSource(stmt2, init, varType);
			checkPtrPermission(stmt2, init["value-type"], varType);
			sa_var["init"] = init;
		}
		declareVar(name, varType, &stmt2);
		sa_stmt["vars"].push_back(sa_var);
	}
	return json::array({sa_stmt});
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_size_expr(const json& stmt, const json& sizeExprAst)
{
	const PlnType* uint64Type = registry_.prim(PrimType::Name::Uint64);
	json sz = sa_expression(sizeExprAst, uint64Type);
	if (!sz.contains("value-type")) return sz;
	const PlnType* t = registry_.fromJson(sz["value-type"]);
	bool is_int = t->kind == PlnType::Kind::Prim
		&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float32
		&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float64;
	if (!is_int) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArraySizeNotInteger) << endl;
		exit(1);
	}
	// A macro-folded lit-int keeps its own (possibly narrower) C-declared
	// type instead of taking uint64 from expectedType, unlike a plain
	// source literal (which the lit-int/lit-uint branches above always
	// force to uint64 here). Left as-is, it would be codegen'd as an
	// inline immediate sized to its own width while every caller of this
	// helper embeds it into hand-built uint64 byte-count math, producing a
	// register-width mismatch at the assembler. Narrow this widening to
	// the literal case only -- a variable/expression operand already
	// reaches codegen as a load sized to the context, same as before this
	// helper existed, and widening it here would just wrap already-working
	// output in a redundant convert node.
	string et = sz.value("expr-type", "");
	if ((et == "lit-int" || et == "lit-uint") && t != uint64Type)
		sz = wrapConvert(sz, registry_.toJson(uint64Type));
	return sz;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Fills an omitted dimension with `count`; otherwise returns the declared
// size, which must be a compile-time constant.
string PlnSemanticAnalyzer::arrLitDimSize(const json& stmt, const string& name, json& sizeExpr, size_t count)
{
	if (sizeExpr.is_null()) {
		sizeExpr = {{"expr-type", "lit-uint"}, {"value", to_string(count)}};
		return to_string(count);
	}
	json sz = sa_arr_size_expr(stmt, sizeExpr);
	const json& lit = sz.value("expr-type", "") == "convert" ? sz["src"] : sz;
	string et = lit.value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitSizeNotConst, name) << endl;
		exit(1);
	}
	return lit["value"].get<string>();
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_arr_lit_var_decl(const json& stmt, VarStorage storage)
{
	const json& var = stmt["vars"][0];
	// A literal stored through a temp is reported by the store's target.
	string name = var.value("lit-name", var["name"].get<string>());
	const json& items = var["init"]["items"];

	json declStmt = stmt;
	json& declVar = declStmt["vars"][0];
	declVar.erase("init");
	declVar.erase("lit-name");

	// An omitted fixed size is the literal's count (of its first row, for a
	// row's size); a `[+]T` row takes any count.
	static const json noRows = json::array();
	json* level = &declVar["var-type"];
	const json* rows = &items;
	string declared;
	for (bool top = true; level->value("type-kind", "") == "arr"; top = false) {
		bool isVarr = isVarrAstType(*level);
		if (!isVarr && (*level)["size-expr"].is_null() && rows->empty()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitEmptySize, name) << endl;
			exit(1);
		}
		if (top || !isVarr) {
			string size = arrLitDimSize(stmt, name, (*level)["size-expr"], rows->size());
			if (top) declared = size;
		}
		rows = !rows->empty() && (*rows)[0].contains("items") ? &(*rows)[0]["items"] : &noRows;
		level = &(*level)["base-type"];
	}
	Storage st = arrVarStorage(stmt, declVar["var-type"]);
	ArrLitShape shape = arrLitShape(stmt, name, st.type, true);

	// Elements are analyzed before the variable is declared, so an element
	// cannot read the array it is initializing.
	json values = sa_arr_lit_values(var["init"], shape, name);

	// A variable-length array's size is its length.
	bool isVarr = isVarrStorage(st.type);
	if (isVarr && stoull(declared) < items.size()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitCountMismatch,
		                                  name, declared, to_string(items.size())) << endl;
		exit(1);
	}
	checkArrLitCount(stmt, items, name, shape);

	json result = sa_arr_var_decl(declStmt, storage, st, isVarr);
	emitArrLitAssigns({{"expr-type", "id"}, {"name", declVar["name"]}}, shape, values, result); // LCOV_EXCL_EXCEPTION_BR_LINE
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The levels of an already declared array, down to the elements a literal can
// give: numeric values or structs (stored in place or owned). A fixed level's
// size must be a compile-time constant.
PlnSemanticAnalyzer::ArrLitShape PlnSemanticAnalyzer::arrLitShape(const json& locNode, const string& name,
                                                                  const json& saType, bool varrTop)
{
	ArrLitShape shape = {{}, {}, nullptr};
	const json* t = &saType;
	while (isArrLevel(*t) || (isVarrStorage(*t) && (varrTop || !shape.levels.empty()))) {
		if (isVarrStorage(*t)) {
			shape.levels.push_back({-1, *t});
			t = &(*t)["base-type"]["base-type"];
			continue;
		}
		if (!t->contains("arr-size")) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrLitSizeNotConst, name) << endl;
			exit(1);
		}
		shape.levels.push_back({(*t)["arr-size"].get<int64_t>(), json()});
		t = &(*t)["base-type"];
	}
	const json& leaf = *t;
	string leafKind = leaf.value("type-kind", "");
	shape.leaf = leaf;
	if (isStructStorage(leaf))
		shape.structDef = &requireCompleteStruct(leaf["base-type"]["type-name"].get<string>(), locNode);
	else if (leafKind == "struct")
		shape.structDef = &requireCompleteStruct(leaf["type-name"].get<string>(), locNode);
	else if (leafKind != "prim") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrLitElemType, name) << endl;
		exit(1);
	}
	return shape;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// The values level by level: a row is an array of the next level's values,
// and a struct element is the array sa_struct_lit gives.
json PlnSemanticAnalyzer::sa_arr_lit_values(const json& lit, const ArrLitShape& shape, const string& name, size_t depth)
{
	bool isRow = depth + 1 < shape.levels.size();
	const PlnType* elemType = shape.structDef || isRow ? nullptr : registry_.fromJson(shape.leaf);
	json values = json::array();
	for (auto& item : lit["items"]) {
		bool isLit = item.value("expr-type", "") == "arr-lit";
		if (isRow != isLit && !(shape.structDef && !isRow)) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, name) << endl;
			exit(1);
		}
		if (isRow) {
			values.push_back(sa_arr_lit_values(item, shape, name, depth + 1));
		} else if (shape.structDef) {
			values.push_back(sa_struct_lit(item, *shape.structDef));
		} else {
			json value = sa_expression(item, elemType);
			values.push_back(convertForBinding(item, value, elemType, shape.leaf));
		}
	}
	return values;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A `[+]T` level takes any count, which its row is resized to.
void PlnSemanticAnalyzer::checkArrLitCount(const json& locNode, const json& items, const string& name,
                                           const ArrLitShape& shape, size_t depth)
{
	int64_t size = shape.levels[depth].size;
	if (size >= 0 && uint64_t(size) != items.size()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(
			depth == 0 ? E_ArrLitCountMismatch : E_ArrLitRowSizeMismatch,
			name, to_string(size), to_string(items.size())) << endl;
		exit(1);
	}
	if (depth + 1 == shape.levels.size())
		return;
	for (auto& row : items)
		checkArrLitCount(row, row["items"], name, shape, depth + 1);
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::emitArrLitAssigns(const json& arrAst, const ArrLitShape& shape, const json& values, json& out,
                                            bool sized, size_t depth)
{
	auto indexOf = [](json array, size_t i) -> json {
		return {{"expr-type", "arr-index"}, {"array", move(array)},
		        {"index", {{"expr-type", "lit-uint"}, {"value", to_string(i)}}}};
	};
	const json& varr = shape.levels[depth].varr;
	if (!sized && !varr.is_null()) {
		// LCOV_EXCL_EXCEPTION_BR_START
		json n = {{"expr-type","lit-int"},{"value",to_string(values.size())},
		          {"value-type",{{"type-kind","prim"},{"type-name","int64"}}}};
		json resize = allocHelperCall(varrShape(varr), "resize", json::array({sa_expression(arrAst), n}));
		out.push_back({{"stmt-type","expr"},{"body",move(resize)}});
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	bool isRow = depth + 1 < shape.levels.size();
	for (size_t i = 0; i < values.size(); i++) {
		json elemAst = indexOf(arrAst, i);
		const json& value = values[i];
		if (isRow) {
			emitArrLitAssigns(elemAst, shape, value, out, false, depth + 1);
		} else if (shape.structDef && value.is_object()) {
			out.push_back(makeCopyStmt(elemAst, sa_expression(elemAst), value));
		} else if (shape.structDef) {
			emitStructLitAssigns(elemAst, *shape.structDef, value, out);
		} else {
			out.push_back({{"stmt-type", "arr-assign"}, {"target", sa_expression(elemAst)}, {"value", value}}); // LCOV_EXCL_EXCEPTION_BR_LINE
		}
	}
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_struct_lit_var_decl(const json& stmt, VarStorage storage)
{
	const json& var = stmt["vars"][0];
	const json& vtype = var["var-type"];
	auto isStructName = [&](const json& t) {
		return t.value("type-kind", "") == "prim" && structDefs_.count(t.value("type-name", ""));
	};
	if (vtype.value("type-kind", "") == "pntr" && isStructName(vtype["base-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructLitTarget) << endl;
		exit(1);
	}
	if (!isStructName(vtype))
		return sa_var_decl_group(stmt, storage);

	const StructDef& def = requireCompleteStruct(vtype["type-name"].get<string>(), stmt);
	// Evaluated before the declaration so the variable is not in scope in its
	// own initializer.
	json values = sa_struct_lit(var["init"], def);
	json declStmt = stmt;
	declStmt["vars"][0].erase("init");
	json result = sa_var_decl_group(declStmt, storage);
	emitStructLitAssigns({{"expr-type", "id"}, {"name", var["name"]}}, def, values, result); // LCOV_EXCL_EXCEPTION_BR_LINE
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Normalizes a struct value written in field order (`[0, 1]`) or by name
// (`{x: 0, y: 1}`) to one array of analyzed values in field declaration
// order; an entry for a struct field is itself such a value, and one for an
// array field is what sa_arr_lit_values gives. A struct value written as an
// expression stays that analyzed expression (an object, not an array): it is
// copied from, like the source of '->' to a struct.
json PlnSemanticAnalyzer::sa_struct_lit(const json& item, const StructDef& def)
{
	string et = item.value("expr-type", "");
	if (et != "arr-lit" && et != "dict-lit") {
		json value = sa_expression(item);
		if (!value.contains("value-type") || !isStructPntr(value["value-type"])) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitExpected, def.name) << endl;
			exit(1);
		}
		const json& vt = value["value-type"];
		if (vt["base-type"]["type-name"] != def.name) {
			json dstType = {{"type-kind", "pntr"}, {"base-type", {{"type-kind", "struct"}, {"type-name", def.name}}}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_CopyShapeMismatch,
				typeDisplayName(dstType), typeDisplayName(vt)) << endl;
			exit(1);
		} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here
		return value;
	}
	const json& items = item["items"];
	vector<const json*> given(def.fields.size(), nullptr);
	if (et == "arr-lit") {
		if (items.size() != def.fields.size()) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitFieldCount,
			                                  def.name, to_string(def.fields.size()), to_string(items.size())) << endl;
			exit(1);
		}
		for (size_t k = 0; k < items.size(); k++)
			given[k] = &items[k];
	} else {
		for (auto& named : items) {
			string fn = named["name"].get<string>();
			auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
			if (it == def.fields.end()) {
				cerr << locPrefix(named) << PlnSaMessage::getMessage(E_UnknownField, def.name, fn, def.keyword()) << endl;
				exit(1);
			}
			const json*& slot = given[it - def.fields.begin()];
			if (slot) {
				cerr << locPrefix(named) << PlnSaMessage::getMessage(E_StructLitDupField, fn) << endl;
				exit(1);
			}
			slot = &named["value"];
		}
		for (size_t k = 0; k < given.size(); k++) {
			if (!given[k]) {
				cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitMissingField, def.name, def.fields[k].name) << endl;
				exit(1);
			}
		}
	}

	json values = json::array();
	for (size_t k = 0; k < def.fields.size(); k++) {
		const FieldLayout& f = def.fields[k];
		const json& v = *given[k];
		string structName = fieldStructName(f);
		if (!structName.empty()) {
			values.push_back(sa_struct_lit(v, requireCompleteStruct(structName, v)));
			continue;
		}
		if (isLitArrField(f)) {
			string vet = v.value("expr-type", "");
			if (vet == "dict-lit") {
				cerr << locPrefix(v) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
				exit(1);
			}
			if (vet != "arr-lit") {
				cerr << locPrefix(v) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, f.name) << endl;
				exit(1);
			}
			ArrLitShape shape = arrLitShape(v, f.name, fieldValueType(f), true);
			values.push_back(sa_arr_lit_values(v, shape, f.name));
			checkArrLitCount(v, v["items"], f.name, shape);
			continue;
		}
		if (f.typeKind != "prim") {
			cerr << locPrefix(v) << PlnSaMessage::getMessage(E_StructLitFieldType, def.name, f.name) << endl;
			exit(1);
		}
		json fieldType = fieldValueType(f);
		const PlnType* toType = registry_.fromJson(fieldType);
		json value = sa_expression(v, toType);
		values.push_back(convertForBinding(v, value, toType, fieldType));
	}
	return values;
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::emitStructLitAssigns(const json& objAst, const StructDef& def, const json& values, json& out)
{
	for (size_t k = 0; k < def.fields.size(); k++) {
		const FieldLayout& f = def.fields[k];
		if (f.typeKind == "prim") {
			out.push_back(makeFieldAssign(resolveObjectChain(objAst, /*forWrite=*/true), f, values[k]));
			continue;
		}
		json fieldAst = {{"expr-type", "field-access"}, {"object", objAst}, {"field", f.name}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if (isLitArrField(f))
			emitArrLitAssigns(fieldAst, arrLitShape(objAst, f.name, fieldValueType(f), true), values[k], out, false); // LCOV_EXCL_EXCEPTION_BR_LINE
		else if (values[k].is_object())
			out.push_back(makeCopyStmt(objAst, makeFieldAccess(resolveObjectChain(objAst, /*forWrite=*/true), f), values[k])); // LCOV_EXCL_EXCEPTION_BR_LINE
		else
			emitStructLitAssigns(fieldAst, requireCompleteStruct(fieldStructName(f), objAst), values[k], out);
	}
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::enterBlockDecls(const json& stmts)
{
	vector<string> names;
	set<string> vars;
	for (auto& stmt : stmts) {
		addDeclaredVarNames(stmt, vars);
		string t = stmt.value("stmt-type", "");
		if (t != "struct-def" && t != "enum-def" && t != "type-alias" && t != "const-decl") continue;
		string name = stmt["name"].get<string>();
		bool varTaken = t == "const-decl" && (findVar(name) || vars.count(name));
		if (varTaken || isDeclNameTaken(name) || find(names.begin(), names.end(), name) != names.end()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(
				t == "const-decl" ? E_DuplicateConstDecl : E_DuplicateTypeName, name) << endl;
			exit(1);
		}
		names.push_back(name);
		topDecls_.emplace(name, TopDecl{&stmt});
		if (t == "struct-def" || t == "enum-def") declareLocalTypeName(stmt);
	}
	blockDeclNames_.push_back(names);
	blockImportedTypes_.emplace_back();
	for (auto& name : names) ensureTypeName(name, false);
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::declareLocalTypeName(const json& stmt)
{
	string name = stmt["name"].get<string>();
	string internal = PlnSaMessage::localTypeName(moduleId_, ++localTypeSeq_, name);
	localTypeNames_[name] = internal;
	// A pointer may name the struct before its layout is resolved.
	if (stmt["stmt-type"] == "struct-def") {
		typeAliases_[name] = {{"type-kind", "prim"}, {"type-name", internal}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		declaredStructs_.insert(internal);
	}
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::leaveBlockDecls()
{
	for (auto& name : blockDeclNames_.back()) {
		topDecls_.erase(name);
		typeAliases_.erase(name);
		constDecls_.erase(name);
		localTypeNames_.erase(name);
	}
	blockDeclNames_.pop_back();
	// An imported type stays registered: a type the block used may depend on it.
	for (auto& written : blockImportedTypes_.back()) typeAliases_.erase(written);
	blockImportedTypes_.pop_back();
}

string PlnSemanticAnalyzer::declaredTypeName(const string& written) const
{
	auto it = localTypeNames_.find(written);
	return it == localTypeNames_.end() ? written : it->second;
}

bool PlnSemanticAnalyzer::isDeclNameTaken(const string& name) const
{
	return topDecls_.count(name) || isWrittenNameTaken(name);
}

// A const is visible from the top of its block, so a variable declared
// before it in the same block still clashes with it.
void PlnSemanticAnalyzer::addDeclaredVarNames(const json& stmt, set<string>& names)
{
	string t = stmt.value("stmt-type", "");
	if (t != "var-decl" && t != "tapple-decl") return;
	const char* key = t == "var-decl" ? "name" : "var-name";
	for (auto& v : stmt["vars"]) names.insert(v[key].get<string>());
}

// A Palan type registered only as another type's dependency, or as `V.T`,
// is not written by its name here, so a declaration may reuse that name.
bool PlnSemanticAnalyzer::isWrittenNameTaken(const string& name) const
{
	bool isCType = (structDefs_.count(name) || enumDefs_.count(name)) && !typeOrigins_.count(name);
	return importedTypes_.count(name) || typeAliases_.count(name) || isCType
		|| constDecls_.count(name) || elemSizeBytes(name) >= 0;
}

json PlnSemanticAnalyzer::sa_struct_def(const json& stmt)
{
	string written = stmt["name"].get<string>();
	string name = declaredTypeName(written);
	declareTypeName(name, stmt);
	// A local struct was bound to its program-wide name when declared.
	typeAliases_.emplace(written, json{{"type-kind", "prim"}, {"type-name", name}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	json fields = stmt["fields"];
	for (auto& f : fields) {
		f["var-type"] = resolveWrittenType(stmt, f["var-type"]);
		// Only a pointee may name a struct that is not registered yet; every
		// other field type is checked by buildStructDef against structDefs_.
		const json* t = &f["var-type"];
		if (t->value("type-kind", "") == "arr" && !t->value("embedded", false))
			t = &(*t)["base-type"];
		if (t->value("type-kind", "") != "pntr") continue;
		string pointee = pointeeLeaf(*t).type->value("type-name", "");
		bool laterDef = pointee == name || declaredStructs_.count(pointee);
		if (!laterDef) requireKnownTypeNames(stmt, f["var-type"]);
		json* leaf = const_cast<json*>(t);
		while (leaf->value("type-kind", "") == "pntr") leaf = &(*leaf)["base-type"];
		if (leaf->value("type-kind", "") == "func") *leaf = deepNormalizePrimToStruct(*leaf);
	}
	structDefs_[name] = buildStructDef(name, fields, stmt);
	return json::array();
} // LCOV_EXCL_EXCEPTION_BR_LINE

// c2ast has no concept of Palan's $T inline-embedding sugar, so it represents
// a C struct/union-by-value field as a plain tag; translate the normalizeCType'd
// "struct" to the "embed" shape buildStructDef expects.
static json cFieldVarType(const json& vtype)
{
	auto isStruct = [](const json& t) { return t.value("type-kind", "") == "struct"; };
	if (isStruct(vtype)) {
		return {{"type-kind", "embed"}, // LCOV_EXCL_EXCEPTION_BR_LINE
		        {"base-type", {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}}}}; // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	if (vtype.value("type-kind", "") == "arr" && vtype.contains("base-type")) {
		json v = vtype;
		json& bt = v["base-type"];
		// Inside an array, a struct-by-value leaf is Palan's [n]$Foo / [n]Foo
		// shape, named with a plain prim type-name rather than wrapped in "embed".
		if (isStruct(bt)) {
			bt = {{"type-kind", "prim"}, {"type-name", bt["type-name"]}};
		} else if (bt.value("type-kind", "") == "pntr") {
			// An inline array of pointer slots ("T *field[n];") is Palan's
			// [n]@T / [n]@!T shape; c2ast sets "embedded" here unlike native
			// syntax, so strip it for a single embed-ptr-arr shape either way.
			v.erase("embedded");
			json& leaf = bt["base-type"];
			if (isStruct(leaf)) {
				leaf = {{"type-kind", "prim"}, {"type-name", leaf["type-name"]}};
			}
		}
		return v;
	}
	return vtype;
}

// An unsupported field type (e.g. a C bitfield) registers its
// owning tag as an incomplete struct -- usable only through @T/@!T -- rather
// than exit(1), since cinclude pulls in glibc-internal structs never
// referenced by Palan interop code. A field naming a still-incomplete struct
// is rejected the same way.
static bool isSupportedCFieldType(const json& vtype, const map<string, StructDef>& structDefs,
                                   const string& ownerName)
{
	string tk = vtype.value("type-kind", "");
	if (tk == "prim")
		return elemSizeBytes(vtype.value("type-name", "")) >= 0;
	if (tk == "embed") {
		string structName = vtype["base-type"].value("type-name", "");
		return structName != ownerName && structDefs.count(structName)
			&& structDefs.at(structName).isComplete;
	}
	if (tk == "arr") {
		if (!vtype.contains("size-expr") || !vtype.contains("base-type")) return false;
		const json& size_expr = vtype["size-expr"];
		if (size_expr.is_null()) return false;  // e.g. "T name[];" -- not a constant size
		string set = size_expr.value("expr-type", "");
		if (set != "lit-int" && set != "lit-uint") return false;

		const json& bt = vtype["base-type"];
		string btk = bt.value("type-kind", "");
		if (btk == "arr") return false;  // 2D+ array fields: buildStructDef doesn't lay these out
		if (btk == "pntr") return true;  // [n]@T / [n]@!T slot array
		if (btk == "prim") {
			string leaf = bt.value("type-name", "");
			return elemSizeBytes(leaf) >= 0
				|| (structDefs.count(leaf) > 0 && structDefs.at(leaf).isComplete);
		}
		return false;
	}
	return tk == "pntr";
}

void PlnSemanticAnalyzer::registerCStruct(const json& cincludeStmt, const json& s)
{
	string name = s["name"].get<string>();
	requireNotPalanType(name, cincludeStmt, true);
	auto existing = structDefs_.find(name);
	if (existing != structDefs_.end() && existing->second.isComplete)
		return;  // first complete definition wins

	if (!s.contains("fields")) {
		// A forward-declared or bare-referenced tag registers as incomplete
		// rather than not at all, so requireCompleteStruct can name it in a
		// diagnostic instead of leaving it unresolved.
		if (existing == structDefs_.end()) {
			StructDef def;
			def.name             = name;
			def.isUnion          = s.value("union", false);
			def.incompleteReason = "forward-declared";
			structDefs_[name] = def;
		}
		return;
	}

	json fields = json::array();
	for (auto& f : s["fields"]) {
		json vt = f["var-type"];
		resolveCTypeRefs(vt);
		vt = cFieldVarType(normalizeCType(vt));
		if (!isSupportedCFieldType(vt, structDefs_, name)) {
			// Register the tag as an incomplete struct rather than not at all: the
			// name is real (glibc defines it), only its layout is unavailable this
			// version. This lets SA report E_IncompleteStructType -- distinct from
			// "no such type" -- and still allows @T/@!T use of the tag.
			StructDef def;
			def.name             = name;
			def.isUnion          = s.value("union", false);
			def.incompleteReason = "unsupported-field";
			structDefs_[name] = def;
			return;
		}
		json field = {{"var-type", vt}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		for (const char* key : {"name", "packed", "aligned"})
			if (f.contains(key)) field[key] = f[key];
		fields.push_back(field);
	}
	structDefs_[name] = buildStructDef(name, fields, cincludeStmt,
	                                   s.value("union", false), s.value("packed", false), s.value("aligned", 1));
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_type_alias(const json& stmt)
{
	// An alias has no program-wide name to claim: it is only written.
	string name = stmt["name"].get<string>();
	typeAliases_[name] = resolveWrittenType(stmt, stmt["type"]);
	return json::array();
}

json PlnSemanticAnalyzer::sa_const_decl(const json& stmt)
{
	string name = stmt["name"].get<string>();
	// Stored untyped so each reference is typed by its own context, exactly
	// like the literal written in place of the name.
	json value = foldConstExpr(stmt["value"]);
	string et = sa_expression(value).value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint" && et != "lit-flo" && et != "lit-str") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ConstNotCompileTimeValue, name) << endl;
		exit(1);
	}
	value.erase("loc");
	constDecls_[name] = value;
	return json::array();
}

json PlnSemanticAnalyzer::sa_enum_def(const json& stmt)
{
	string written = stmt["name"].get<string>();
	string name = declaredTypeName(written);
	declareTypeName(name, stmt);

	map<string, int64_t> values;
	int64_t next = 0;
	bool nextOverflows = false;
	for (auto& e : stmt["enumerators"]) {
		string ename = e["name"].get<string>();
		int64_t v = next;
		if (e.contains("value")) {
			json lit = foldConstExpr(e["value"]);
			if (lit.value("expr-type", "") != "lit-int" && lit.value("expr-type", "") != "lit-uint") {
				cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueNotConst, ename) << endl;
				exit(1);
			}
			string text = lit["value"].get<string>();
			try { v = stoll(text); }
			catch (const out_of_range&) {
				cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueOutOfRange, ename, text) << endl;
				exit(1);
			} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here
		} else if (nextOverflows) {
			cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueOutOfRange, ename, "9223372036854775808") << endl;
			exit(1);
		}
		if (!values.emplace(ename, v).second) {
			cerr << locPrefix(e) << PlnSaMessage::getMessage(E_DuplicateEnumerator, ename, name) << endl;
			exit(1);
		}
		nextOverflows = v == INT64_MAX;
		next = nextOverflows ? v : v + 1;
	}
	registerEnum(name, move(values), stmt["loc"], written);
	return json::array();
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::registerEnum(const string& name, map<string, int64_t> values, const json& loc,
                                       const string& written)
{
	int64_t minV = 0, maxV = 0;
	bool first = true;
	for (auto& [ename, v] : values) {
		minV = first ? v : min(minV, v);
		maxV = first ? v : max(maxV, v);
		first = false;
	}
	// C types an enumerator as int when it fits, and GCC widens past that the
	// same way; the width always matches the C enum's, so values pass to and
	// from C unchanged.
	string base = minV >= INT32_MIN && maxV <= INT32_MAX ? "int32"
		: minV >= 0 && maxV <= UINT32_MAX ? "uint32"
		: minV < 0 ? "int64" : "uint64";
	enumDefs_[name] = {move(values), loc};
	typeAliases_[written] = {{"type-kind", "prim"}, {"type-name", base}, {"enum", name}}; // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::registerCEnum(const json& cincludeStmt, const json& e)
{
	string name = e["name"].get<string>();
	auto existing = enumDefs_.find(name);
	if (existing != enumDefs_.end() && existing->second.loc.is_null())
		return;  // first definition wins, as for a C struct
	if (existing != enumDefs_.end() || structDefs_.count(name) || typeAliases_.count(name)) {
		cerr << locPrefix(cincludeStmt) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
	map<string, int64_t> values;
	for (auto& en : e["enumerators"])
		values[en["name"].get<string>()] = stoll(en["value"].get<string>());
	registerEnum(name, move(values), json(), name);
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Each cinclude is a separate c2ast run, so a header relying on an earlier
// one's typedef (jpeglib.h on stdio.h's size_t) gets "user" for it; that is
// replaced here by what c2ast would have emitted had both been one
// translation unit. An unregistered enum (c2ast couldn't compute its values)
// or unknown "user" name stays as is, reported as unrepresentable where used.
void PlnSemanticAnalyzer::resolveCTypeRefs(json& node) const
{
	if (node.is_array()) {
		for (auto& e : node) resolveCTypeRefs(e);
		return;
	}
	if (!node.is_object()) return;
	if (node.value("type-kind", "") == "user" && node.contains("type-name")) {
		auto it = cTypedefs_.find(node["type-name"].get<string>());
		if (it == cTypedefs_.end()) return;
		bool isConst = node.value("const", false);
		node = it->second;
		if (isConst) node["const"] = true;
		return;
	}
	if (node.value("type-kind", "") == "enum" && node.contains("type-name")) {
		json t = enumTypeNamed(node["type-name"].get<string>());
		if (t.is_null()) return;
		if (node.value("const", false)) t["const"] = true;
		node = move(t);
		return;
	}
	for (auto& [key, val] : node.items()) resolveCTypeRefs(val);
} // LCOV_EXCL_EXCEPTION_BR_LINE

const json* PlnSemanticAnalyzer::findTypeBinding(const string& written) const
{
	auto it = typeAliases_.find(written);
	return it == typeAliases_.end() ? nullptr : &it->second;
}

// Every Palan type source may write here is bound in typeAliases_; an
// unbound one is registered only as another type's dependency, or as `V.T`.
void PlnSemanticAnalyzer::requireWritableTypeName(const json& locNode, const string& written) const
{
	if (typeAliases_.count(written) || !typeOrigins_.count(written)) return;
	auto it = hiddenTypeNames_.find(written);
	if (it == hiddenTypeNames_.end())
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownType, written) << endl;
	else
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnqualifiedAliasType, written, it->second) << endl;
	exit(1);
}

// A binding already holds program-wide names, so it is never looked up again:
// in another scope the same string may name a different type. Leaves a name
// as "prim" rather than "struct", so callers' structDefs_.count(type-name)
// checks keep working.
json PlnSemanticAnalyzer::resolveWrittenType(const json& locNode, const json& type) const
{
	string tk = type.value("type-kind", "");
	if (tk == "prim") {
		string tname = type.value("type-name", "");
		requireWritableTypeName(locNode, tname);
		const json* bound = findTypeBinding(tname);
		return bound ? *bound : type;
	}
	json resolved = type;
	if ((tk == "pntr" || tk == "arr" || tk == "embed") && type.contains("base-type"))
		resolved["base-type"] = resolveWrittenType(locNode, type["base-type"]);
	if (tk == "func") {
		for (auto& p : resolved["parameters"]) p["var-type"] = resolveWrittenType(locNode, p["var-type"]);
		resolved["ret-type"] = resolveWrittenType(locNode, resolved["ret-type"]);
	}
	return resolved;
}

json PlnSemanticAnalyzer::resolveKnownType(const json& locNode, const json& type) const
{
	json resolved = resolveWrittenType(locNode, type);
	requireKnownTypeNames(locNode, resolved);
	return resolved;
}

bool PlnSemanticAnalyzer::claimTypeName(const string& name, const string& origin, const json& locNode)
{
	auto [it, isNew] = typeOrigins_.emplace(name, origin);
	// Every Palan struct and enum is in typeOrigins_, so a new name already
	// registered here is a C type or a primitive.
	bool taken = isNew && (structDefs_.count(name) || enumDefs_.count(name) || elemSizeBytes(name) >= 0);
	if (taken || (!isNew && it->second != origin)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
	return isNew;
}

// Unlike an import, which may bring the same type in twice, a declaration
// claims its name exactly once.
void PlnSemanticAnalyzer::declareTypeName(const string& name, const json& stmt)
{
	if (!claimTypeName(name, moduleId_, stmt)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

// A cinclude inside a block runs after the Palan types are declared. A C
// struct takes the program-wide name too, a typedef only the written one.
void PlnSemanticAnalyzer::requireNotPalanType(const string& name, const json& locNode, bool isStruct) const
{
	auto d = topDecls_.find(name);
	bool isAlias = d != topDecls_.end() && (*d->second.stmt)["stmt-type"] == "type-alias";
	bool written = localTypeNames_.count(name)
		|| (typeAliases_.count(name) && (typeOrigins_.count(name) || isAlias));
	if (written || (isStruct && typeOrigins_.count(name))) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

// The same type may come in through two imports.
void PlnSemanticAnalyzer::bindImportedTypeName(const json& locNode, const string& name, const json& type)
{
	auto [it, isNew] = typeAliases_.emplace(name, type);
	if (!isNew && it->second != type) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

void PlnSemanticAnalyzer::setTypeNameable(const string& name, bool nameable, const string& alias)
{
	if (nameable)
		hiddenTypeNames_.erase(name);
	else if (!alias.empty() && !typeAliases_.count(name))
		hiddenTypeNames_.emplace(name, alias);
}

// Registers the types `stmt` imports, as the exporting module resolved them.
void PlnSemanticAnalyzer::adoptExportedTypes(const json& stmt, const json& impAst, PlnSemanticAnalyzer& sub)
{
	if (!impAst["ast"].contains("statements")) return;
	set<string> targets;
	bool selective = stmt.contains("targets");
	if (selective)
		for (auto& t : stmt["targets"])
			targets.insert(t.get<string>());

	string alias = stmt.value("alias", "");
	for (auto& d : impAst["ast"]["statements"]) {
		if (!d.value("export", false)) continue;
		string name = d["name"].get<string>();
		if (selective && !targets.count(name)) continue;
		string written = alias.empty() ? name : alias + "." + name;
		bool wasWritable = typeAliases_.count(written);
		adoptExportedType(stmt, d, sub);
		if (!blockImportedTypes_.empty() && !wasWritable) blockImportedTypes_.back().push_back(written);
	}
}

void PlnSemanticAnalyzer::adoptExportedType(const json& stmt, const json& decl, PlnSemanticAnalyzer& sub)
{
	string alias = stmt.value("alias", "");
	string name = decl["name"].get<string>();
	sub.ensureTypeName(name, false);
	string kind = decl["stmt-type"].get<string>();
	if (kind == "struct-def") {
		adoptStruct(sub, name, alias.empty(), alias, stmt);
		if (!alias.empty())
			typeAliases_[alias + "." + name] = {{"type-kind", "prim"}, {"type-name", name}};
	} else if (kind == "enum-def") {
		adoptEnum(sub, name, alias.empty(), alias, stmt);
		if (!alias.empty())
			typeAliases_[alias + "." + name] = sub.typeAliases_.at(name);
	} else {
		const json& t = sub.typeAliases_.at(name);
		adoptTypeDeps(sub, t, stmt);
		if (alias.empty()) {
			bindImportedTypeName(stmt, name, t);
			setTypeNameable(name, true, alias);
		} else {
			typeAliases_[alias + "." + name] = t;
		}
	}
} // LCOV_EXCL_EXCEPTION_BR_LINE

// A struct comes with every struct and enum its fields use, nameable or not:
// its layout and allocator need them.
void PlnSemanticAnalyzer::adoptStruct(PlnSemanticAnalyzer& sub, const string& name, bool nameable,
		const string& alias, const json& locNode)
{
	auto origin = sub.typeOrigins_.find(name);
	// This module's own struct, which it resolves itself.
	if (origin != sub.typeOrigins_.end() && origin->second == moduleId_) return;
	const StructDef& def = sub.structDefs_.at(name);
	bool adopted;
	if (origin == sub.typeOrigins_.end()) {
		// A C struct, global as in C: first complete definition wins, as for a cinclude.
		auto it = structDefs_.find(name);
		adopted = it == structDefs_.end() || (!it->second.isComplete && def.isComplete);
		if (adopted) structDefs_[name] = def;
	} else {
		adopted = claimTypeName(name, origin->second, locNode);
		if (adopted) structDefs_[name] = def;
		if (nameable) bindImportedTypeName(locNode, name, {{"type-kind", "prim"}, {"type-name", name}});
		setTypeNameable(name, nameable, alias);
	}
	if (!adopted) return;
	for (auto& f : def.fields) {
		if (f.typeKind == "owned") {
			adoptTypeDeps(sub, f.type, locNode);
			// The defining module got the builtins of each `[+]U` level from
			// varrStorageType; a field access here needs them too.
			for (const json* t = &f.type; t->contains("base-type"); t = &(*t)["base-type"])
				if (isVarrStorage(*t)) recordVarrShape(*t);
			continue;
		}
		if (sub.structDefs_.count(f.typeName)) {
			adoptStruct(sub, f.typeName, false, "", locNode);
		} else if (f.elemKind == "struct" && sub.declaredStructs_.count(f.typeName)) {
			// A pointee the exporting module has yet to resolve.
			string pointee = f.typeName;
			const json* loc = &locNode;
			whenSettled([this, &sub, pointee, loc] {
				sub.ensureStructDef(pointee);
				adoptStruct(sub, pointee, false, "", *loc);
			});
		}
		if (!f.enumName.empty()) adoptEnum(sub, f.enumName, false, "", locNode);
		if (f.elemKind == "func") adoptTypeDeps(sub, f.funcType, locNode);
	}
} // LCOV_EXCL_EXCEPTION_BR_LINE

void PlnSemanticAnalyzer::adoptEnum(const PlnSemanticAnalyzer& sub, const string& name, bool nameable,
		const string& alias, const json& locNode)
{
	auto origin = sub.typeOrigins_.find(name);
	if (origin != sub.typeOrigins_.end() && origin->second == moduleId_) return;
	if (origin == sub.typeOrigins_.end()) {
		if (!enumDefs_.count(name)) {
			enumDefs_[name]   = sub.enumDefs_.at(name);
			typeAliases_[name] = sub.typeAliases_.at(name);
		}
		return;
	}
	bool adopted = claimTypeName(name, origin->second, locNode);
	if (adopted) enumDefs_[name] = sub.enumDefs_.at(name);
	if (nameable) bindImportedTypeName(locNode, name, sub.typeAliases_.at(name));
	setTypeNameable(name, nameable, alias);
}

void PlnSemanticAnalyzer::adoptTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode)
{
	const json* t = &type;
	while (t->contains("base-type")) t = &(*t)["base-type"];
	string tname = t->value("type-name", "");
	if (sub.structDefs_.count(tname)) adoptStruct(sub, tname, false, "", locNode);
	if (t->contains("enum")) adoptEnum(sub, (*t)["enum"].get<string>(), false, "", locNode);
}

json PlnSemanticAnalyzer::sa_struct_var_decl(const json& stmt, VarStorage storage)
{
	const string& structName = stmt["vars"][0]["var-type"]["type-name"].get<string>();
	const StructDef& def = requireCompleteStruct(structName, stmt);
	json pntr_type = {{"type-kind","pntr"},
	                  {"base-type",{{"type-kind","struct"},{"type-name",structName}}}};
	json result = json::array();
	json sa_stmt = {{"stmt-type","var-decl"},{"vars",json::array()}};
	// sa_var_decl gives each initialized var its own run, so these only follow
	// the var-decl that allocates their storage.
	json initStmts = json::array();
	json post = json::array();
	// LCOV_EXCL_EXCEPTION_BR_START
	for (auto& var : stmt["vars"]) {
		string name = var["name"].get<string>();
		json structRetCall;  // stays null unless `var` has a valid struct-returning C call initializer
		json copySrc;        // stays null unless `var` is initialized by copying another struct
		json takenOver;      // stays null unless `var` takes over a struct returned by a Palan call
		if (var.contains("init")) {
			// Evaluate before declareVar: the initializer must not see the
			// variable it is initializing (same rule as sa_var_decl).
			json userInit = sa_expression(var["init"]);
			bool isStructRetCall = userInit.contains("value-type")
				&& userInit.value("expr-type", "") == "call"
				&& userInit.value("func-type", "") == "c"
				&& userInit["value-type"].value("type-kind","") == "struct"
				&& userInit["value-type"].value("type-name","") == structName;
			if (isExpiringOwned(userInit) && userInit["value-type"]["base-type"] == pntr_type["base-type"]) {
				// The variable becomes the owner of the returned struct, as in
				// `(T v) = f()`, instead of allocating storage only to copy over it.
				takenOver = move(userInit);
			} else if (!isStructRetCall) {
				// A struct of another type is left to makeCopyStmt's shape check.
				if (!userInit.contains("value-type")
						|| (userInit.value("category", "") == "expiring" && !isExpiringOwned(userInit))) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructInitNotSupported, structName) << endl;
					exit(1);
				}
				copySrc = move(userInit);
			} else {
				userInit.erase("value-type");
				json ebs = eightbytesJson(def, structDefs_);
				bool inMemory = ebs.empty();
				userInit["struct-ret"] = {{"var",name}, {"struct-name",structName},
				                          {"size",def.totalSize}, {"eightbytes",move(ebs)}};
				if (inMemory) {
					// MEMORY class (>16 bytes): prepend the destination pointer as an
					// ordinary first argument -- the callee writes the whole struct
					// through it directly, so codegen needs no struct-ret-specific
					// lowering for this class.
					json var_id = {{"expr-type","id"},{"name",name},
					               {"var-type",pntr_type},{"value-type",pntr_type}};
					if (!userInit.contains("args")) userInit["args"] = json::array();
					userInit["args"].insert(userInit["args"].begin(), var_id);
				}
				structRetCall = {{"stmt-type","expr"},{"body",userInit}};
			}
		}
		declareVar(name, pntr_type, &stmt);
		json var_id = {{"expr-type","id"},{"name",name},{"var-type",pntr_type},{"value-type",pntr_type}};

		json init;
		if (!takenOver.is_null() && storage == VarStorage::Heap) {
			init = move(takenOver);
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, pntr_type));
		} else {
			json heapInit;
			bool fresh = takenOver.is_null();
			if (!fresh) {
				// Frame storage can't take the returned block over: its contents,
				// owned fields included, move into the frame and only the block is freed.
				string temp = "__tmp_" + to_string(tempVarCounter_++);
				json tempId = {{"expr-type","id"},{"name",temp},{"var-type",pntr_type},{"value-type",pntr_type}};
				json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
				json size_arg = {{"expr-type","lit-uint"},{"value",to_string(def.totalSize)},{"value-type",uint64_type}};
				json body = json::array({
					{{"stmt-type","var-decl"},{"vars",json::array({{{"name",temp},{"var-type",pntr_type},{"init",move(takenOver)}}})}},
					{{"stmt-type","expr"},{"body",{{"expr-type","call"},{"name","memcpy"},{"func-type","c"},
						{"args",json::array({var_id, tempId, size_arg})}}}},
					makeFreeStmt(temp, pntr_type)});
				initStmts.push_back({{"stmt-type","block"},{"body",move(body)}});
				heapInit = json::object();
			} else {
				heapInit = structAllocExpr(structName);
			}
			init = placeAggregateVar(stmt, name, pntr_type, move(heapInit), makeOwnedFreeStmt(name, pntr_type),
			                         storage, def.totalSize, def.maxAlign, post,
			                         structShape(structName));
		}

		sa_stmt["vars"].push_back({{"name",name},{"var-type",pntr_type},{"init",init}});
		if (!structRetCall.is_null())
			initStmts.push_back(structRetCall);
		if (!copySrc.is_null())
			initStmts.push_back(makeCopyStmt(stmt, var_id, copySrc));
	}
	result.push_back(sa_stmt);
	for (auto& s : post)
		result.push_back(s);
	for (auto& s : initStmts)
		result.push_back(s);
	// LCOV_EXCL_EXCEPTION_BR_STOP
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE
