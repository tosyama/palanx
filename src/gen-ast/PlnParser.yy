/// Palan Parser
///
/// @file PlnParser.yy
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

%glr-parser
%language "c++"
%require "3.8"
%skeleton "glr2.cc"

%defines
%define api.parser.class	{PlnParser}
%parse-param	{PlnLexer& lexer}	{json& ast}	{MacroTable& macros}	{string& cMacroState}
%lex-param	{PlnLexer& lexer}

%code requires
{
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <stdexcept>

#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnGenAstMacroFold.h"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::runtime_error;
using json = nlohmann::json;
namespace fs = std::filesystem;

class PlnLexer;

// glr2.cc passes $N to actions as const references (GLR stacks may share a
// value), so `$$ = move($1); $$.push_back(x)` copies the whole list on every
// append. An immutable list sharing its prefix makes each append O(1).
template <class T> struct PList {
	struct Node {
		T item;
		// mutable only so the destructor can unlink the chain iteratively;
		// the default recursive release overflows the stack on long lists.
		mutable std::shared_ptr<const Node> prev;
		~Node() {
			auto p = std::move(prev);
			while (p && p.use_count() == 1)
				p = std::move(p->prev);
		}
	};
	std::shared_ptr<const Node> last;
	size_t size = 0;

	PList push(T item) const { return { std::make_shared<const Node>(Node{ std::move(item), last }), size + 1 }; } // LCOV_EXCL_EXCEPTION_BR_LINE

	vector<T> toVector() const {
		vector<T> v(size);
		const Node* n = last.get();
		for (size_t i = size; i > 0; --i, n = n->prev.get()) v[i-1] = n->item;
		return v;
	} // LCOV_EXCL_LINE

	template <class Pred> const T* findLast(Pred pred) const {
		for (const Node* n = last.get(); n; n = n->prev.get())
			if (pred(n->item)) return &n->item;
		return nullptr;
	}
};

// A statement holds its nested blocks by handle and is turned into json once
// at the end; building json at each reduction would copy a block's contents
// once per enclosing level.
struct LazyNode;
using NodeRef = std::shared_ptr<const LazyNode>;
struct LazyNode {
	json self;
	vector<std::pair<string, NodeRef>> nodes;
	vector<std::pair<string, PList<NodeRef>>> lists;

	json toJson() const {
		json j = self;
		for (auto& [key, node] : nodes) j[key] = node->toJson();
		for (auto& [key, list] : lists) j[key] = toJsonArray(list);
		return j;
	} // LCOV_EXCL_LINE
	static json toJsonArray(const PList<NodeRef>& list) {
		json arr = json::array();
		for (auto& n : list.toVector()) arr.push_back(n->toJson());
		return arr;
	} // LCOV_EXCL_LINE
};

inline NodeRef leafNode(json j) { return std::make_shared<const LazyNode>(LazyNode{ std::move(j), {}, {} }); } // LCOV_EXCL_EXCEPTION_BR_LINE
inline NodeRef blockStmtNode(const PList<NodeRef>& body) {
	return std::make_shared<const LazyNode>(LazyNode{ {{"stmt-type", "block"}}, {}, {{"body", body}} }); // LCOV_EXCL_EXCEPTION_BR_LINE
} // LCOV_EXCL_EXCEPTION_BR_LINE

struct BodyList {
	PList<NodeRef> functions, body;
	NodeRef toNode() const {
		return std::make_shared<const LazyNode>(LazyNode{ json::object(), {}, {{"functions", functions}, {"body", body}} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	} // LCOV_EXCL_EXCEPTION_BR_LINE
};
}

%code
{
	#include <set>
	#include "PlnLexer.h"
	#include "PlnGenAstMessage.h"
	#include "PlnGenAstInternal.h"
	#include "PlnGenAstC2Ast.h"

	static std::set<std::string> typeNames = {
		"int8",  "int16",  "int32",  "int64",
		"uint8", "uint16", "uint32", "uint64",
		"flo32", "flo64", "bool"
	};

	int yylex(
		palan::PlnParser::value_type* yylval,
		palan::PlnParser::location_type* location,
		PlnLexer& lexer)
	{
		return lexer.yylex(*yylval, *location);
	}

// glr2.cc grows the GLR stack by memcpy-ing items and then destroying the
// originals, which frees each semantic value twice. Reserve the maximum depth
// up front and never grow; a deeper parse reports "memory exhausted".
#define YYMAXDEPTH 10000
#define YYINITDEPTH YYMAXDEPTH
#define YYSTACKEXPANDABLE 0

#define LOC(J, L)       J["loc"] = { (int)L.begin.line, (int)L.begin.column, (int)L.end.line, (int)L.end.column }
#define LOC_BE(J, B, E) J["loc"] = { (int)B.begin.line, (int)B.begin.column, (int)E.end.line, (int)E.end.column }

// An empty rule (e.g. arguments) is located at the end of the previous token,
// so a rule's start is taken from its first non-empty component instead.
#define YYLLOC_DEFAULT(Current, Rhs, N)                                   \
	do {                                                                  \
		if (N) {                                                          \
			int yyk = 1;                                                  \
			while (yyk < (N) && YYRHSLOC(Rhs, yyk).begin.line == YYRHSLOC(Rhs, yyk).end.line \
			       && YYRHSLOC(Rhs, yyk).begin.column == YYRHSLOC(Rhs, yyk).end.column) \
				yyk++;                                                    \
			(Current).begin = YYRHSLOC(Rhs, yyk).begin;                   \
			(Current).end   = YYRHSLOC(Rhs, N).end;                       \
		} else {                                                          \
			(Current).begin = (Current).end = YYRHSLOC(Rhs, 0).end;       \
		}                                                                 \
	} while (false)

	static void requireInKeyword(const string& file, const string& word,
	                             const palan::PlnParser::location_type& loc)
	{
		if (word != "in")
			throw runtime_error(PlnGenAstMessage::locatedError(file, loc.begin.line, loc.begin.column,
				PlnGenAstMessage::getMessage(E_ExpectedInKeyword, word)));
	}

	// A parameter or return the compiler does not implement stays in the
	// signature as is, so SA can reject the function where it is defined.
	static json funcDecl(json& ast, bool exported, json fn, const vector<json>& params, const json& ret,
	                     const palan::PlnParser::location_type& loc)
	{
		// Frame storage is a property of a local declaration, and a name after
		// only unimplemented entries has no type to inherit; such a parameter or
		// named return stays an unimplemented entry for SA to report.
		auto rejectUnimplemented = [&](vector<json> vars) {
			for (auto& v : vars)
				if (v.value("inherit-type", false)
						|| (v.contains("var-type") && v["var-type"].value("type-kind", "") == "embed")) {
					v = {{"not-impl", true}};
					LOC(v, loc);
				}
			return vars;
		};
		fn["parameters"] = rejectUnimplemented(params);
		if (ret.contains("rets"))
			fn["rets"] = rejectUnimplemented(ret["rets"].get<vector<json>>());
		else if (ret.contains("ret-type"))
			fn["ret-type"] = ret["ret-type"];
		LOC(fn, loc); // LCOV_EXCL_EXCEPTION_BR_LINE
		if (exported) {
			fn["export"] = true;
			ast["export"].push_back(fn);
		}
		return fn;
	} // LCOV_EXCL_EXCEPTION_BR_LINE
}

%locations
%define api.namespace	{palan}
%define api.value.type	variant
%define parse.error	verbose

%token <string>	INT	"integer"
%token <string>	UINT	"unsigned integer"
%token <string>	FLO	"float"
%token <string>	STRING	"string"
%token <string>	ID	"identifier"
%token <string>	PATH	"path"
%token <string>	INCLUDE_FILE	"include file"
%token KW_EXPORT	"export"
%token KW_IMPORT	"import"
%token KW_CINCLUDE	"cinclude"
%token KW_FROM	"from"
%token KW_AS	"as"
%token KW_FUNC	"func"
%token KW_TYPE	"type"
%token KW_ENUM	"enum"
%token KW_CONSTRUCT	"construct"
%token KW_INTERFACE	"interface"
%token KW_CONST	"const"
%token KW_VOID	"void"
%token KW_RETURN	"return"
%token KW_FOR	"for"
%token KW_WHILE	"while"
%token KW_IF	"if"
%token KW_ELSE	"else"
%token KW_BREAK	"break"
%token KW_CONTINUE	"continue"
%token KW_SYSCALL	"syscall"
%token KW_SIZEOF	"sizeof"
%token KW_TRUE	"true"
%token KW_FALSE	"false"
%token OPE_LE	"<="
%token OPE_GE	">="
%token DBL_GRTR	">>"
%token OPE_SHL	"<<<"
%token OPE_SHR	">>>"
%token ARROW	"->"
%token DBL_ARROW	"->>"
%token AT_EXCL	"@!"
%token DBL_PLUS	"++"
%token OPE_EQ	"=="
%token OPE_NE	"!="
%token OPE_AND	"&&"
%token OPE_OR	"||"

%type <PList<NodeRef>>	statements block
%type <NodeRef>	block_stmt
%type <json>	expr_stmt
%type <PList<NodeRef>>	stmt_list_e stmt_list_b
%type <BodyList>	body_list_e body_list_b
%type <json>	import cinclude import_path
%type <PList<string>>	import_ids link_libs
%type <vector<string>>	link_clause
%type <string>	import_as
%type <json>	expression term store_loc
%type <PList<json>>	arguments
%type <json>	array_desc array_row dict_desc
%type <PList<json>>	array_rows array_items dict_items
%type <json>	type_expr fptr_ret
%type <PList<json>>	fptr_params fptr_param_list
%type <json>	var_declaration inherit_var_decl
%type <PList<json>>	var_declarations
%type <NodeRef>	func_def func_item syscall_decl
%type <json>	return_def syscall_ret ret_type
%type <json>	return
%type <vector<json>>	paramaters
%type <PList<json>>	expressions
%type <NodeRef>	block_obj standalone_block block_body_items
%type <json>	tapple_decl tapple_inner
%type <PList<json>>	tapple_decl_inner
%type <NodeRef>	if_stmt else_stmt while_loop for_loop
%type <json>	for_elem
%type <json>		type_decl type_member const_decl
%type <PList<json>>	type_members enumerators
%type <json>	enumerator

// A function pointer type's return is taken greedily: `e as @func(T) -> X`
// casts to a type returning X rather than chaining the cast into X.
%precedence FPTR_NORET
%left OPE_OR
%left OPE_AND
%left OPE_EQ OPE_NE
%left '<' '>' OPE_LE OPE_GE
%left ARROW DBL_ARROW
%left OPE_SHL OPE_SHR
%left '+' '-'
%left '*' '/' '%' '&' '|' '^'
%left KW_AS
%right UNARY_MINUS '!' '~'
%left '.'

%start module

%%
module: statements
	{
		if (!ast["ast"].contains("functions"))
			ast["ast"]["functions"] = json::array();
		ast["ast"]["statements"] = LazyNode::toJsonArray($1);
	}
	;

statements: /* empty */
	{ }
	| stmt_list_e
	{ $$ = $1; }
	| stmt_list_b
	{ $$ = $1; }
	| stmt_list_e ';'
	{ $$ = $1; }
	;

block_stmt: standalone_block
	{ $$ = $1; }
	| construct_def
	{ json j = {{"stmt-type", "not-impl"}}; LOC(j, @$); $$ = leafNode(move(j)); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| for_loop
	{ $$ = $1; }
	| while_loop
	{ $$ = $1; }
	| if_stmt
	{ $$ = $1; }
	;

expr_stmt: import
	{
		$$ = move($1);
		$$["stmt-type"] = "import";
		LOC($$, @$);
	}
	| cinclude
	{
		$$ = move($1);
		$$["stmt-type"] = "cinclude";

		json c_ast = execute_c2ast($$["path-type"], $$["path"],
		                          fs::path(lexer.inputFile).parent_path().string(), cMacroState);
		if (c_ast.is_object() && c_ast.contains("macro-state"))
			cMacroState = c_ast["macro-state"].get<string>();
		if (c_ast.is_object() && c_ast.contains("ast")) {
			if (c_ast["ast"].contains("functions")) {
				$$["functions"] = move(c_ast["ast"]["functions"]);
			}
			if (c_ast["ast"].contains("enums")) {
				$$["enums"] = move(c_ast["ast"]["enums"]);
			}
			if (c_ast["ast"].contains("structs")) {
				$$["structs"] = move(c_ast["ast"]["structs"]);
			}
			if (c_ast["ast"].contains("globals")) {
				$$["globals"] = move(c_ast["ast"]["globals"]);
			}
			if (c_ast["ast"].contains("typedefs")) {
				$$["typedefs"] = move(c_ast["ast"]["typedefs"]);
			}
		}
		LOC($$, @$);
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("constants")) {
			registerMacroConstants(macros, c_ast["ast"]["constants"], $$["loc"]);
		}
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("unexported-macros")) {
			registerUnexportedMacros(macros, c_ast["ast"]["unexported-macros"], $$["loc"]);
		}
		if (c_ast.is_object() && c_ast.contains("ast") && c_ast["ast"].contains("undefined-macros")) {
			registerUndefinedMacros(macros, c_ast["ast"]["undefined-macros"], $$["loc"]);
		}
	}
	| var_declarations
	{
		vector<json> vars = $1.toVector();
		// Detect a tapple-decl emitted by var_declaration
		if (vars.size() == 1 && vars[0].value("stmt-type", "") == "tapple-decl") { // LCOV_EXCL_EXCEPTION_BR_LINE
			$$ = move(vars[0]);
			LOC($$, @$);
		} else {
			bool all_ok = true;
			for (auto& v : vars) {
				if (v.count("not-impl") || v.value("stmt-type","") == "tapple-decl"
					|| v.value("inherit-type", false)) { all_ok = false; break; }
			}
			if (all_ok) {
				$$ = {{"stmt-type", "var-decl"}, {"vars", move(vars)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
				LOC($$, @$);
			} else {
				$$ = {{"stmt-type", "not-impl"}};
				for (auto& v : vars)
					if (v.contains("untyped-var")) { $$["untyped-var"] = v["untyped-var"]; break; }
				LOC($$, @$);
			}
		}
	}
	| const_decl
	{ $$ = {{"stmt-type", "const-decl"}, {"name", $1["name"]}, {"value", move($1["value"])}}; LOC($$, @$); }
	| type_decl
	{
		if ($1.contains("alias-of")) {
			$$ = {{"stmt-type", "type-alias"}, {"name", $1["name"]}, {"type", move($1["alias-of"])}};
			LOC($$, @$);
		} else if ($1.contains("enumerators")) {
			$$ = {{"stmt-type", "enum-def"}, {"name", $1["name"]}, {"enumerators", move($1["enumerators"])}};
			LOC($$, @$);
		} else if ($1.contains("name")) {
			$$ = {{"stmt-type", "struct-def"}, {"name", $1["name"]}, {"fields", move($1["fields"])}};
			LOC($$, @$);
		} else {
			$$ = {{"stmt-type", "not-impl"}};
			LOC($$, @$);
		}
		if ($1.contains("export")) $$["export"] = true;
	}
	| interface_decl
	{ $$ = {{"stmt-type", "not-impl"}}; LOC($$, @$); }
	| expression
	{
		if (isStoreStmt($1)) {
			$$ = move($1);
		} else {
			$$ = {{"stmt-type", "expr"}, {"body", move($1)}};
			LOC($$, @$);
		}
	}
	| return
	{
		$$ = {{"stmt-type", "return"}};
		if ($1.contains("values"))
			$$["values"] = move($1["values"]);
		LOC($$, @$);
	}
	| term DBL_PLUS
	{ $$ = {{"stmt-type", "not-impl"}}; LOC($$, @$); }
	| KW_BREAK
	{ $$ = {{"stmt-type", "break"}}; LOC($$, @$); }
	| KW_CONTINUE
	{ $$ = {{"stmt-type", "continue"}}; LOC($$, @$); }
	;

stmt_list_e: expr_stmt
	{ $$ = PList<NodeRef>().push(leafNode(move($1))); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| stmt_list_b expr_stmt
	{ $$ = $1.push(leafNode(move($2))); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| stmt_list_b ';' expr_stmt
	{ $$ = $1.push(leafNode(move($3))); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| stmt_list_e ';' expr_stmt
	{ $$ = $1.push(leafNode(move($3))); } // LCOV_EXCL_EXCEPTION_BR_LINE
	;

stmt_list_b: block_stmt
	{ $$ = PList<NodeRef>().push($1); }
	| func_item
	{ ast["ast"]["functions"].push_back($1->toJson()); }
	| stmt_list_b block_stmt
	{ $$ = $1.push($2); }
	| stmt_list_b func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($2->toJson());
	}
	| stmt_list_b ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_b ';' func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($3->toJson());
	}
	| stmt_list_e ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_e ';' func_item
	{
		$$ = $1;
		ast["ast"]["functions"].push_back($3->toJson());
	}
	;

import: KW_IMPORT import_path import_as
	{
		ast["import"].emplace_back($2);
		$$ = move($2);
		if ($3.size()) {
			$$["alias"] = $3;
		}
	}
	| KW_IMPORT import_ids KW_FROM import_path import_as
	{
		ast["import"].emplace_back($4);
		$$ = move($4);
		$$["targets"] = $2.toVector();
		if ($5.size()) { $$["alias"] = $5; }
	}
	; 

import_ids: ID
	{ $$ = PList<string>().push($1); }
	| import_ids ',' ID
	{ $$ = $1.push($3); }
	;

import_path: PATH
	{
		json pathinf = {
			{"path-type", "src"},
			{"path", $1}
		};
		$$ = move(pathinf);
	}
	| INCLUDE_FILE
	{
		json pathinf = {
			{"path-type", "inc"},
			{"path", $1}
		};
		$$ = move(pathinf);
	}
	;

import_as: /* empty */
	{ $$ = ""; }
	| KW_AS ID
	{ $$ = move($2); }
	;

cinclude: KW_CINCLUDE import_path import_as link_clause
	{
		$$ = move($2);
		if ($3.size()) {
			$$["alias"] = $3;
		}
		json libs = json::array(), objs = json::array();
		for (auto& name : $4) {
			if (isLinkObjName(name))
				objs.push_back(resolveLinkObjPath(name, lexer.inputFile));
			else
				libs.push_back(name);
		}
		if (libs.size()) $$["libs"] = move(libs);
		if (objs.size()) $$["objs"] = move(objs);
	}
	;

link_clause: /* empty */
	{ }
	| ID link_libs
	{
		if ($1 != "link") {
			throw runtime_error(
				PlnGenAstMessage::getMessage(E_ExpectedLinkKeyword, $1));
		}
		$$ = $2.toVector();
	}
	;

link_libs: STRING
	{ $$ = PList<string>().push($1); }
	| link_libs ',' STRING
	{ $$ = $1.push($3); }
	;

block: '{' statements '}'
	{ $$ = $2; }
	;

body_list_e: expr_stmt
	{
		$$.body = $$.body.push(leafNode(move($1))); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| body_list_b expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode(move($2))); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| body_list_b ';' expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode(move($3))); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| body_list_e ';' expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode(move($3))); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

body_list_b: block_stmt
	{
		$$.body = $$.body.push($1);
	}
	| func_item
	{ $$.functions = $$.functions.push($1); }
	| body_list_b block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($2);
	}
	| body_list_b func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($2);
	}
	| body_list_b ';' block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($3);
	}
	| body_list_b ';' func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($3);
	}
	| body_list_e ';' block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($3);
	}
	| body_list_e ';' func_item
	{
		$$ = $1;
		$$.functions = $$.functions.push($3);
	}
	;

block_body_items: /* empty */
	{ $$ = BodyList().toNode(); }
	| body_list_e
	{ $$ = $1.toNode(); }
	| body_list_b
	{ $$ = $1.toNode(); }
	| body_list_e ';'
	{ $$ = $1.toNode(); }
	;

block_obj: '{' block_body_items '}'
	{ $$ = $2; }
	;

standalone_block: block_obj
	{
		LazyNode blk = *$1;
		blk.self["stmt-type"] = "block";
		LOC(blk.self, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(std::move(blk));
	}
	;

var_declarations: var_declaration
	{ $$ = PList<json>().push($1); }
	| var_declarations ',' var_declaration   %dprec 1
	{ $$ = $1.push($3); }
	| var_declarations ',' inherit_var_decl  %dprec 2
	{
		json next = $3;
		if (auto typed = $1.findLast([](const json& v) { return v.contains("var-type"); })) {
			next["var-type"] = (*typed)["var-type"];
			next.erase("inherit-type");
		}
		$$ = $1.push(move(next));
	}
	;

inherit_var_decl: ID
	{ $$ = {{"name", $1}, {"inherit-type", true}}; }
	| ID '=' expression
	{ $$ = {{"name", $1}, {"inherit-type", true}, {"init", $3}}; }
	;

var_declaration: type_expr ID
	{
		if (isDeclarableVarType($1) || isFrameVarType($1)) {
			$$ = {{"name", $2}, {"var-type", move($1)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		} else {
			$$ = {{"not-impl", true}};
			LOC($$, @$);
		}
	}
	| type_expr DBL_GRTR ID
	{ $$ = {{"not-impl", true}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| type_expr ID '=' expression
	{
		if (isDeclarableVarType($1) || isFrameVarType($1)) {
			$$ = {{"name", $2}, {"var-type", move($1)}, {"init", move($4)}};
		} else {
			$$ = {{"not-impl", true}};
			LOC($$, @$);
		}
	}
	| ID '=' expression
	{ $$ = {{"not-impl", true}, {"untyped-var", $1}}; LOC($$, @$); }
	| tapple_decl '=' expression
	{
		string et = $3.value("expr-type","");
		if ($1.is_array() && (et == "call" || et == "member-call" || et == "chain-call"))
			$$ = {{"stmt-type", "tapple-decl"}, {"vars", $1}, {"value", $3}};
		else
			$$ = {{"not-impl", true}};
		LOC($$, @$);
	}
	;

tapple_decl: '(' tapple_decl_inner ')'
	{ $$ = $2.toVector(); }
	;

tapple_decl_inner: type_expr ID
	{ $$ = PList<json>().push({{"var-name", $2}, {"var-type", $1}}); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_VOID
	{ }
	| tapple_decl_inner ',' type_expr ID
	{ $$ = $1.push({{"var-name", $4}, {"var-type", $3}}); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| tapple_decl_inner ',' KW_VOID
	{ $$ = $1; }
	| tapple_decl_inner ',' ID   %dprec 1
	{
	  if (auto typed = $1.findLast([](const json& v) { return v.contains("var-type"); }))
	      $$ = $1.push({{"var-name", $3}, {"var-type", (*typed)["var-type"]}}); // LCOV_EXCL_EXCEPTION_BR_LINE
	  else
	      $$ = $1;
	}
	;

const_decl: KW_CONST ID '=' expression
	{ $$ = {{"name", $2}, {"value", move($4)}}; LOC($$, @$); }
	;

type_decl: KW_TYPE ID implememts '{' type_members '}'
	{ $$ = {{"name", $2}, {"fields", $5.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_EXPORT KW_TYPE ID implememts '{' type_members '}'
	{ $$ = {{"name", $3}, {"fields", $6.toVector()}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_TYPE ID KW_ENUM '{' enumerators opt_comma '}'
	{ $$ = {{"name", $2}, {"enumerators", $5.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_EXPORT KW_TYPE ID KW_ENUM '{' enumerators opt_comma '}'
	{ $$ = {{"name", $3}, {"enumerators", $6.toVector()}, {"export", true}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $2}, {"alias-of", move($4)}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| KW_EXPORT KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $3}, {"alias-of", move($5)}, {"export", true}}; LOC($$, @$); }
	| KW_TYPE ID
	{ $$ = json{}; }
	| KW_EXPORT KW_TYPE ID
	{ $$ = json{}; }
	;

enumerators: enumerator
	{ $$ = PList<json>().push($1); }
	| enumerators ',' enumerator
	{ $$ = $1.push($3); }
	;

enumerator: ID
	{ $$ = {{"name", move($1)}}; LOC($$, @$); }
	| ID '=' expression
	{ $$ = {{"name", move($1)}, {"value", move($3)}}; LOC($$, @$); }
	;

opt_comma: /* empty */
	| ','
	;

implememts: /* empty */
	| ':' implements_types
	;

implements_types: implements_type
	| implements_types ',' implements_type
	;

implements_type: type_expr
	| type_expr KW_AS ID
	;

type_members: type_member
	{
		if (!$1.value("not-impl", false)) $$ = PList<json>().push($1);
	}
	| type_members type_member
	{
		$$ = $2.value("not-impl", false) ? $1 : $1.push($2); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

type_member: type_expr ID ';'
	{ $$ = {{"name", move($2)}, {"var-type", move($1)}}; }
	| KW_FUNC long_func_name '(' paramaters ')' return_def block
	{ $$ = {{"not-impl", true}}; }
	;

long_func_name: ID
	| long_func_name '.' ID
	;

interface_decl: KW_INTERFACE ID '{' interface_methods '}'
	| KW_EXPORT KW_INTERFACE ID '{' interface_methods '}'
	;

interface_methods: /* empty */ 
	| interface_methods KW_FUNC ID '(' paramaters ')' return_def ';'
	| interface_methods KW_FUNC ID '(' paramaters ')' return_def block
	;

expression: term
	{ $$ = move($1); }
	| array_desc
	{ $$ = move($1); }
	| dict_desc
	{ $$ = move($1); }
	| expression '+' expression
	{ $$ = {{"expr-type", "add"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '-' expression
	{ $$ = {{"expr-type", "sub"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| '-' expression %prec UNARY_MINUS
	{ $$ = negateExpr(move($2)); LOC($$, @$); }
	| expression '*' expression
	{ $$ = {{"expr-type", "mul"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '/' expression
	{ $$ = {{"expr-type", "div"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '%' expression
	{ $$ = {{"expr-type", "mod"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '&' expression
	{ $$ = {{"expr-type", "bitand"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '|' expression
	{ $$ = {{"expr-type", "bitor"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '^' expression
	{ $$ = {{"expr-type", "bitxor"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_SHL expression
	{ $$ = {{"expr-type", "shl"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_SHR expression
	{ $$ = {{"expr-type", "shr"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression KW_AS type_expr
	{ $$ = {{"expr-type", "cast"}, {"target-type", move($3)}, {"src", move($1)}}; LOC($$, @$); }
	| '~' expression %prec UNARY_MINUS
	{ $$ = {{"expr-type", "bitnot"}, {"operand", $2}}; LOC($$, @$); }
	| expression OPE_LE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "<="}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_GE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", ">="}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '<' expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "<"},  {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression '>' expression
	{ $$ = {{"expr-type", "cmp"}, {"op", ">"},  {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_EQ expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "=="}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_NE expression
	{ $$ = {{"expr-type", "cmp"}, {"op", "!="}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_AND expression
	{ $$ = {{"expr-type", "logical-and"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| expression OPE_OR expression
	{ $$ = {{"expr-type", "logical-or"}, {"left", $1}, {"right", $3}}; LOC($$, @$); }
	| '!' expression
	{ $$ = {{"expr-type", "logical-not"}, {"operand", $2}}; LOC($$, @$); }
	| '@' store_loc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", false}}; LOC($$, @$); }
	| AT_EXCL store_loc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", true}}; LOC($$, @$); }
	| '@' array_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", false}}; LOC($$, @$); }
	| AT_EXCL array_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", true}}; LOC($$, @$); }
	| '@' dict_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", false}}; LOC($$, @$); }
	| AT_EXCL dict_desc %prec UNARY_MINUS
	{ $$ = {{"expr-type", "addr-of"}, {"object", move($2)}, {"mutable", true}}; LOC($$, @$); }
	| expression ARROW store_loc
	{
		string kind = $3.value("kind", "");
		if (kind == "tapple") {
			string et = $1.value("expr-type", "");
			if (et == "call" || et == "member-call" || et == "chain-call") {
				$$ = {{"stmt-type", "tapple-assign"}, {"targets", move($3["targets"])}, {"value", move($1)}};
				LOC($$, @$);
			} else {
				$$ = {{"expr-type", "not-impl"}};
				LOC($$, @$);
			}
		} else if (kind == "call" && !chainsThroughTapple($1)) {
			$$ = {{"expr-type", "chain-call"}, {"value", move($1)}, {"call", move($3["call"])}};
			LOC($$, @$);
		} else if (kind == "call") {
			$$ = {{"expr-type", "not-impl"}};
			LOC($$, @$);
		} else {
			$$ = storeStmt(move($3), move($1), false);
			LOC($$, @$);
		}
	}
	| expression DBL_ARROW store_loc
	{
		string kind = $3.value("kind", "");
		if (kind == "var" || kind == "arr-index" || kind == "field")
			$$ = storeStmt(move($3), move($1), true);
		else
			$$ = {{"expr-type", "not-impl"}};
		LOC($$, @$);
	}
	| noname_func
	{ $$ = {{"expr-type", "not-impl"}}; LOC($$, @$); }
	;

term: INT
	{ $$ = {{"expr-type", "lit-int"}, {"value", move($1)}}; LOC($$, @$); }
	| UINT
	{ $$ = {{"expr-type", "lit-uint"}, {"value", move($1)}}; LOC($$, @$); }
	| FLO
	{ $$ = {{"expr-type", "lit-flo"}, {"value", move($1)}}; LOC($$, @$); }
	| STRING
	{ $$ = {{"expr-type", "lit-str"}, {"value", move($1)}}; LOC($$, @$); }
	| KW_TRUE
	{ $$ = boolLiteral("1"); LOC($$, @$); }
	| KW_FALSE
	{ $$ = boolLiteral("0"); LOC($$, @$); }
	| KW_SIZEOF '(' type_expr ')'
	{
		if (isImplementedTypeExpr($3)) {
			$$ = {{"expr-type", "sizeof"}, {"type", move($3)}};
			LOC($$, @$);
		} else {
			$$ = {{"expr-type", "not-impl"}};
			LOC($$, @$);
		}
	}
	| ID
	{ $$ = {{"expr-type", "id"}, {"name", move($1)}}; LOC($$, @$); }
	| '(' tapple_inner ')'
	{
		// Single-expression grouping (e.g. -(2+3)): pass the inner expression through.
		// Multi-expression tapple (e.g. (a, b)) is only a multiple-assignment target.
		if ($2.count("not-impl") || $2.count("tapple-items")) {
			$$ = {{"expr-type", "not-impl"}};
			LOC($$, @$);
		} else {
			$$ = $2;
		}
	}
	| term '.' ID
	{ $$ = {{"expr-type", "field-access"}, {"object", move($1)}, {"field", move($3)}}; LOC($$, @$); }
	| term '[' expression ']'
	{
		$$ = {{"expr-type", "arr-index"}, {"array", move($1)}, {"index", move($3)}};
		LOC($$, @$);
	}
	| ID '(' arguments ')'
	{
		if (typeNames.count($1)) {
			if ($3.size == 1) {
				$$ = {{"expr-type",   "cast"},
				      {"target-type", {{"type-kind", "prim"}, {"type-name", move($1)}}},
				      {"src",         $3.last->item}}; // LCOV_EXCL_EXCEPTION_BR_LINE
				LOC($$, @$);
			} else {
				$$ = {{"expr-type", "not-impl"}};
				LOC($$, @$);
			}
		} else {
			$$ = {{"expr-type", "call"}, {"name", move($1)}, {"args", $3.toVector()}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			LOC($$, @$);
		}
	}
	| term '.' ID '(' arguments ')'
	{
		$$ = {{"expr-type", "member-call"},
		      {"object", move($1)},
		      {"method", move($3)},
		      {"args", $5.toVector()}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC($$, @$);
	}
	;

tapple_inner: expression
	{ $$ = $1; }
	| '-'
	{ $$ = {{"not-impl", true}}; }
	| tapple_inner ',' expression
	{
		if ($1.count("not-impl")) {
			$$ = move($1);
		} else if ($1.count("tapple-items")) {
			$$ = move($1);
			$$["tapple-items"].push_back(move($3));
		} else {
			$$ = {{"tapple-items", json::array({move($1), move($3)})}};
		}
	}
	| tapple_inner ',' '-'
	{ $$ = {{"not-impl", true}}; }
	;

arguments: /* empty */
	{ }
	| expression
	{ $$ = PList<json>().push($1); }
	| expression DBL_GRTR
	{
		json arg = {{"expr-type", "not-impl"}};
		LOC(arg, @$);
		$$ = PList<json>().push(move(arg));
	}
	| arguments ',' expression
	{ $$ = $1.push($3); }
	| arguments ',' expression DBL_GRTR
	{
		json arg = {{"expr-type", "not-impl"}};
		LOC_BE(arg, @3, @4);
		$$ = $1.push(move(arg));
	}
	;

array_desc: array_row
	{ $$ = move($1); }
	| array_rows
	{
		// The concatenated form [a,b][c,d] yields the same AST as the nested [[a,b],[c,d]].
		$$ = {{"expr-type", "arr-lit"}, {"items", $1.toVector()}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC($$, @$);
	}
	;

array_rows: array_row array_row
	{ $$ = PList<json>().push($1).push($2); }
	| array_rows array_row
	{ $$ = $1.push($2); }
	;

array_row: '[' array_items ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| '[' array_items ',' ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| '[' ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", json::array()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	;

array_items: expression
	{ $$ = PList<json>().push($1); }
	| array_items ',' expression
	{ $$ = $1.push($3); }
	;

dict_desc: '{' dict_items '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| '{' dict_items ',' '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); } // LCOV_EXCL_EXCEPTION_BR_LINE
	;

dict_items: ID ':' expression
	{
		json item = {{"name", move($1)}, {"value", move($3)}};
		LOC(item, @$);
		$$ = PList<json>().push(move(item));
	}
	| dict_items ',' ID ':' expression
	{
		json item = {{"name", move($3)}, {"value", move($5)}};
		LOC_BE(item, @3, @5);
		$$ = $1.push(move(item));
	}
	;

store_loc
	: ID
	{ $$ = {{"kind", "var"}, {"name", move($1)}}; LOC($$, @$); }
	| store_loc '[' expression ']'
	{
		json array_expr = storeLocToExpr($1);
		$$ = {{"kind", "arr-index"}, {"array", move(array_expr)}, {"index", move($3)}};
		LOC($$, @$);
	}
	| store_loc '.' ID
	{ $$ = {{"kind", "field"}, {"base", move($1)}, {"field", move($3)}}; LOC($$, @$); }
	| '(' tapple_inner ')'
	{
		$$ = {{"kind", "not-impl"}};
		if (!$2.count("tapple-items") && !$2.count("not-impl")) {
			$$ = {{"kind", "expr"}, {"expr", move($2)}};
		} else if ($2.count("tapple-items")) {
			bool all_ok = true;
			for (auto& t : $2["tapple-items"]) {
				string et = t.value("expr-type", "");
				if (et != "id" && et != "arr-index" && et != "field-access") { all_ok = false; break; }
			}
			if (all_ok) $$ = {{"kind", "tapple"}, {"targets", move($2["tapple-items"])}};
		}
		LOC($$, @$);
	}
	// Not term's call rules: a term-rooted member call would let every `-> x[i]`
	// also parse as the object of a pending `.f()` and split the GLR parse
	// exponentially along an assignment chain.
	| ID '(' arguments ')'
	{
		$$ = {{"kind", "not-impl"}};
		if (!typeNames.count($1)) {
			$$ = {{"kind", "call"},
			      {"call", {{"expr-type", "call"}, {"name", move($1)}, {"args", $3.toVector()}}}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			LOC($$["call"], @$);
		}
		LOC($$, @$);
	}
	| store_loc '.' ID '(' arguments ')'
	{
		$$ = {{"kind", "not-impl"}};
		json obj = storeLocToExpr($1);
		if (obj.value("expr-type", "") != "not-impl") {
			$$ = {{"kind", "call"},
			      {"call", {{"expr-type", "member-call"}, {"object", move(obj)},
			                {"method", move($3)}, {"args", $5.toVector()}}}}; // LCOV_EXCL_EXCEPTION_BR_LINE
			LOC($$["call"], @$);
		}
		LOC($$, @$);
	}
	;

func_def: KW_FUNC ID '(' paramaters ')' return_def block_obj
	{
		json fn = funcDecl(ast, false, {{"name", $2}, {"func-type", "palan"}}, $4, $6, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $7}}, {} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| KW_EXPORT KW_FUNC ID '(' paramaters ')' return_def block_obj
	{
		json fn = funcDecl(ast, true, {{"name", $3}, {"func-type", "palan"}}, $5, $7, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $8}}, {} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

func_item: func_def
	{ $$ = $1; }
	| syscall_decl
	{ $$ = $1; }
	;

// A syscall declaration binds a number to a name callable like any other
// function (Linux syscall ABI, not System V -- see doc/SpecAndDesign.md).
syscall_decl: KW_SYSCALL ID '(' paramaters ')' syscall_ret '=' expression ';'
	{
		json fn = funcDecl(ast, false, {{"name", $2}, {"func-type", "syscall"}, {"syscall-number", $8}}, $4, $6, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = leafNode(move(fn)); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| KW_EXPORT KW_SYSCALL ID '(' paramaters ')' syscall_ret '=' expression ';'
	{
		json fn = funcDecl(ast, true, {{"name", $3}, {"func-type", "syscall"}, {"syscall-number", $9}}, $5, $7, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = leafNode(move(fn)); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

paramaters: /* empty */
	{ }
	| var_declarations
	{ $$ = $1.toVector(); }
	;

return_def: /* empty */
	{ }
	| ARROW var_declarations
	{ $$["rets"] = $2.toVector(); }
	| ARROW ret_type
	{ $$["ret-type"] = move($2); }
	;

// Named returns get no "= expr" initializer here: a syscall has no body to
// use them in, and allowing one would collide with the syscall number's "=".
syscall_ret: /* empty */
	{ }
	| ARROW ret_type
	{ $$["ret-type"] = move($2); }
	| ARROW syscall_named_rets
	{
		throw runtime_error(PlnGenAstMessage::locatedError(lexer.inputFile,
			@2.begin.line, @2.begin.column, PlnGenAstMessage::getMessage(E_SyscallNamedReturn)));
	}
	;

syscall_named_rets: type_expr ID
	| syscall_named_rets ',' type_expr ID
	| syscall_named_rets ',' ID
	;

ret_type: type_expr
	{
		if (isDeclarableVarType($1)) {
			$$ = move($1);
		} else {
			$$ = {{"not-impl", true}};
			LOC($$, @1);
		}
	}
	;

noname_func: KW_FUNC '(' paramaters ')'
	return_def block
	;

construct_def: KW_CONSTRUCT type_expr '(' paramaters ')' block
	| KW_EXPORT KW_CONSTRUCT type_expr '(' paramaters ')' block
	;

return: KW_RETURN
	{ }
	| KW_RETURN expressions
	{ $$["values"] = $2.toVector(); }
	;

// 'in' stays an identifier elsewhere: C structs and existing code name fields 'in'.
for_loop: KW_FOR for_elem ID expression block
	{
		requireInKeyword(lexer.inputFile, $3, @3);
		json f = {{"stmt-type", "for"}, {"elem", move($2)}, {"range", move($4)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC(f, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(f), {}, {{"body", $5}} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	| KW_FOR for_elem ',' ID ID expression block
	{
		requireInKeyword(lexer.inputFile, $5, @5);
		json f = {{"stmt-type", "for"}, {"elem", move($2)}, {"index", $4}, {"range", move($6)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC(f, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(f), {}, {{"body", $7}} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

for_elem: ID
	{ $$ = {{"name", $1}, {"borrow", false}}; LOC($$, @$); }
	| '@' ID
	{ $$ = {{"name", $2}, {"borrow", true}, {"mutable", false}}; LOC($$, @$); }
	| AT_EXCL ID
	{ $$ = {{"name", $2}, {"borrow", true}, {"mutable", true}}; LOC($$, @$); }
	;

while_loop: KW_WHILE expression block
	{
		json w = {{"stmt-type", "while"}, {"cond", $2}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC(w, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(w), {}, {{"body", $3}} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

if_stmt: KW_IF expression block else_stmt
	{
		json i = {{"stmt-type", "if"}, {"cond", $2}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		LOC(i, @$); // LCOV_EXCL_EXCEPTION_BR_LINE
		vector<std::pair<string, NodeRef>> children = {{"then", blockStmtNode($3)}}; // LCOV_EXCL_EXCEPTION_BR_LINE
		if ($4) children.push_back({"else", $4});
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(i), move(children), {} }); // LCOV_EXCL_EXCEPTION_BR_LINE
	}
	;

else_stmt: /* empty */
	{ }
	| KW_ELSE block
	{ $$ = blockStmtNode($2); }
	| KW_ELSE if_stmt
	{ $$ = $2; }
	;

expressions: expression
	{ $$ = PList<json>().push($1); }
	| expressions ',' expression
	{ $$ = $1.push($3); }
	;

type_expr: ID
	{ $$ = {{"type-kind","prim"},{"type-name",move($1)}}; }
	| ID '.' ID
	{ $$ = {{"type-kind","prim"},{"type-name",$1 + "." + $3}}; }
	| '@' type_expr
	{ $$ = pntrTypeExpr(move($2), false); }
	| AT_EXCL type_expr
	{ $$ = pntrTypeExpr(move($2), true); }
	| '@' KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), false); }
	| AT_EXCL KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), true); }
	| '@' KW_FUNC '(' fptr_params ')' fptr_ret
	{ $$ = pntrTypeExpr(funcTypeExpr($4.toVector(), move($6)), false); }
	| AT_EXCL KW_FUNC '(' fptr_params ')' fptr_ret
	{ $$ = pntrTypeExpr(funcTypeExpr($4.toVector(), move($6)), true); }
	| '$' type_expr
	{ $$ = {{"type-kind","embed"},{"base-type",move($2)}}; }
	| '[' expression ']' type_expr
	{
		if ($4.value("type-kind","") == "embed")
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",move($2)},{"embedded",true},{"base-type",move($4["base-type"])}};
		else
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",move($2)},{"base-type",move($4)}};
	}
	| '[' ']' type_expr
	{
		if ($3.value("type-kind","") == "embed")
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",nullptr},{"embedded",true},{"base-type",move($3["base-type"])}};
		else
			$$ = {{"type-kind","arr"},{"specifier","raw"},{"size-expr",nullptr},{"base-type",move($3)}};
	}
	| '[' '#' ']' type_expr
	{ $$ = {{"type-kind","arr"},{"specifier","fixed"},{"size-expr",nullptr},{"base-type",move($4)}}; }
	| '[' '#' expression ']' type_expr
	{ $$ = {{"type-kind","arr"},{"specifier","fixed"},{"size-expr",move($3)},{"base-type",move($5)}}; }
	| '[' '+' ']' type_expr
	{ $$ = varrTypeExpr(nullptr, move($4)); }
	| '[' '+' expression ']' type_expr
	{ $$ = varrTypeExpr(move($3), move($5)); }
	;

fptr_params: /* empty */
	{ }
	| fptr_param_list
	{ $$ = $1; }
	;

fptr_param_list: type_expr
	{ $$ = PList<json>().push({{"var-type", move($1)}}); } // LCOV_EXCL_EXCEPTION_BR_LINE
	| fptr_param_list ',' type_expr
	{ $$ = $1.push({{"var-type", move($3)}}); } // LCOV_EXCL_EXCEPTION_BR_LINE
	;

fptr_ret: /* empty */ %prec FPTR_NORET
	{ $$ = voidTypeExpr(); }
	| ARROW type_expr
	{ $$ = move($2); }
	;


%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cerr << PlnGenAstMessage::locatedError(lexer.inputFile, l.begin.line, l.begin.column, m) << endl;
}

