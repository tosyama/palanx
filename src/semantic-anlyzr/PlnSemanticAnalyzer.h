/// Palan Semantic Analyzer
///
/// @file PlnSemanticAnalyzer.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#include <string>
#include <map>
#include <set>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"

using namespace std;

using json = nlohmann::json;

struct FieldLayout {
	string name;
	string typeKind;   // "prim" | "embed" | "struct-ptr" | "raw-ptr"
	string typeName;   // prim type name, embed/struct-ptr struct name, raw-ptr base type name
	bool   isMutable;  // raw-ptr only: @T=false, @!T=true
	int    offset;     // byte offset from struct start (C ABI aligned)
	int    size;       // prim: type size, embed: sub-struct totalSize, struct-ptr/raw-ptr: 8
};

struct StructDef {
	string name;
	vector<FieldLayout> fields;
	int  totalSize;
	int  maxAlign;
	bool hasOwnedStructFields = false;
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
	json sa;
	PlnTypeRegistry registry_;
	map<string, string> strLiteralLabels;  // value -> label
	const json*       currentFunc_    = nullptr;  // null = _start level
	int               loopDepth_     = 0;        // nesting depth of while loops
	size_t            funcBodyScopeIdx_ = 0;     // 0 = top-level (no function)

	vector<map<string, json>> varScopes;
	vector<map<string, json>> cFuncScopes;
	vector<map<string, json>> plnFuncScopes;
	// scope stack: alias("" = unqualified) → funcname → funcDef (empty json{} = ambiguous sentinel)
	vector<map<string, map<string, json>>> importScopes;

	// Array variable tracking per scope (parallel to varScopes)
	vector<vector<pair<string,json>>> arrayScopeVars_;
	// Stack of while-body scope indices (for break/continue cleanup)
	vector<size_t> whileScopeStack_;
	// Counter for generating unique temporary variable names
	int tempVarCounter_ = 0;
	// True when analyzing a __pln_alloc_* function body (suppress recursive alloc)
	bool inAllocFunc_ = false;
	// Registered struct type definitions
	map<string, StructDef> structDefs_;
	set<string>            allocShapeNames_;  // dedup guard for struct alloc-shapes

	void enterScope();
	void leaveScope();
	json collectFreeStmts(size_t from_idx, size_t to_idx);

	string locPrefix(const json& node) const;

	void        declareVar(const string& name, const json& type, const json* loc_node = nullptr);
	const json* findVar(const string& name) const;
	bool        isInArrayScope(const string& name) const;
	void        removeFromArrayScope(const string& name);

	void        registerCFunc(const string& name, const json& def);
	const json* findCFunc(const string& name) const;

	void        registerPlnFunc(const string& name, const json& def, const json* loc_node = nullptr);
	const json* findPlnFunc(const string& name) const;
	const json* findImportFunc(const string& fname) const;
	const json* findImportFuncByAlias(const string& alias, const string& fname) const;

	json sa_statements(const json& stmts);
	void sa_import(const json &stmt);
	void sa_cinclude(const json &stmt);
	json sa_expression(const json &expr, const PlnType* expectedType = nullptr);
	json sa_expr_arith(const json& expr, const PlnType* expectedType);
	json sa_expr_call(const json& expr);
	json sa_expr_member_call(const json& expr);
	json sa_expr_arr_index(const json& expr);
	json sa_expression_stmt(const json& stmt);
	json sa_var_decl(const json& stmt);           // returns array of statements
	json sa_arr_var_decl(const json& stmt);       // returns array of statements
	json sa_embed_arr_var_decl(const json& stmt); // returns array of statements
	json sa_struct_def(const json& stmt);         // consume struct-def, register in structDefs_
	json sa_struct_var_decl(const json& stmt);    // returns array of statements
	void recordAllocShape(const string& structName);
	bool isStructType(const json& type) const;
	json  toStructPntrType(const json& type) const;
	bool  isNamedReturnVar(const string& varName) const;
	void  normalizeStructSig(json& funcDef);
	json sa_field_assign(const json& stmt);
	FieldChain resolveObjectChain(const json& obj);
	FieldChain resolveStoreLocChain(const json& loc);
	void validateEmbeddedParams(const json& funcDef);
	void sa_functions(const json& funcs);
	void sa_function(const json& funcDef);
	json sa_assign_stmt(const json& stmt);
	json sa_arr_assign_stmt(const json& stmt);  // returns json::array()
	json sa_return_stmt(const json& stmt);
	json sa_tapple_decl(const json& stmt);
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
