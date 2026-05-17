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
                                const map<string, StructDef>& structDefs)
{
	StructDef def;
	def.name = name;
	int offset = 0, maxAlign = 1;
	for (auto& f : fields) {
		const json& vtype = f["var-type"];
		string tk = vtype.value("type-kind", "");
		string fieldName = f["name"].get<string>();

		if (tk == "prim") {
			string tname = vtype["type-name"].get<string>();
			if (structDefs.count(tname)) {
				int sz = 8, align = 8;
				offset = alignUp(offset, align);
				def.fields.push_back({.name=fieldName, .typeKind="struct-ptr",
				                      .typeName=tname, .isMutable=false,
				                      .offset=offset, .size=sz});
				offset += sz;
				maxAlign = max(maxAlign, align);
				def.hasOwnedStructFields = true;
			} else {
				int sz = elemSizeBytes(tname);
				if (sz < 0) {
					cerr << PlnSaMessage::getMessage(E_UnknownStructType, tname) << endl;
					exit(1);
				}
				offset = alignUp(offset, sz);
				def.fields.push_back({.name=fieldName, .typeKind="prim",
				                      .typeName=tname, .isMutable=false,
				                      .offset=offset, .size=sz});
				offset += sz;
				maxAlign = max(maxAlign, sz);
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
			int align = sub.maxAlign;
			offset = alignUp(offset, align);
			def.fields.push_back({.name=fieldName, .typeKind="embed",
			                      .typeName=structName, .isMutable=false,
			                      .offset=offset, .size=sub.totalSize});
			offset += sub.totalSize;
			maxAlign = max(maxAlign, align);
		} else if (tk == "pntr") {
			int sz = 8, align = 8;
			string baseName = vtype["base-type"].value("type-name", "");
			bool isMut = vtype.value("mutable", false);
			offset = alignUp(offset, align);
			def.fields.push_back({.name=fieldName, .typeKind="raw-ptr",
			                      .typeName=baseName, .isMutable=isMut,
			                      .offset=offset, .size=sz});
			offset += sz;
			maxAlign = max(maxAlign, align);
		} else {
			cerr << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
			exit(1);
		}
	}
	def.totalSize = alignUp(offset, maxAlign);
	def.maxAlign  = maxAlign;
	return def;
}

json PlnSemanticAnalyzer::sa_expression_stmt(const json& stmt)
{
	return {
		{"stmt-type", "expr"},
		{"body", sa_expression(stmt["body"])}
	};
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	if (!stmt["vars"].empty()) {
		const json& vtype = stmt["vars"][0]["var-type"];
		string tk = vtype.value("type-kind", "");

		if (tk == "arr" && vtype.value("specifier", "") == "raw" && vtype["size-expr"].is_null()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnsizedArrVarDecl) << endl;
			exit(1);
		}
		if (tk == "arr" && vtype.value("specifier", "") == "raw"
				&& !vtype["size-expr"].is_null()
				&& vtype.value("embedded", false))
			return sa_embed_arr_var_decl(stmt);
		if (tk == "arr" && vtype.value("specifier", "") == "raw" && !vtype["size-expr"].is_null())
			return sa_arr_var_decl(stmt);
		if (tk == "prim") {
			string tname = vtype.value("type-name", "");
			if (structDefs_.count(tname))
				return sa_struct_var_decl(stmt);
			if (elemSizeBytes(tname) < 0) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UnknownStructType, tname) << endl;
				exit(1);
			}
		}
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

json PlnSemanticAnalyzer::sa_embed_arr_var_decl(const json& stmt)
{
	// [n]$[m]T: contiguous 2D array — single malloc(n * stride), free at scope exit
	const json& vtype     = stmt["vars"][0]["var-type"];
	const json& inner_arr = vtype["base-type"];   // [m]T part
	const json& leaf_type = inner_arr["base-type"]; // T (prim)
	const json& inner_sz  = inner_arr["size-expr"]; // m AST node

	string leaf_name = leaf_type.value("type-name","");
	int elem_size = elemSizeBytes(leaf_name);

	// LCOV_EXCL_EXCEPTION_BR_START
	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}};
	// LCOV_EXCL_EXCEPTION_BR_STOP
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

	bool inner_is_const = inner_sz.value("expr-type","") == "lit-int"
	                   || inner_sz.value("expr-type","") == "lit-uint";

	json result = json::array();
	for (auto& var : stmt["vars"]) {
		string name = var["name"];

		json sa_outer = sa_expression(vtype["size-expr"], uint64Type); // n
		checkIntSize(sa_outer);
		json sa_inner = sa_expression(inner_sz, uint64Type);            // m
		checkIntSize(sa_inner);

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
	structDefs_[name] = buildStructDef(name, stmt["fields"], structDefs_);
	return json::array();
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnSemanticAnalyzer::sa_struct_var_decl(const json& stmt)
{
	const string& structName = stmt["vars"][0]["var-type"]["type-name"].get<string>();
	const StructDef& def = structDefs_[structName];
	json pntr_type = {{"type-kind","pntr"},
	                  {"base-type",{{"type-kind","struct"},{"type-name",structName}}}};
	json result = json::array();
	json sa_stmt = {{"stmt-type","var-decl"},{"vars",json::array()}};
	// LCOV_EXCL_EXCEPTION_BR_START
	for (auto& var : stmt["vars"]) {
		string name = var["name"].get<string>();
		declareVar(name, pntr_type, &stmt);

		json init;
		json free_stmt;

		bool useSimpleCalloc = !def.hasOwnedStructFields || inAllocFunc_;
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
	}
	result.push_back(sa_stmt);
	// LCOV_EXCL_EXCEPTION_BR_STOP
	return result;
} // LCOV_EXCL_EXCEPTION_BR_LINE
