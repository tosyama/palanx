/// Palan Semantic Analyzer
///
/// @file PlnSemanticAnalyzer.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <optional>
#include <filesystem>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"

using namespace std;

using json = nlohmann::json;

struct FieldLayout {
	string name;
	string typeKind;   // "prim" | "embed" | "struct-ptr" | "raw-ptr" | "embed-arr" | "embed-ptr-arr" | "arr-ptr"
	string typeName;   // prim type name, embed/struct-ptr struct name, raw-ptr base type name,
	                    // embed-arr/embed-ptr-arr/arr-ptr: leaf prim type name or struct name
	bool   isMutable;  // raw-ptr/embed-ptr-arr: @T=false, @!T=true
	int    offset;     // byte offset from struct start (C ABI aligned)
	int    size;       // prim: type size, embed: sub-struct totalSize, struct-ptr/raw-ptr/arr-ptr: 8,
	                    // embed-arr/embed-ptr-arr: count*stride

	// Array-field-only members (valid when typeKind == "embed-arr", "embed-ptr-arr", or "arr-ptr")
	int64_t count    = 0;   // element count n (compile-time constant)
	string  elemKind = "";  // "prim" | "struct"; also set for typeKind == "raw-ptr" (pointee kind)
	int     stride   = 0;   // bytes per element

	string  enumName = "";  // enum the primitive leaf is typed as, if any
};

struct StructDef {
	string name;
	vector<FieldLayout> fields;
	int  totalSize = -1;
	int  maxAlign  = 0;
	bool ownsFields = false;  // has a struct-ptr (T) or arr-ptr ([n]T) field
	// false = tag is known but its layout isn't (C incomplete-type equivalent, e.g. FILE).
	// Only usable through a pointer (@T/@!T); buildStructDef sets this true on success.
	bool   isComplete = false;
	string incompleteReason;  // "unsupported-field" or "forward-declared" when !isComplete; empty otherwise
	// A C union: every field sits at offset 0. Only layout (buildStructDef) and
	// diagnostics consult this; every other consumer sees offsets/totalSize.
	bool   isUnion = false;
	const char* keyword() const { return isUnion ? "union" : "struct"; }
};

struct FieldChain {
	bool   isPointerBased;  // false = var+offset (inline chain), true = ptr-expr needed
	string varName;         // base variable name when !isPointerBased
	int    offset;          // accumulated byte offset
	json   ptrExpr;         // partial SA json when isPointerBased
	string structName;      // struct type currently being resolved
};

class PlnSemanticAnalyzer {
	string basePath;
	string astFileName;
	string c2astPath;
	string inputFilePath;
	string moduleId_;  // canonical path of the source file, a type's origin
	json sa;
	PlnTypeRegistry registry_;
	map<string, string> strLiteralLabels;  // value -> label
	const json*       currentFunc_    = nullptr;  // null = _start level
	int               loopDepth_     = 0;        // nesting depth of while loops
	size_t            funcBodyScopeIdx_ = 0;     // 0 = top-level (no function)

	vector<map<string, json>> varScopes;
	vector<map<string, json>> cFuncScopes;
	vector<map<string, json>> cGlobalScopes;
	vector<map<string, json>> plnFuncScopes;
	// scope stack: alias("" = unqualified) → funcname → funcDef (empty json{} = ambiguous sentinel)
	vector<map<string, map<string, json>>> importScopes;

	// Array variable tracking per scope (parallel to varScopes)
	vector<vector<pair<string,json>>> arrayScopeVars_;
	// Stack of while-body scope indices (for break/continue cleanup)
	vector<size_t> whileScopeStack_;
	// Counter for generating unique temporary variable names
	int tempVarCounter_ = 0;
	// Registered struct type definitions
	map<string, StructDef> structDefs_;
	set<string>            allocShapeNames_;  // dedup guard for struct alloc-shapes
	// struct-def names of each statement list being analyzed, innermost last:
	// a pointer field may name a struct defined later in an enclosing list.
	vector<set<string>>    structDefNameScopes_;
	// Registered type aliases (name -> fully-resolved base type json)
	map<string, json>      typeAliases_;
	// cinclude'd C typedefs (name -> var-type, its own references resolved), for
	// resolving a later header's "user" reference to one of them.
	map<string, json>      cTypedefs_;
	// Registered enum types: enumerator values, and the declaration's loc so
	// the step-2 revisit of a pre-scanned enum-def is told apart from a
	// redefinition. A cinclude'd C enum has a null loc.
	struct EnumDef { map<string, int64_t> values; json loc; };
	map<string, EnumDef>   enumDefs_;
	// Defining module of each Palan type name. Type names are program-wide
	// identities (build-mgr keys struct allocators by them), so a name may come
	// from one module only. A C type has no entry.
	map<string, string>    typeOrigins_;
	// Imported type names source may not write unqualified: the module alias
	// to write them with, or "" for a type that was not selected or not
	// exported, registered only because an imported type depends on it.
	map<string, string>    hiddenTypeNames_;
	// Modules whose types are being pre-scanned, shared with the analyzers
	// created for imports so a circular import stops.
	shared_ptr<set<string>> typeImportsInProgress_ = make_shared<set<string>>();
	// Registered const declarations (name -> {"value": <SA'd literal expr>, "value-type": <type>})
	map<string, json>      constDecls_;
	// Library names collected from cinclude `link` clauses. A set: the same
	// library named by two cinclude statements is emitted once, and the
	// ordering makes the sa.json "libs" section deterministic.
	set<string>            linkLibs_;

	void enterScope();
	void leaveScope();
	json collectFreeStmts(size_t from_idx, size_t to_idx);

	string locPrefix(const json& node) const;

	void        declareVar(const string& name, const json& type, const json* loc_node = nullptr);
	const json* findVar(const string& name) const;
	bool        isLocalVar(const string& name) const;
	bool        isInArrayScope(const string& name) const;
	void        removeFromArrayScope(const string& name);

	void        registerCFunc(const string& name, const json& def);
	const json* findCFunc(const string& name) const;

	void        registerCGlobal(const string& name, const json& def);
	const json* findCGlobal(const string& name) const;

	void        registerPlnFunc(const string& name, const json& def, const json& locNode);
	const json* findPlnFunc(const string& name) const;
	const json* findImportFunc(const string& fname) const;
	const json* findImportFuncByAlias(const string& alias, const string& fname) const;

	json sa_statements(const json& stmts);
	void beginModule(const json& ast);
	void prescanTypes(const json& stmts);
	json loadImportAst(const json& stmt, filesystem::path& impPath) const;
	// An analyzer holding the imported module's pre-scanned types, or null
	// while that module is itself being pre-scanned (a circular import).
	unique_ptr<PlnSemanticAnalyzer> importTypeContext(const json& impAst, const filesystem::path& impPath) const;
	void importTypes(const json& stmt);
	void adoptExportedTypes(const json& stmt, const json& impAst, const PlnSemanticAnalyzer& sub);
	void adoptStruct(const PlnSemanticAnalyzer& sub, const string& name, bool nameable, const string& alias,
	                 const json& locNode);
	void adoptEnum(const PlnSemanticAnalyzer& sub, const string& name, bool nameable, const string& alias,
	               const json& locNode);
	void adoptTypeDeps(const PlnSemanticAnalyzer& sub, const json& type, const json& locNode);
	void setTypeNameable(const string& name, bool isNew, bool nameable, const string& alias);
	// Records `name` as defined by `origin`; false if it already was, exits if
	// another module defined it.
	bool claimTypeName(const string& name, const string& origin, const json& locNode);
	void requireNameableType(const json& locNode, const json& type) const;
	void sa_import(const json &stmt);
	void sa_cinclude(const json &stmt);
	void registerCIncludeTypes(const json& stmt); // cinclude structs/typedefs only
	json resolveConstRef(const json& expr) const;
	json sa_expression(const json &expr, const PlnType* expectedType = nullptr);
	json sa_expr_arith(const json& expr, const PlnType* expectedType);
	void checkIntLiteralRange(const json& lit);
	json sa_expr_call(const json& expr);
	json sa_expr_member_call(const json& expr);
	json normalizeMethodCall(const json& expr);
	// Analyze a call's argument list against the callee's parameter list.
	json saCallArgs(const json& locNode, const json& args, const json& funcParams,
	                bool isCFunc, const string& funcName);
	// Analyze the argument in a `_callback-param` slot: only a bare reference
	// to a Palan function is accepted, matched by exact ABI identity.
	json sa_func_ref_arg(const json& locNode, const json& arg,
	                      const string& cFuncName, const json& param);
	void checkArgPtrPermission(const json& expr, const string& funcName, bool isCFunc,
	                           const json& saArg, const json& param, size_t argIdx);
	// Shared narrowing rule for every binding site.
	json convertForBinding(const json& locNode, json value, const PlnType* toType, const json& toTypeJson);
	// Convert a single call argument to a parameter's type, diagnosing
	// E_InvalidNarrowingConv if it doesn't fit without an explicit cast.
	json convertCallArg(const json& locNode, json saArg, const json& paramVT);
	// forWrite: the result is written through (store, '@!', or a field write
	// below it), so every hop back to the chain's root must be writable --
	// the same rule resolveObjectChain applies to struct field chains.
	json sa_expr_arr_index(const json& expr, bool forWrite = false);
	json sa_expr_field_access(const json& expr, bool forWrite);
	// Evaluate an array-size sub-expression (an `[n]T` declaration's `n`, or
	// a 2D array's inner/outer dimension) and normalize the result to
	// uint64, diagnosing E_ArraySizeNotInteger for a non-integer. Every
	// caller embeds the result directly into a hand-built uint64-typed node
	// (byte-count math, or a uint64 temp var's init) without further type
	// checking, so this is the one place that must guarantee uint64 --
	// sa_expression's expectedType alone no longer does, now that a
	// macro-folded lit-int keeps its own C-declared type instead of taking
	// expectedType unconditionally.
	json sa_arr_size_expr(const json& stmt, const json& sizeExprAst);
	json sa_expression_stmt(const json& stmt);
	json sa_var_decl(const json& stmt);           // returns array of statements
	json sa_var_decl_group(const json& stmt);     // all vars share one var-type; returns array of statements
	json sa_arr_var_decl(const json& stmt);       // returns array of statements
	json sa_arr_lit_var_decl(const json& stmt);   // single var; returns array of statements
	json sa_arr_copy_var_decl(const json& stmt);  // single var; returns array of statements
	json sa_struct_lit(const json& item, const StructDef& def);
	void emitStructLitAssigns(const json& objAst, const StructDef& def, const json& values, json& out);
	string arrLitDimSize(const json& stmt, const string& name, json& sizeExpr, size_t count);
	json sa_embed_arr_var_decl(const json& stmt); // returns array of statements
	json sa_owned_struct_arr_var_decl(const json& stmt); // returns array of statements
	json sa_owned_struct_arr2d_var_decl(const json& stmt); // returns array of statements
	void recordArrStructShape(const string& structName);
	void recordArrArrShape(const string& leafName);
	void recordArrArrStructShape(const string& structName);
	void pushStructDefNames(const json& stmts);
	json sa_struct_def(const json& stmt);         // consume struct-def, register in structDefs_
	void registerCStruct(const json& cincludeStmt, const json& s);  // consume c2ast "structs" entry, register in structDefs_
	json sa_struct_var_decl(const json& stmt);    // returns array of statements
	json sa_type_alias(const json& stmt);         // consume type-alias, register in typeAliases_
	json sa_const_decl(const json& stmt);         // consume const-decl, register in constDecls_
	json sa_enum_def(const json& stmt);           // consume enum-def, register in enumDefs_/typeAliases_
	void registerEnum(const string& name, map<string, int64_t> values, const json& loc);
	void registerCEnum(const json& cincludeStmt, const json& e);
	void resolveCTypeRefs(json& node) const;      // C enum/earlier-cinclude typedef references -> SA type
	json enumTypeNamed(const string& name) const; // enum value-type for a type name, or null
	string typeNameWritten(const json& obj) const;
	json enumCast(const json& expr, const string& typeName);
	json resolveEnumerator(const json& expr);     // `Name.X` as a lit-int, or null for a field access
	int64_t typeByteSize(const json& locNode, const json& type);  // `sizeof(type)`
	void recordAllocShape(const string& structName);
	// Scope-exit release of an owned struct variable (pntrType is pntr(struct)).
	json makeStructFreeStmt(const string& name, const json& pntrType);
	json makeStructFreeCall(json ptr, const string& structName);
	pair<string, string> structFreeFunc(const string& structName);
	json makeOwnedValueFreeStmt(json value);
	bool isStructType(const json& type) const;
	// True if `name` resolves to some type: a primitive, a registered struct, or a type alias.
	bool isKnownTypeName(const string& name) const;
	// isKnownTypeName plus "void", valid only as a pointer pointee, never a standalone value type.
	bool isKnownPointeeTypeName(const string& name) const;
	// Exits with E_UnknownStructType if a name at the leaf of `type` (through arr/pntr levels) is unknown,
	// or if a `$` element is not a struct or a `$[m]` row. Struct fields don't pass through here:
	// a `[n]$int32` field is an inline array, which a variable has no counterpart of.
	// `type` must be as written in source (see requireNameableType).
	void requireKnownTypeNames(const json& locNode, const json& type) const;
	json  toStructPntrType(const json& type) const;
	json  unsizedArrToPntr(const json& locNode, const json& type);
	// A '@T'/'@!T' array element (a pointer slot) as SA represents it.
	json  ptrSlotElemType(const json& locNode, const json& type);
	void  normalizeUnsizedArrSig(json& funcDef);
	bool  isNamedReturnVar(const string& varName) const;
	json  deepNormalizePrimToStruct(const json& type) const;
	json  resolveTypeAlias(const json& vtype) const;
	json  resolveTypeAliasDeep(const json& vtype) const;
	// The canonical form of a signature type (an owned struct becomes pntr(struct)),
	// also what a tapple-decl variable's declared type is compared in.
	json  normalizeSigType(const json& type) const;
	void  normalizeStructSig(json& funcDef);
	// Diagnose (and exit) if `funcDef`'s parameters/ret-type/rets use a type
	// PlnTypeRegistry::fromJson cannot represent. Must be called after
	// normalizeStructSig -- a struct parameter is still prim(Name) before
	// that runs and would be misclassified as unsupported.
	void  validateNativeSig(const json& funcDef);
	json  normalizeArrBorrowType(const json& locNode, const json& type);
	// isMutable: the borrow's permission, or none for an owned level.
	json  sizedArrLevel(const json& locNode, const json& arr, optional<bool> isMutable);
	int64_t constLevelSize(const json& locNode, const json& sizeExprAst, bool isBorrow);
	void  normalizeArrBorrowSig(json& funcDef);
	bool  onlyNamesConsts(const json& expr) const;
	void  checkElemShape(const json& locNode, const json& saValue, const json& dstType);
	void  checkArrBorrowBinding(const json& locNode, const json& srcAst, const json& saValue,
	                            const json& dstType);
	void  checkStructBorrowSource(const json& locNode, const json& saValue, const json& dstType);
	void  registerTypeAliasChecked(const json& locNode, const string& aliasName, const json& resolved);
	void  registerTypedefAliasInType(const json& locNode, json& vtype);
	void  registerCFuncTypedefAliases(const json& locNode, json& funcEntry);
	// Shared function pre-registration sequence (normalize + validate + register)
	// used by top-level, block-local, and function-nested func-defs alike.
	void  preregisterFunc(const json& f);
	json  normalizeFuncSig(const json& f);
	// Diagnose Linux syscall ABI constraints on a syscall declaration
	// (funcDef must already be normalizeStructSig'd) and fold its
	// "syscall-number" expression node into a plain JSON integer.
	void  validateSyscallDecl(json& funcDef);
	// Copy a resolved Palan/syscall callee's func-type, ret-type (as
	// value-type) and, for a syscall, its folded syscall-number onto a call
	// node. Shared by sa_expr_call and sa_expr_member_call so the two paths
	// can't drift on which fields a syscall call carries.
	void  applyPlnCalleeSig(json& sa_expr, const json& pFunc);
	json sa_field_assign(const json& stmt);  // returns json::array()
	FieldChain resolveObjectChain(const json& obj, bool forWrite);
	json makeFieldAssign(const FieldChain& chain, const FieldLayout& field, json value);
	json makeFieldAccess(const FieldChain& chain, const FieldLayout& field);
	const FieldLayout& findFieldOrExit(const string& structName, const string& fieldName, const json& locNode);
	// Look up a struct already known to be registered (name presence must be checked
	// by the caller beforehand) and reject it if its layout isn't known yet.
	const StructDef& requireCompleteStruct(const string& structName, const json& locNode);
	// `entry` must be a cinclude'd C function entry already processed by
	// normalizeCFuncSig -- the only producer of "_unsupported-sig". No-op if
	// the signature is fully representable.
	void requireSupportedCFuncSig(const json& entry, const string& funcName, const json& locNode);
	// `entry` must be a cinclude'd C global entry already processed by
	// normalizeCGlobal -- the only producer of "_unsupported-global". No-op if
	// the type is fully representable.
	void requireSupportedCGlobal(const json& entry, const string& globalName, const json& locNode);
	json sa_expr_addr_of(const json& expr);
	void validateEmbeddedParams(const json& funcDef);
	void sa_functions(const json& funcs);
	void sa_function(const json& funcDef);
	json sa_assign_stmt(const json& stmt);
	// `src -> dst` for a dst that isCopiedByValue: a statement copying src's
	// contents into dst's storage. Diagnoses E_CopyShapeMismatch.
	json makeCopyStmt(const json& locNode, const json& dst, const json& src);
	json sa_arr_assign_stmt(const json& stmt);  // returns json::array()
	void appendTransferSourceReset(json& stmts, const json& stmt, const json& saValue);
	json sa_return_stmt(const json& stmt);
	json bindReturnValueToTemp(const json& stmt, json& ret);
	const json& findMultiRetFunc(const json& stmt, const json& callExpr, size_t recvCount);
	json sa_tapple_decl(const json& stmt);       // returns array of statements
	json sa_tapple_assign(const json& stmt);
	json sa_block(const json& stmt);
	json sa_if_stmt(const json& stmt);
	json sa_while_stmt(const json& stmt);
	json sa_break_stmt(const json& stmt);
	json sa_continue_stmt(const json& stmt);

public:
	PlnSemanticAnalyzer(string base_path, string ast_filename, string c2ast_path);
	void analysis(const json &ast);
	const json& result();
};
