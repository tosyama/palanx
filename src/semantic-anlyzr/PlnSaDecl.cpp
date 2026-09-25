/// Palan Semantic Analyzer — declaration analysis
///
/// @file PlnSaDecl.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

static int alignUp(int val, int align) { return (val + align - 1) & ~(align - 1); }

static StructDef buildStructDef(const string& name,
                                const json& fields,
                                const map<string, StructDef>& structDefs,
                                bool isUnion = false)
{
	StructDef def;
	def.name = name;
	def.isUnion = isUnion;
	int offset = 0, maxAlign = 1;
	// Returns the new field's offset; a union overlays every field at 0 and
	// sizes to its largest member instead of advancing past each one.
	auto place = [&](int64_t size, int align) {
		maxAlign = max(maxAlign, align);
		if (isUnion) {
			offset = max<int64_t>(offset, size);
			return 0;
		}
		int at = alignUp(offset, align);
		offset = at + size;
		return at;
	};
	for (auto& f : fields) {
		const json& vtype = f["var-type"];
		string tk = vtype.value("type-kind", "");
		string fieldName = f["name"].get<string>();

		if (tk == "prim") {
			string tname = vtype["type-name"].get<string>();
			if (structDefs.count(tname)) {
				const StructDef& target = structDefs.at(tname);
				if (!target.isComplete) {
					cerr << PlnSaMessage::getMessage(E_IncompleteStructType, tname,
					                                  target.incompleteReason, target.keyword()) << endl;
					exit(1);
				}
				int sz = 8, align = 8;
				int at = place(sz, align);
				def.fields.push_back({.name=fieldName, .typeKind="struct-ptr",
				                      .typeName=tname, .isMutable=false,
				                      .offset=at, .size=sz});
				def.hasOwnedStructFields = true;
			} else {
				int sz = elemSizeBytes(tname);
				if (sz < 0) {
					cerr << PlnSaMessage::getMessage(E_UnknownStructType, tname) << endl;
					exit(1);
				}
				int at = place(sz, sz);
				def.fields.push_back({.name=fieldName, .typeKind="prim",
				                      .typeName=tname, .isMutable=false,
				                      .offset=at, .size=sz});
			}
		} else if (tk == "embed") {
			string structName = vtype["base-type"]["type-name"].get<string>();
			if (structName == name) {
				cerr << PlnSaMessage::getMessage(E_RecursiveStruct, name) << endl;
				exit(1);
			}
			if (!structDefs.count(structName)) {
				cerr << PlnSaMessage::getMessage(E_UnknownStructType, structName) << endl;
				exit(1);
			}
			const StructDef& sub = structDefs.at(structName);
			if (!sub.isComplete) {
				cerr << PlnSaMessage::getMessage(E_IncompleteStructType, structName,
				                                  sub.incompleteReason, sub.keyword()) << endl;
				exit(1);
			}
			int align = sub.maxAlign;
			int at = place(sub.totalSize, align);
			def.fields.push_back({.name=fieldName, .typeKind="embed",
			                      .typeName=structName, .isMutable=false,
			                      .offset=at, .size=sub.totalSize});
		} else if (tk == "pntr") {
			int sz = 8, align = 8;
			string baseName = vtype["base-type"].value("type-name", "");
			bool isMut = vtype.value("mutable", false);
			string elemKind = isPrimPointeeName(baseName) ? "prim" : "struct";
			int at = place(sz, align);
			def.fields.push_back({.name=fieldName, .typeKind="raw-ptr",
			                      .typeName=baseName, .isMutable=isMut,
			                      .offset=at, .size=sz, .elemKind=elemKind});
		} else if (tk == "arr") {
			const json& size_expr = vtype["size-expr"];
			if (size_expr.is_null()) {
				// []T (unsized array) -- not a valid struct field form
				cerr << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
				exit(1);
			}
			string set = size_expr.value("expr-type", "");
			if (set != "lit-int" && set != "lit-uint") {
				cerr << PlnSaMessage::getMessage(E_ArrFieldSizeNotConstant) << endl;
				exit(1);
			}
			int64_t count = stoll(size_expr["value"].get<string>());

			if (!vtype.value("embedded", false)) {
				const json& base_wrap = vtype["base-type"];
				string base_kind = base_wrap.value("type-kind", "");
				if (base_kind == "pntr") {
					// [n]@T / [n]@!T: embedded array of n non-owning pointer slots (8B each)
					bool isMut = base_wrap.value("mutable", false);
					string leaf_name = base_wrap["base-type"].value("type-name", "");
					string elemKind = structDefs.count(leaf_name) ? "struct" : "prim";

					int align = 8;
					int at = place(count*8, align);
					def.fields.push_back({.name=fieldName, .typeKind="embed-ptr-arr",
					                      .typeName=leaf_name, .isMutable=isMut,
					                      .offset=at, .size=(int)(count*8),
					                      .count=count, .elemKind=elemKind, .stride=8});
					continue;
				}
				// [n]T: owned pointer array (field is an 8B pointer, cascade alloc/free)
				string leaf_name = base_wrap.value("type-name", "");
				if (base_kind == "prim" && !structDefs.count(leaf_name)) {
					// primitive leaf
					int stride = elemSizeBytes(leaf_name);
					if (stride < 0) {
						cerr << PlnSaMessage::getMessage(E_UnknownStructType, leaf_name) << endl;
						exit(1);
					}
					int align = 8;
					int at = place(8, align);
					def.fields.push_back({.name=fieldName, .typeKind="arr-ptr",
					                      .typeName=leaf_name, .isMutable=false,
					                      .offset=at, .size=8,
					                      .count=count, .elemKind="prim", .stride=stride});
					def.hasOwnedArrayFields = true;
					continue;
				}
				// struct leaf ([n]Point): owned pointer array, cascades to the
				// existing __pln_alloc_arr_T/__pln_free_arr_T allocator helpers
				const StructDef& leafDef = structDefs.at(leaf_name);
				if (!leafDef.isComplete) {
					cerr << PlnSaMessage::getMessage(E_IncompleteStructType, leaf_name,
					                                  leafDef.incompleteReason, leafDef.keyword()) << endl;
					exit(1);
				}
				int align = 8;
				int at = place(8, align);
				def.fields.push_back({.name=fieldName, .typeKind="arr-ptr",
				                      .typeName=leaf_name, .isMutable=false,
				                      .offset=at, .size=8,
				                      .count=count, .elemKind="struct", .stride=8});
				def.hasOwnedArrayFields = true;
				continue;
			}

			const json& base = vtype["base-type"];
			string base_kind = base.value("type-kind", "");
			string leaf_name = base.value("type-name", "");

			int stride = 0;
			string elemKind;
			int align;
			if (base_kind == "prim" && leaf_name == name) {
				cerr << PlnSaMessage::getMessage(E_RecursiveStruct, name) << endl;
				exit(1);
			} else if (base_kind == "prim" && !structDefs.count(leaf_name)) {
				// primitive leaf
				stride = elemSizeBytes(leaf_name);
				if (stride < 0) {
					cerr << PlnSaMessage::getMessage(E_UnknownStructType, leaf_name) << endl;
					exit(1);
				}
				elemKind = "prim";
				align = stride;
			} else if (base_kind == "prim") {
				// struct leaf ([n]$Point)
				const StructDef& leafDef = structDefs.at(leaf_name);
				if (!leafDef.isComplete) {
					cerr << PlnSaMessage::getMessage(E_IncompleteStructType, leaf_name,
					                                  leafDef.incompleteReason, leafDef.keyword()) << endl;
					exit(1);
				}
				if (leafDef.hasOwnedStructFields) {
					cerr << PlnSaMessage::getMessage(E_EmbedArrOwnedSubStruct) << endl;
					exit(1);
				}
				stride = leafDef.totalSize;
				elemKind = "struct";
				align = leafDef.maxAlign;
			} else {
				// base_kind == "arr" ([n]$[m]T nested) -- not supported
				cerr << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
				exit(1);
			}

			int at = place(count*stride, align);
			def.fields.push_back({.name=fieldName, .typeKind="embed-arr",
			                      .typeName=leaf_name, .isMutable=false,
			                      .offset=at, .size=(int)(count*stride),
			                      .count=count, .elemKind=elemKind, .stride=stride});
		} else {
			cerr << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
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
	json body = sa_expression(stmt["body"]);
	// A bare `struct` value-type only comes from a C function's by-value
	// struct return; discarding it as a statement has no codegen lowering,
	// so it's rejected here rather than adding one.
	if (body.contains("value-type") && body["value-type"].value("type-kind","") == "struct") {
		string structName = body["value-type"].value("type-name", "");
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ByvalStructRetDiscarded, structName) << endl;
		exit(1);
	}
	return {
		{"stmt-type", "expr"},
		{"body", body}
	};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	// Resolve type-alias names (e.g. "FILE" -> struct "_IO_FILE") at every
	// level before dispatching, so a struct reached only through an alias
	// doesn't fall through to the plain scalar var-decl path below.
	json stmt2 = stmt;
	for (auto& var : stmt2["vars"])
		if (var.contains("var-type"))
			var["var-type"] = resolveTypeAliasDeep(var["var-type"]);

	auto isArrLit = [](const json& var) {
		return var["var-type"].value("type-kind", "") == "arr" && var.contains("init");
	};
	for (auto& var : stmt2["vars"]) {
		if (isArrLit(var) && var["init"].value("expr-type", "") != "arr-lit") {
			cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_ArrVarInitNotLiteral, var["name"]) << endl;
			exit(1);
		}
	}

	// The lowering paths below apply one var-type to every var in a statement,
	// so the declaration is split into runs of equal var-type. The comparison
	// includes loc, so only vars inheriting the same source type node (`[f()]int32 a, b`)
	// share one evaluation of its size expression. Each array literal fixes its own
	// variable's size (`[]int32 a = [1,2], b = [1,2,3]`), so it always gets its own run.
	json result = json::array();
	const json& vars = stmt2["vars"];
	size_t begin = 0;
	for (size_t i = 1; i <= vars.size(); i++) {
		if (i < vars.size() && vars[i]["var-type"] == vars[i-1]["var-type"]
				&& !isArrLit(vars[i]) && !isArrLit(vars[i-1]))
			continue;
		json run = stmt2;
		run["vars"] = json(vars.begin() + begin, vars.begin() + i);
		json lowered = isArrLit(vars[begin]) ? sa_arr_lit_var_decl(run) : sa_var_decl_group(run);
		for (auto& s : lowered)
			result.push_back(move(s));
		begin = i;
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl_group(const json& stmt2)
{
	const json& vtype = stmt2["vars"][0]["var-type"];
	string tk = vtype.value("type-kind", "");

	if (tk == "arr" && vtype.value("specifier", "") == "raw" && vtype["size-expr"].is_null()) {
		cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_UnsizedArrVarDecl) << endl;
		exit(1);
	}
	if (tk == "arr" && vtype.value("specifier", "") == "raw"
			&& !vtype["size-expr"].is_null()
			&& vtype.value("embedded", false))
		return sa_embed_arr_var_decl(stmt2);
	if (tk == "arr" && vtype.value("specifier", "") == "raw" && !vtype["size-expr"].is_null()) {
		const json& base = vtype["base-type"];
		if (base.value("type-kind","") == "prim" && structDefs_.count(base.value("type-name","")))
			return sa_owned_struct_arr_var_decl(stmt2);
		return sa_arr_var_decl(stmt2);
	}
	if (tk == "prim") {
		string tname = vtype.value("type-name", "");
		if (structDefs_.count(tname))
			return sa_struct_var_decl(stmt2);
		if (!isKnownTypeName(tname)) {
			cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_UnknownStructType, tname) << endl;
			exit(1);
		}
	}
	if (tk == "pntr") {
		// Validate the pointee name at declaration time so `@!NoSuchStruct p;`
		// is rejected here rather than aborting later via an unguarded throw.
		const json* base = &vtype["base-type"];
		while (base->value("type-kind","") == "pntr")
			base = &(*base)["base-type"];
		if (base->value("type-kind","") == "prim") {
			string tname = base->value("type-name", "");
			if (!isKnownPointeeTypeName(tname)) {
				cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_UnknownStructType, tname) << endl;
				exit(1);
			}
		}
	}

	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}};
	for (auto& var : stmt2["vars"]) {
		string name = var["name"];
		json sa_var = var;
		json varType = deepNormalizePrimToStruct(var["var-type"]);
		sa_var["var-type"] = varType;
		if (var.contains("init")) {
			// Evaluate init before declaring the variable so that the variable
			// itself is not in scope during its own initializer (e.g. `int32 z = z+1`
			// should be an error, not silently read uninitialized z).
			const PlnType* toType = registry_.fromJson(varType);
			json init = sa_expression(var["init"], toType);
			if (!init.contains("value-type")) {
				cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
				exit(1);
			}
			init = convertForBinding(stmt2, init, toType, varType);
			if (!ptrPermissionOk(init["value-type"], varType)) {
				cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
				exit(1);
			}
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

		json result = json::array();
		for (auto& var : stmt["vars"]) {
			string name = var["name"];
			string d0_name = "__" + name + "_d0";

			json d0_expr = sa_arr_size_expr(stmt, vtype["size-expr"]);
			json n_expr  = sa_arr_size_expr(stmt, base_type["size-expr"]);

			json d0_id = {{"expr-type","id"},{"name",d0_name},
			              {"var-type",uint64_type},{"value-type",uint64_type}};
			json mat_id = {{"expr-type","id"},{"name",name},
			               {"var-type",pntr_type},{"value-type",pntr_type}};

			declareVar(d0_name, uint64_type, &stmt);
			// LCOV_EXCL_EXCEPTION_BR_START
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",d0_name},{"var-type",uint64_type},{"init",d0_expr}
			}})}});
			// LCOV_EXCL_EXCEPTION_BR_STOP

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
			// LCOV_EXCL_EXCEPTION_BR_START
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",name},{"var-type",pntr_type},{"init",alloc_call}
			}})}});
			// LCOV_EXCL_EXCEPTION_BR_STOP
		}
		return result;
	} // LCOV_EXCL_EXCEPTION_BR_LINE

	int elem_size;
	json sa_elem_type;
	if (base_type.value("type-kind","") == "pntr") {
		const json& inner = base_type["base-type"];
		elem_size = 8;
		if (inner.value("type-kind","") == "prim" && structDefs_.count(inner.value("type-name",""))) {
			// [n]@T / [n]@!T: elem is non-owning pntr to struct → SA elem = pntr(struct(T), mutable:bool)
			json struct_type = {{"type-kind","struct"},{"type-name",inner["type-name"]}};
			sa_elem_type = {{"type-kind","pntr"},{"base-type",struct_type},
			                {"mutable",base_type.value("mutable", false)}};
		} else {
			// [n]@![]T: elem is mutable pntr to unsized arr → SA elem = pntr(T)
			sa_elem_type = unsizedArrToPntr(inner);
		}
	} else {
		elem_size = elemSizeBytes(base_type.value("type-name",""));
		sa_elem_type = base_type;
	}

	json sa_count = sa_arr_size_expr(stmt, vtype["size-expr"]);

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

json PlnSemanticAnalyzer::sa_arr_lit_var_decl(const json& stmt)
{
	const json& var = stmt["vars"][0];
	string name = var["name"];
	const json& vtype = var["var-type"];
	const json& base = vtype["base-type"];
	const json& items = var["init"]["items"];

	if (base.value("type-kind", "") == "arr") {
		cerr << locPrefix(var["init"]) << PlnSaMessage::getMessage(E_ArrLitContext) << endl;
		exit(1);
	}
	string leafName = base.value("type-name", "");
	if (base.value("type-kind", "") != "prim" || vtype.value("embedded", false)
			|| structDefs_.count(leafName)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitElemType, name) << endl;
		exit(1);
	}
	if (!isKnownTypeName(leafName)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownStructType, leafName) << endl;
		exit(1);
	}
	const PlnType* elemType = registry_.fromJson(base);

	// Elements are analyzed before the variable is declared, so an element
	// cannot read the array it is initializing.
	json values = json::array();
	for (auto& item : items) {
		if (item.value("expr-type", "") == "arr-lit") {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, name) << endl;
			exit(1);
		}
		json value = sa_expression(item, elemType);
		if (!value.contains("value-type")) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
			exit(1);
		}
		values.push_back(convertForBinding(item, value, elemType, base));
	}

	json declStmt = stmt;
	json& declVar = declStmt["vars"][0];
	declVar.erase("init");
	if (vtype["size-expr"].is_null()) {
		declVar["var-type"]["size-expr"] = {{"expr-type", "lit-uint"}, {"value", to_string(items.size())}};
	} else {
		json sz = sa_arr_size_expr(stmt, vtype["size-expr"]);
		const json& lit = sz.value("expr-type", "") == "convert" ? sz["src"] : sz;
		string et = lit.value("expr-type", "");
		if (et != "lit-int" && et != "lit-uint") {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitSizeNotConst, name) << endl;
			exit(1);
		}
		string declared = lit["value"].get<string>();
		if (stoull(declared) != items.size()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitCountMismatch,
			                                  name, declared, to_string(items.size())) << endl;
			exit(1);
		}
	}

	json result = sa_arr_var_decl(declStmt);
	for (size_t i = 0; i < values.size(); i++) {
		json target = {
			{"expr-type", "arr-index"},
			{"array", {{"expr-type", "id"}, {"name", name}}},
			{"index", {{"expr-type", "lit-uint"}, {"value", to_string(i)}}}
		};
		result.push_back({{"stmt-type", "arr-assign"}, {"target", sa_expression(target)}, {"value", values[i]}});
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_embed_arr_var_decl(const json& stmt)
{
	const json& vtype = stmt["vars"][0]["var-type"];
	string base_kind = vtype["base-type"].value("type-kind", "");

	// [n]$T: contiguous 1D struct array — single malloc(n * totalSize), free at scope exit
	if (base_kind == "prim") {
		string leaf_name = vtype["base-type"].value("type-name", "");
		if (!structDefs_.count(leaf_name)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownStructType, leaf_name) << endl;
			exit(1);
		}
		const StructDef& def = requireCompleteStruct(leaf_name, stmt);
		if (def.hasOwnedStructFields) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_EmbedArrOwnedSubStruct) << endl;
			exit(1);
		}
		int stride = def.totalSize;
		// LCOV_EXCL_EXCEPTION_BR_START
		json uint64_type  = {{"type-kind","prim"},{"type-name","uint64"}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
		// LCOV_EXCL_EXCEPTION_BR_START
		json struct_base = {{"type-kind","struct"},{"type-name",leaf_name}};
		json pntr_type   = {{"type-kind","pntr"},{"embedded",true},{"stride",stride},{"base-type",struct_base}};
		// LCOV_EXCL_EXCEPTION_BR_STOP
		json result = json::array();
		for (auto& var : stmt["vars"]) {
			string name = var["name"];
			json sa_outer = sa_arr_size_expr(stmt, vtype["size-expr"]);
			// LCOV_EXCL_EXCEPTION_BR_START
			json stride_lit = {{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}};
			json size_arg   = {{"expr-type","mul"},{"value-type",uint64_type},{"left",sa_outer},{"right",stride_lit}};
			json malloc_call = {{"expr-type","call"},{"name","malloc"},{"func-type","c"},
			                    {"args",json::array({size_arg})},{"value-type",pntr_type}};
			// LCOV_EXCL_EXCEPTION_BR_STOP
			declareVar(name, pntr_type, &stmt);
			arrayScopeVars_.back().push_back({name, makeFreeStmt(name, pntr_type)});
			// LCOV_EXCL_EXCEPTION_BR_START
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",name},{"var-type",pntr_type},{"init",malloc_call}
			}})}});
			// LCOV_EXCL_EXCEPTION_BR_STOP
		}
		return result;
	}

	// [n]$[m]T: contiguous 2D array — single malloc(n * stride), free at scope exit
	const json& inner_arr = vtype["base-type"];   // [m]T part
	const json& leaf_type = inner_arr["base-type"]; // T (prim)
	const json& inner_sz  = inner_arr["size-expr"]; // m AST node

	string leaf_name = leaf_type.value("type-name","");
	int elem_size = elemSizeBytes(leaf_name);

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	// LCOV_EXCL_EXCEPTION_BR_STOP

	bool inner_is_const = inner_sz.value("expr-type","") == "lit-int"
	                   || inner_sz.value("expr-type","") == "lit-uint";

	json result = json::array();
	for (auto& var : stmt["vars"]) {
		string name = var["name"];

		json sa_outer = sa_arr_size_expr(stmt, vtype["size-expr"]); // n
		json sa_inner = sa_arr_size_expr(stmt, inner_sz);            // m

		json pntr_type;
		json size_arg;

		if (inner_is_const) {
			int64_t m_val = stoll(inner_sz["value"].get<string>());
			int64_t stride = m_val * elem_size;
			// LCOV_EXCL_EXCEPTION_BR_START
			pntr_type = {{"type-kind","pntr"},{"embedded",true},
			             {"inner-size",m_val},{"base-type",leaf_type}};
			json stride_lit = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			};
			size_arg = {
				{"expr-type","mul"},{"value-type",uint64_type},
				{"left",sa_outer},{"right",stride_lit}
			};
			// LCOV_EXCL_EXCEPTION_BR_STOP
		} else {
			string d1_name = "__" + name + "_d1";
			// LCOV_EXCL_EXCEPTION_BR_START
			pntr_type = {{"type-kind","pntr"},{"embedded",true},{"base-type",leaf_type}};

			json d1_id = {{"expr-type","id"},{"name",d1_name},
			              {"var-type",uint64_type},{"value-type",uint64_type}};
			json elem_lit = {
				{"expr-type","lit-uint"},{"value",to_string(elem_size)},{"value-type",uint64_type}
			};
			json d1_times_elem = {
				{"expr-type","mul"},{"value-type",uint64_type},
				{"left",d1_id},{"right",elem_lit}
			};
			size_arg = {
				{"expr-type","mul"},{"value-type",uint64_type},
				{"left",sa_outer},{"right",d1_times_elem}
			};
			// LCOV_EXCL_EXCEPTION_BR_STOP

			declareVar(d1_name, uint64_type, &stmt);
			// LCOV_EXCL_EXCEPTION_BR_START
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
				{"name",d1_name},{"var-type",uint64_type},{"init",sa_inner}
			}})}});
			// LCOV_EXCL_EXCEPTION_BR_STOP
		}

		// LCOV_EXCL_EXCEPTION_BR_START
		json malloc_call = {
			{"expr-type","call"},{"name","malloc"},{"func-type","c"},
			{"args",json::array({size_arg})},{"value-type",pntr_type}
		};
		// LCOV_EXCL_EXCEPTION_BR_STOP
		declareVar(name, pntr_type, &stmt);
		arrayScopeVars_.back().push_back({name, makeFreeStmt(name, pntr_type)});
		// LCOV_EXCL_EXCEPTION_BR_START
		result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
			{"name",name},{"var-type",pntr_type},{"init",malloc_call}
		}})}});
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_struct_def(const json& stmt)
{
	string name = stmt["name"].get<string>();
	json fields = stmt["fields"];
	for (auto& f : fields)
		f["var-type"] = resolveTypeAliasDeep(f["var-type"]);
	structDefs_[name] = buildStructDef(name, fields, structDefs_);
	return json::array();
} // LCOV_EXCL_EXCEPTION_BR_LINE

// c2ast has no concept of Palan's $T inline-embedding sugar, so it represents
// a C struct/union-by-value field as a plain tag; translate to the "embed" shape
// buildStructDef expects.
static json cFieldVarType(const json& vtype)
{
	if (isCRecordTag(vtype)) {
		return {{"type-kind", "embed"},
		        {"base-type", {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}}}};
	}
	if (vtype.value("type-kind", "") == "arr" && vtype.contains("base-type")) {
		json v = vtype;
		json& bt = v["base-type"];
		// Inside an array, a struct-by-value leaf is Palan's [n]$Foo / [n]Foo
		// shape, named with a plain prim type-name rather than wrapped in "embed".
		if (isCRecordTag(bt)) {
			bt = {{"type-kind", "prim"}, {"type-name", bt["type-name"]}};
		} else if (bt.value("type-kind", "") == "pntr") {
			// An inline array of pointer slots ("T *field[n];") is Palan's
			// [n]@T / [n]@!T shape; c2ast sets "embedded" here unlike native
			// syntax, so strip it for a single embed-ptr-arr shape either way.
			v.erase("embedded");
			json& leaf = bt["base-type"];
			if (isCRecordTag(leaf)) {
				leaf = {{"type-kind", "prim"}, {"type-name", leaf["type-name"]}};
			}
		}
		return v;
	}
	return vtype;
}

// An unsupported field type (e.g. a C bitfield/anonymous union) registers its
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

void PlnSemanticAnalyzer::registerCStruct(const json& s)
{
	string name = s["name"].get<string>();
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
		json vt = cFieldVarType(f["var-type"]);
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
		fields.push_back({{"name", f["name"]}, {"var-type", vt}});
	}
	structDefs_[name] = buildStructDef(name, fields, structDefs_, s.value("union", false));
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_type_alias(const json& stmt)
{
	string name = stmt["name"].get<string>();
	// Resolve one level so chains of aliases (alias-of-alias) collapse to the
	// base type at registration time; later lookups need only one map access.
	typeAliases_[name] = resolveTypeAlias(stmt["type"]);
	return json::array();
}

json PlnSemanticAnalyzer::sa_const_decl(const json& stmt)
{
	string name = stmt["name"].get<string>();
	json value = sa_expression(stmt["value"]);
	string et = value.value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint" && et != "lit-flo" && et != "lit-str") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ConstNotCompileTimeValue, name) << endl;
		exit(1);
	}
	constDecls_[name] = {{"value", value}, {"value-type", value["value-type"]}};
	return json::array();
}

json PlnSemanticAnalyzer::resolveTypeAlias(const json& vtype) const
{
	if (vtype.value("type-kind", "") == "prim") {
		string tname = vtype.value("type-name", "");
		if (!structDefs_.count(tname)) {
			auto it = typeAliases_.find(tname);
			if (it != typeAliases_.end())
				return it->second;
		}
	}
	return vtype;
}

// Like resolveTypeAlias, but also resolves alias names nested inside pntr/arr
// wrappers (e.g. "[3]PT", "@!PT" or "$PT"). Unlike deepNormalizePrimToStruct, this
// leaves a resolved name as "prim" rather than "struct", so callers' existing
// structDefs_.count(type-name) checks keep working.
json PlnSemanticAnalyzer::resolveTypeAliasDeep(const json& vtype) const
{
	json resolved = resolveTypeAlias(vtype);
	string tk = resolved.value("type-kind", "");
	if ((tk == "pntr" || tk == "arr" || tk == "embed") && resolved.contains("base-type"))
		resolved["base-type"] = resolveTypeAliasDeep(resolved["base-type"]);
	return resolved;
}

json PlnSemanticAnalyzer::sa_struct_var_decl(const json& stmt)
{
	const string& structName = stmt["vars"][0]["var-type"]["type-name"].get<string>();
	const StructDef& def = requireCompleteStruct(structName, stmt);
	json pntr_type = {{"type-kind","pntr"},
	                  {"base-type",{{"type-kind","struct"},{"type-name",structName}}}};
	// Inside T's own generated allocator, `T p;` must be the bare calloc or
	// __pln_alloc_T would recurse into itself.
	bool inOwnAllocator = currentFunc_
		&& (*currentFunc_)["name"] == "__pln_alloc_" + structName; // LCOV_EXCL_EXCEPTION_BR_LINE
	bool useSimpleCalloc = (!def.hasOwnedStructFields && !def.hasOwnedArrayFields) || inOwnAllocator;
	json result = json::array();
	json sa_stmt = {{"stmt-type","var-decl"},{"vars",json::array()}};
	// Accumulated separately so `Pair p = f(), q = g();` allocates storage for
	// both before either struct-ret call runs.
	json structRetStmts = json::array();
	// LCOV_EXCL_EXCEPTION_BR_START
	for (auto& var : stmt["vars"]) {
		string name = var["name"].get<string>();
		json structRetCall;  // stays null unless `var` has a valid struct-returning C call initializer
		if (var.contains("init")) {
			// Evaluate before declareVar: the initializer must not see the
			// variable it is initializing (same rule as sa_var_decl).
			json userInit = sa_expression(var["init"]);
			bool isStructRetCall = userInit.contains("value-type")
				&& userInit.value("expr-type", "") == "call"
				&& userInit.value("func-type", "") == "c"
				&& userInit["value-type"].value("type-kind","") == "struct"
				&& userInit["value-type"].value("type-name","") == structName;
			if (!isStructRetCall) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructInitNotSupported, structName) << endl;
				exit(1);
			}
			bool ok;
			vector<EightbyteRet> eightbytes = classifySysVStructRet(def, structDefs_, ok);
			if (!ok || !useSimpleCalloc) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnsupportedCStructReturn, structName) << endl;
				exit(1);
			}
			userInit.erase("value-type");
			json ebs = json::array();
			for (auto& eb : eightbytes)
				ebs.push_back({{"class", eb.cls == EightbyteClass::Integer ? "int" : "sse"}, {"size", eb.size}});
			userInit["struct-ret"] = {{"var",name}, {"struct-name",structName},
			                          {"size",def.totalSize}, {"eightbytes",ebs}};
			if (eightbytes.empty()) {
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
		declareVar(name, pntr_type, &stmt);

		json init;
		json free_stmt;

		if (useSimpleCalloc) {
			json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
			json size_arg = {{"expr-type","lit-int"},{"value",to_string(def.totalSize)},
			                 {"value-type",uint64_type}};
			json one_arg  = {{"expr-type","lit-int"},{"value","1"},
			                 {"value-type",uint64_type}};
			init = {{"expr-type","call"},{"name","calloc"},{"func-type","c"},
			        {"args",json::array({one_arg, size_arg})},{"value-type",pntr_type}};
			free_stmt = makeFreeStmt(name, pntr_type);
		} else {
			recordAllocShape(structName);
			string alloc_fn = "__pln_alloc_" + structName;
			string free_fn  = "__pln_free_"  + structName;
			init = {{"expr-type","call"},{"name",alloc_fn},{"func-type","pln"},
			        {"args",json::array()},{"value-type",pntr_type}};
			free_stmt = makePlanFreeStmt(name, pntr_type, free_fn);
		}

		bool namedRet = isNamedReturnVar(name);
		if (!namedRet)
			arrayScopeVars_.back().push_back({name, free_stmt});
		sa_stmt["vars"].push_back({{"name",name},{"var-type",pntr_type},{"init",init}});
		if (!structRetCall.is_null())
			structRetStmts.push_back(structRetCall);
	}
	result.push_back(sa_stmt);
	for (auto& s : structRetStmts)
		result.push_back(s);
	// LCOV_EXCL_EXCEPTION_BR_STOP
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_owned_struct_arr_var_decl(const json& stmt)
{
	// [n]T (T = struct): owned pointer array.
	// __pln_alloc_arr_T(n) on declaration, __pln_free_arr_T(pts, n) at scope exit.
	const json& vtype = stmt["vars"][0]["var-type"];
	const json& base_type = vtype["base-type"];
	string struct_name = base_type["type-name"].get<string>();
	requireCompleteStruct(struct_name, stmt);  // recordAllocShape below needs totalSize/fields

	string shape_key = "arr_" + struct_name;
	bool found = false;
	for (auto& s : sa["alloc-shapes"])
		if (s.value("shape-key","") == shape_key) { found = true; break; }
	if (!found) {
		// struct shape must be present for build-mgr to know the field layout
		recordAllocShape(struct_name);
		sa["alloc-shapes"].push_back({
			{"shape-kind","arr-struct"}, {"shape-key",shape_key}, {"struct-name",struct_name}
		});
	}

	string alloc_func = "__pln_alloc_" + shape_key;
	string free_func  = "__pln_free_"  + shape_key;

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	json struct_type = {{"type-kind","struct"},{"type-name",struct_name}};
	json elem_pntr   = {{"type-kind","pntr"},{"base-type",struct_type}};
	json pntr_type   = {{"type-kind","pntr"},{"base-type",elem_pntr}};
	// LCOV_EXCL_EXCEPTION_BR_STOP

	json result = json::array();
	for (auto& var : stmt["vars"]) {
		string name = var["name"];
		string n_name = "__" + name + "_n";

		json n_expr = sa_arr_size_expr(stmt, vtype["size-expr"]);

		// LCOV_EXCL_EXCEPTION_BR_START
		json n_id = {{"expr-type","id"},{"name",n_name},
		             {"var-type",uint64_type},{"value-type",uint64_type}};
		json arr_id = {{"expr-type","id"},{"name",name},
		               {"var-type",pntr_type},{"value-type",pntr_type}};
		// LCOV_EXCL_EXCEPTION_BR_STOP

		declareVar(n_name, uint64_type, &stmt);
		// LCOV_EXCL_EXCEPTION_BR_START
		result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
			{"name",n_name},{"var-type",uint64_type},{"init",n_expr}
		}})}});
		// LCOV_EXCL_EXCEPTION_BR_STOP

		json alloc_call = {
			{"expr-type","call"}, {"name",alloc_func}, {"func-type","palan"},
			{"args",json::array({n_id})}, {"value-type",pntr_type}
		};
		json free_stmt_json = {{"stmt-type","expr"},{"body",{
			{"expr-type","call"}, {"name",free_func}, {"func-type","palan"},
			{"args",json::array({arr_id, n_id})}
		}}};

		declareVar(name, pntr_type, &stmt);
		arrayScopeVars_.back().push_back({name, free_stmt_json});
		// LCOV_EXCL_EXCEPTION_BR_START
		result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{
			{"name",name},{"var-type",pntr_type},{"init",alloc_call}
		}})}});
		// LCOV_EXCL_EXCEPTION_BR_STOP
	}
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE
