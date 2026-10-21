/// Palan Semantic Analyzer
///
/// @file PlnSemanticAnalyzer.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <optional>
#include <functional>
#include <filesystem>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"
#include "PlnSaMessage.h"

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
	string  elemKind = "";  // "prim" | "struct"; also set for typeKind == "raw-ptr" (pointee kind);
	                        // "func" for a function pointer (see funcType)
	int     stride   = 0;   // bytes per element

	string  enumName = "";  // enum the primitive leaf is typed as, if any
	// raw-ptr/embed-ptr-arr: mutability of each pointer level between the
	// field's own pointer and the leaf (typeName), outermost first; empty = @T.
	vector<bool> innerPtrMutable;
	// A C type this version cannot represent (e.g. "function pointer"): the
	// field keeps its place in the layout but cannot be accessed.
	string  unsupported = "";
	json    funcType;  // elemKind == "func": the pointee function type
};

// Where an aggregate variable's outermost storage block lives: `$T v;` puts it
// in the function's stack frame.
enum class VarStorage { Heap, Frame };

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
	// A field (or one nested in an embedded struct) sits off its type's own
	// alignment, as only a C packed struct allows; passed by value as MEMORY class.
	bool   hasUnalignedField = false;
	const char* keyword() const { return isUnion ? "union" : "struct"; }
};

struct FieldChain {
	bool   isPointerBased;  // false = var+offset (inline chain), true = ptr-expr needed
	string varName;         // base variable name when !isPointerBased
	int    offset;          // accumulated byte offset
	json   ptrExpr;         // partial SA json when isPointerBased
	string structName;      // struct type currently being resolved
};

// A function a call can name: from a cinclude (isC) or a Palan definition.
struct Callee {
	const json* def;
	bool        isC;
};

// A call argument before the callee's parameter is known. A pending one gets
// its type only from the parameter (see takesExpectedType), so it is analyzed
// once the callee is chosen.
struct CallArg {
	json ast;
	bool pending;
	json sa;
};

// A diagnostic decided without exiting, so a check can also just test a fit.
struct SaDiag {
	PlnSaMessageCode code;
	string arg1 = "\x01";
	string arg2 = "\x01";
};

// The value of a constant expression under evaluation.
struct ConstVal {
	enum class Kind { Int, Uint, Flo } kind;
	int64_t  i = 0;
	uint64_t u = 0;
	double   f = 0;
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
	const json*       currentFuncDef_ = nullptr;  // currentFunc_'s types resolved, not normalized
	int               loopDepth_     = 0;        // nesting depth of while loops
	size_t            funcBodyScopeIdx_ = 0;     // 0 = top-level (no function)

	vector<map<string, json>> varScopes;
	vector<map<string, json>> cFuncScopes;
	vector<map<string, json>> cGlobalScopes;
	vector<map<string, json>> plnFuncScopes;
	// scope stack: alias("" = unqualified) → funcname → funcDefs (several
	// when unqualified imports of different modules export the same name)
	vector<map<string, map<string, vector<json>>>> importScopes;

	// Aggregate variables per scope (parallel to varScopes) and what runs when
	// each goes out of scope. A frame variable is listed even with nothing to
	// release, but only a heap variable owns its storage.
	struct ScopeVar { string name; json release; bool owned; };
	vector<vector<ScopeVar>> arrayScopeVars_;
	// Stack of while-body scope indices (for break/continue cleanup)
	vector<size_t> whileScopeStack_;
	// Counter for generating unique temporary variable names
	int tempVarCounter_ = 0;
	// Variables of the function being analyzed (the top level counts as one)
	// whose storage a pointer has been given to, and the count of
	// `pending-alias` statements waiting for that to be known.
	set<string> escapedVars_;
	int pendingAliases_ = 0;
	// Registered struct type definitions
	map<string, StructDef> structDefs_;
	set<string>            allocShapeNames_;  // dedup guard for struct alloc-shapes
	// Written type name -> the type it names in this scope, in program-wide
	// names: aliases, C typedefs, enums, and every Palan struct source may
	// write here. An unbound C struct is written by its program-wide name.
	map<string, json>      typeAliases_;
	// cinclude'd C typedefs (name -> var-type, its own references resolved), for
	// resolving a later header's "user" reference to one of them.
	map<string, json>      cTypedefs_;
	// Registered enum types: enumerator values, and the declaration's loc,
	// null for a cinclude'd C enum (whose first definition wins).
	struct EnumDef { map<string, int64_t> values; json loc; };
	map<string, EnumDef>   enumDefs_;
	// Defining module of each Palan struct and enum name. These names are
	// program-wide identities (build-mgr keys struct allocators by them), so a
	// name may come from one module only. A C type or an alias has no entry.
	map<string, string>    typeOrigins_;
	// Imported type names source may not write unqualified -> the module
	// alias to write them with.
	map<string, string>    hiddenTypeNames_;
	// A top-level struct/enum/alias/const declaration, resolved when first
	// needed, so neither source order nor which of two files importing each
	// other is analyzed first decides whether a type is known yet.
	struct TopDecl {
		const json* stmt;
		enum State { Pending, Resolving, Done } state = Pending;
	};
	map<string, TopDecl>   topDecls_;
	// A type a top-level import makes writable here, keyed by how it is
	// written (`T` or `V.T`).
	struct ImportedType {
		const json*          importStmt;
		PlnSemanticAnalyzer* module;
		const json*          decl;  // the exporting module's declaration
		bool                 adopted = false;
	};
	map<string, ImportedType> importedTypes_;
	// Struct names declared at the top level here or imported, resolved or
	// not: a pointer may name one before its layout is known.
	set<string>            declaredStructs_;
	// Top-level declarations the pre-scan resolved; step 2 skips them.
	set<const json*>       prescannedDecls_;
	// A block's declarations join topDecls_, typeAliases_ and constDecls_
	// under their written names while the block is analyzed. Shadowing is an
	// error, so leaving the block erases them without hiding an outer entry.
	vector<vector<string>> blockDeclNames_;
	// Written name -> program-wide name of each unexported top-level
	// struct/enum and each one of the enclosing blocks. Modules and blocks may
	// reuse a name, the allocators and sa.json may not.
	map<string, string>    localTypeNames_;
	int                    localTypeSeq_ = 0;
	// Type names (`T` or `V.T`) each enclosing block's imports made writable.
	vector<vector<string>> blockImportedTypes_;
	// Program-wide type resolution state, shared by every module's analyzer.
	struct TypeProgram;
	unique_ptr<TypeProgram> ownedProgram_;
	TypeProgram*            program_;
	json                    moduleAst_;  // an imported module's AST, which topDecls_ point into
	const json*             moduleAstRoot_ = nullptr;  // this module's AST, wherever it is held
	// Registered const declarations (name -> {"value": <SA'd literal expr>, "value-type": <type>})
	map<string, json>      constDecls_;
	// Library names collected from cinclude `link` clauses. A set: the same
	// library named by two cinclude statements is emitted once, and the
	// ordering makes the sa.json "libs" section deterministic.
	set<string>            linkLibs_;
	set<string>            linkObjs_;  // absolute paths, resolved by gen-ast

	void enterScope();
	void leaveScope();
	json collectFreeStmts(size_t from_idx, size_t to_idx);

	string locPrefix(const json& node) const;

	void        declareVar(const string& name, const json& type, const json* loc_node = nullptr);
	const json* findVar(const string& name) const;
	bool        isLocalVar(const string& name) const;
	bool        isOwnedVar(const string& name) const;
	bool        isFrameVar(const string& name) const;
	void        removeFromArrayScope(const string& name);
	void        addOwnedScopeVar(const string& name, json release);

	void        registerCFunc(const string& name, const json& def);
	const json* findCFunc(const string& name) const;

	void        registerCGlobal(const string& name, const json& def);
	const json* findCGlobal(const string& name) const;
	[[noreturn]] void undefinedNameError(const json& at, const json& ref, PlnSaMessageCode undefinedCode) const;

	void        registerPlnFunc(const string& name, const json& def, const json& locNode);
	const json* findPlnFunc(const string& name) const;
	vector<Callee> findCallees(const string& name) const;
	vector<Callee> findCalleesByAlias(const string& alias, const string& name) const;
	void   requireAlias(const json& locNode, const string& alias) const;
	Callee requireUniqueCallee(const json& locNode, const string& name, const vector<Callee>& cands) const;

	json sa_statements(const json& stmts);
	void beginModule(const json& ast);
	void prescanTypes(const json& stmts);
	void indexTopDecls(const json& stmts);
	json loadImportAst(const json& stmt, filesystem::path& impPath) const;
	// The imported module's type context, shared program-wide; its types are
	// resolved only as they are needed.
	PlnSemanticAnalyzer& importTypeContext(json impAst, const filesystem::path& impPath);
	// Resolves the type or const `name` (as written in this module) if a
	// top-level declaration or import provides it and it is not resolved yet.
	// Through a pointer, a struct only has to exist.
	void ensureTypeName(const string& name, bool viaPointer);
	// ensureTypeName for every type and const name `node` refers to.
	void ensureTypeRefs(const json& node, bool viaPointer = false);
	// ensureTypeName for a struct by its program-wide name.
	void ensureStructDef(const string& name);
	void resolveTopDecl(const string& name, TopDecl& d);
	void adoptImportedType(ImportedType& t, const string& alias);
	// Runs `f` once no declaration is being resolved: resolving a pointee
	// earlier could report a cycle a pointer breaks.
	void whenSettled(function<void()> f);
	void adoptExportedTypes(const json& stmt, const json& impAst, PlnSemanticAnalyzer& sub);
	void adoptExportedType(const json& stmt, const json& decl, PlnSemanticAnalyzer& sub);
	void adoptStruct(PlnSemanticAnalyzer& sub, const string& name, bool nameable, const string& alias,
	                 const json& locNode);
	void adoptEnum(const PlnSemanticAnalyzer& sub, const string& name, bool nameable, const string& alias,
	               const json& locNode);
	void adoptTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode);
	void setTypeNameable(const string& name, bool nameable, const string& alias);
	// Records `name` as defined by `origin`; false if it already was, exits if
	// another module or a cincluded header defined it.
	bool claimTypeName(const string& name, const string& origin, const json& locNode);
	void declareTypeName(const string& name, const json& stmt);
	void requireNotPalanType(const string& name, const json& locNode, bool isStruct) const;
	// Binds a type an import makes writable; exits if the name is bound to another type.
	void bindImportedTypeName(const json& locNode, const string& name, const json& type);
	// Exits if `written` names a Palan type this scope cannot write.
	void requireWritableTypeName(const json& locNode, const string& written) const;
	void sa_import(const json &stmt);
	void sa_cinclude(const json &stmt);
	void registerCIncludeTypes(const json& stmt); // cinclude structs/typedefs only
	json foldConstExpr(const json& expr);
	optional<ConstVal> evalConst(const json& expr);
	json sa_expression(const json &expr, const PlnType* expectedType = nullptr);
	// Also gives back a void call or a multi-value call, which has no value-type.
	json sa_expr_maybe_void(const json &expr, const PlnType* expectedType = nullptr);
	json sa_expr_arith(const json& expr, const PlnType* expectedType);
	json sa_expr_shift(const json& expr, const PlnType* expectedType);
	void checkIntLiteralRange(const json& lit);
	json sa_expr_call(const json& expr);
	json sa_func_ref(const json& expr);
	const json* findFuncPtrVar(const json& call) const;
	json funcPtrField(const json& memberCall);
	json sa_expr_call_ptr(const json& expr, json callee, const string& funcName);
	json sa_expr_member_call(const json& expr);
	json normalizeMethodCall(const json& expr);
	bool isMethodForm(const json& memberCall) const;
	const json* findCalleeByName(const string& name, bool& isCFunc) const;
	const json* findCallee(const json& call, bool& isCFunc) const;
	json operandValueType(const json& obj);
	json chainArg(const json& value, const json* paramVT, bool isCFunc);
	size_t chainRetCount(const json& value) const;
	json normalizeChainCall(const json& expr);
	json chainMultiCall(const json& chain, json& body);
	json sa_expr_chain_call(const json& expr);
	json tappleCallExpr(const json& stmt);
	// Analyze a call's argument list against the callee's parameter list.
	json saCallArgs(const json& locNode, const json& args, const json& funcParams,
	                bool isCFunc, const string& funcName);
	bool takesExpectedType(const json& expr);
	vector<CallArg> analyzeCallArgs(const json& args);
	optional<SaDiag> callArgMismatch(const json& argAst, const json& saArg, const json& param,
	                                 size_t argIdx, bool isCFunc, const string& funcName);
	json bindCallArgs(const json& locNode, vector<CallArg>& args, const json& funcParams,
	                  bool isCFunc, const string& funcName);
	[[noreturn]] void reportDiag(const json& locNode, const SaDiag& diag) const;
	optional<SaDiag> ptrPermissionMismatch(const json& from, const json& to);
	void checkPtrPermission(const json& locNode, const json& from, const json& to);
	// Shared narrowing rule for every binding site.
	json convertForBinding(const json& locNode, json value, const PlnType* toType, const json& toTypeJson);
	optional<SaDiag> callArgConvMismatch(const json& saArg, const json& paramVT);
	json convertCallArg(json saArg, const json& paramVT);
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
	json discardedValueStmt(const json& stmt, json body);
	json sa_var_decl(const json& stmt);           // returns array of statements
	// sa_var_decl for var-types already resolved (absent storage = heap).
	json sa_resolved_var_decl(const json& stmt, vector<VarStorage> storage = {});
	// all vars share one var-type; returns array of statements
	json sa_var_decl_group(const json& stmt, VarStorage storage);
	json sa_arr_var_decl(const json& stmt, VarStorage storage);       // returns array of statements
	json sa_arr_lit_var_decl(const json& stmt, VarStorage storage);   // single var; returns array of statements
	json sa_arr_copy_var_decl(const json& stmt, VarStorage storage);  // single var; returns array of statements
	json sa_struct_lit_var_decl(const json& stmt, VarStorage storage); // single var; returns array of statements
	json placeAggregateVar(const json& stmt, const string& name, const json& varType,
	                       json heapInit, json heapRelease, VarStorage storage,
	                       int64_t frameBytes, int frameAlign, bool zeroFill, json& post);
	json sa_struct_lit(const json& item, const StructDef& def);
	void emitStructLitAssigns(const json& objAst, const StructDef& def, const json& values, json& out);
	string arrLitDimSize(const json& stmt, const string& name, json& sizeExpr, size_t count);
	// Where an array literal's elements go: numeric leaf values, or structs
	// (structDef set) given as struct literals.
	struct ArrLitShape { bool is2d; json leaf; const StructDef* structDef; };
	ArrLitShape arrLitShape(const json& locNode, const string& name, const json& saType);
	json sa_arr_lit_values(const json& lit, const ArrLitShape& shape, const string& name);
	void checkArrLitCount(const json& locNode, const json& items, const string& name,
	                      bool is2d, uint64_t size, uint64_t rowSize);
	void emitArrLitAssigns(const json& arrAst, const ArrLitShape& shape, const json& values, json& out);
	json sa_embed_arr_var_decl(const json& stmt, VarStorage storage); // returns array of statements
	json sa_owned_struct_arr_var_decl(const json& stmt, VarStorage storage); // returns array of statements
	json sa_owned_struct_arr2d_var_decl(const json& stmt, VarStorage storage); // returns array of statements
	void recordArrStructShape(const string& structName);
	void recordArrArrShape(const string& leafName);
	void recordArrArrStructShape(const string& structName);
	void enterBlockDecls(const json& stmts);
	void declareLocalTypeName(const json& stmt);
	void leaveBlockDecls();
	string declaredTypeName(const string& written) const;
	bool isDeclNameTaken(const string& name) const;
	bool isWrittenNameTaken(const string& name) const;  // isDeclNameTaken but for topDecls_
	static void addDeclaredVarNames(const json& stmt, set<string>& names);
	json sa_struct_def(const json& stmt);         // consume struct-def, register in structDefs_
	void registerCStruct(const json& cincludeStmt, const json& s);  // consume c2ast "structs" entry, register in structDefs_
	json sa_struct_var_decl(const json& stmt, VarStorage storage); // returns array of statements
	json sa_type_alias(const json& stmt);         // consume type-alias, register in typeAliases_
	json sa_const_decl(const json& stmt);         // consume const-decl, register in constDecls_
	json sa_enum_def(const json& stmt);           // consume enum-def, register in enumDefs_/typeAliases_
	// `written` is how source names it, `name` its program-wide name.
	void registerEnum(const string& name, map<string, int64_t> values, const json& loc, const string& written);
	void registerCEnum(const json& cincludeStmt, const json& e);
	void resolveCTypeRefs(json& node) const;      // C enum/earlier-cinclude typedef references -> SA type
	json enumTypeNamed(const string& name) const; // enum value-type for a type name, or null
	string typeNameWritten(const json& obj) const;
	json aliasCast(const json& expr, const string& typeName);
	json sa_expr_cast(const json& expr);
	json resolveEnumerator(const json& expr);     // `Name.X` as a lit-int, or null for a field access
	int64_t typeByteSize(const json& locNode, json type);  // `sizeof(type)`, type resolved
	void recordAllocShape(const string& structName);
	// Scope-exit release of an owned struct variable (pntrType is pntr(struct)).
	json makeStructFreeStmt(const string& name, const json& pntrType);
	json makeStructFreeCall(json ptr, const string& structName);
	pair<string, string> structFreeFunc(const string& structName);
	json makeOwnedValueFreeStmt(json value);
	bool isStructType(const json& type) const;
	// True if the program-wide name `name` is a primitive or a registered struct.
	bool isKnownTypeName(const string& name) const;
	// isKnownTypeName plus "void", valid only as a pointer pointee, never a standalone value type.
	bool isKnownPointeeTypeName(const string& name) const;
	// Exits with E_UnknownType if a name at the leaf of `type` (through arr/pntr levels) is unknown,
	// or if a `$` element is not a struct or a `$[m]` row. Struct fields don't pass through here:
	// a `[n]$int32` field is an inline array, which a variable has no counterpart of.
	// `type` must be resolved (resolveWrittenType).
	void requireKnownTypeNames(const json& locNode, const json& type) const;
	// Exits unless each parameter/return of a function type is one a function
	// pointer can carry (isFuncSigType) and names known types.
	void requireFuncSigTypes(const json& locNode, const json& func) const;
	json  toStructPntrType(const json& type) const;
	json  unsizedArrToPntr(const json& locNode, const json& type);
	// A '@T'/'@!T' array element (a pointer slot) as SA represents it.
	json  ptrSlotElemType(const json& locNode, const json& type);
	void  normalizeUnsizedArrSig(json& funcDef);
	json  deepNormalizePrimToStruct(const json& type) const;
	// The type a written name is bound to in this scope, or null.
	const json* findTypeBinding(const string& written) const;
	// A type as written in source -> the same type in program-wide names, at
	// every level. Each written type passes through here exactly once.
	json  resolveWrittenType(const json& locNode, const json& type) const;
	// resolveWrittenType, then requireKnownTypeNames.
	json  resolveKnownType(const json& locNode, const json& type) const;
	// A function's parameter/return types resolved, before any normalization.
	json  resolveSigTypes(const json& f) const;
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
	const json* firstNonConstName(const json& expr) const;
	optional<SaDiag> elemShapeMismatch(const json& saValue, const json& dstType);
	void  checkElemShape(const json& locNode, const json& saValue, const json& dstType);
	optional<SaDiag> arrBorrowBindingMismatch(const json& srcAst, const json& saValue,
	                                          const json& dstType);
	void  checkArrBorrowBinding(const json& locNode, const json& srcAst, const json& saValue,
	                            const json& dstType);
	json  sa_call_borrow_arg(const json& arg);
	json  sa_lit_arg(const json& arg, const json* paramVT, const string& paramName,
	                 const string& funcName, json& argAst, json& body, json& release);
	json  bindCallTemp(json saArg);
	void  checkBorrowSource(const json& locNode, const json& saValue, const json& dstType);
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
	void  rejectNotImplSig(const json& f);
	json  makeCallNode(const json& expr, const Callee& callee, const string& writtenName);
	json sa_field_assign(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	FieldChain resolveObjectChain(const json& obj, bool forWrite);
	void requireOwnedBase(const json& saBase, const json& locNode);
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
	json sa_store_scope(const json& stmt);  // returns json::array()
	json sa_store_stmt(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	void hoistStores(json& value, json& body);
	json sa_store_steps(const json& stmt, json& body, bool readBack);
	json storeTargetType(const json& stmt);
	json pinLocation(json loc, json& body);
	json pinValue(json saValue, const json& astValue, json& body);
	bool valueFirst(json& s, json& pre, json& post, set<string>& roots);
	json snapshotCopy(const json& saSrc, const json& locNode, json& pre, json& post);
	json takeScopeRelease(const string& name);
	const json& scopeRelease(const string& name) const;
	void noteEscape(const json& saValue);
	bool storageRoots(const json& v, set<string>& roots, bool contents);
	void resolvePending(json& stmts);
	json sa_assign_stmt(const json& stmt);  // returns json::array()
	json sa_transfer_to_var(const json& stmt, const string& name, const json& varType, const PlnType* toType);
	// `src -> dst` for a dst that isCopiedByValue: a statement copying src's
	// contents into dst's storage. Diagnoses E_CopyShapeMismatch.
	json makeCopyStmt(const json& locNode, const json& dst, const json& src);
	json sa_arr_assign_stmt(const json& stmt, json* litAddr = nullptr);  // returns json::array()
	// `[...] -> target` / `{...} -> target` for a struct or array target; returns a block.
	// With litAddr, returns the statements without a scope of their own and
	// sets litAddr to the target's address, held in a temp, to read it back.
	json sa_aggregate_lit_assign(const json& stmt, const json& targetAst, const json& targetType,
	                             json* litAddr = nullptr);
	void appendTransferSourceReset(json& stmts, const json& stmt, const json& saValue);
	// `value ->> target`: releases what target holds, stores, and resets the source.
	json transferStmts(const json& stmt, json targetRelease, json store, const json& value);
	json sa_return_stmt(const json& stmt);
	json bindReturnValueToTemp(const json& stmt, json& ret);
	json bindReturnStructLit(json& stmt);
	json copyReturnStruct(const json& stmt, json& ret);
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
	~PlnSemanticAnalyzer();
	void analysis(const json &ast);
	const json& result();
};
