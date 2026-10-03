/// Palan Parser
///
/// @file PlnParser.yy
/// @copyright 2024-2025 YAMAGUCHI Toshinobu

%glr-parser
%language "c++"
%require "3.8"
%skeleton "glr2.cc"

%defines
%define api.parser.class	{PlnParser}
%parse-param	{PlnLexer& lexer}	{json& ast}	{MacroTable& macros}
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

	PList push(T item) const { return { std::make_shared<const Node>(Node{ std::move(item), last }), size + 1 }; }

	vector<T> toVector() const {
		vector<T> v(size);
		const Node* n = last.get();
		for (size_t i = size; i > 0; --i, n = n->prev.get()) v[i-1] = n->item;
		return v;
	}

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
	}
	static json toJsonArray(const PList<NodeRef>& list) {
		json arr = json::array();
		for (auto& n : list.toVector()) arr.push_back(n->toJson());
		return arr;
	}
};

inline NodeRef leafNode(json j) { return std::make_shared<const LazyNode>(LazyNode{ std::move(j), {}, {} }); }
inline NodeRef blockStmtNode(const PList<NodeRef>& body) {
	return std::make_shared<const LazyNode>(LazyNode{ {{"stmt-type", "block"}}, {}, {{"body", body}} });
}

struct BodyList {
	PList<NodeRef> functions, body;
	NodeRef toNode() const {
		return std::make_shared<const LazyNode>(LazyNode{ json::object(), {}, {{"functions", functions}, {"body", body}} });
	}
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

	// Returns null when the signature uses a not-impl feature.
	static json funcDecl(json& ast, bool exported, json fn, const vector<json>& params, const json& ret,
	                     const palan::PlnParser::location_type& loc)
	{
		for (auto& p : params)
			if (p.count("not-impl")) return nullptr;
		fn["parameters"] = params;
		if (ret.contains("rets"))
			fn["rets"] = ret["rets"];
		else if (ret.contains("ret-type"))
			fn["ret-type"] = ret["ret-type"];
		LOC(fn, loc);
		if (exported) {
			fn["export"] = true;
			ast["export"].push_back(fn);
		}
		return fn;
	}
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
%token KW_TRUE	"true"
%token KW_FALSE	"false"
%token OPE_LE	"<="
%token OPE_GE	">="
%token DBL_GRTR	">>"
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
%type <json>	expression func_call term store_loc
%type <PList<json>>	arguments
%type <json>	array_desc array_row dict_desc
%type <PList<json>>	array_rows array_items dict_items
%type <json>	type_expr
%type <json>	var_declaration inherit_var_decl
%type <PList<json>>	var_declarations
%type <NodeRef>	func_def func_item syscall_decl
%type <json>	return_def
%type <json>	return
%type <vector<json>>	paramaters
%type <PList<json>>	expressions
%type <NodeRef>	block_obj standalone_block block_body_items
%type <json>	tapple_decl tapple_inner
%type <PList<json>>	tapple_decl_inner
%type <NodeRef>	if_stmt else_stmt while_loop
%type <json>		type_decl type_member const_decl
%type <PList<json>>	type_members

%left ARROW DBL_ARROW
%left OPE_OR
%left OPE_AND
%left OPE_EQ OPE_NE
%left '<' '>' OPE_LE OPE_GE
%left '+' '-'
%left '*' '/' '%' '&' '|' '^'
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
	{ json j = {{"stmt-type", "not-impl"}}; LOC(j, @$); $$ = leafNode(move(j)); }
	| for_loop
	{ json j = {{"stmt-type", "not-impl"}}; LOC(j, @$); $$ = leafNode(move(j)); }
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
		                          fs::path(lexer.inputFile).parent_path().string());
		if (c_ast.is_object() && c_ast.contains("ast")) {
			if (c_ast["ast"].contains("functions")) {
				$$["functions"] = move(c_ast["ast"]["functions"]);
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
	}
	| var_declarations
	{
		vector<json> vars = $1.toVector();
		// Detect a tapple-decl emitted by var_declaration
		if (vars.size() == 1 && vars[0].value("stmt-type", "") == "tapple-decl") {
			$$ = move(vars[0]);
			LOC($$, @$);
		} else {
			bool all_ok = true;
			for (auto& v : vars) {
				if (v.count("not-impl") || v.value("stmt-type","") == "tapple-decl"
					|| v.value("inherit-type", false)) { all_ok = false; break; }
			}
			if (all_ok) {
				$$ = {{"stmt-type", "var-decl"}, {"vars", move(vars)}};
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
		} else if ($1.contains("name")) {
			$$ = {{"stmt-type", "struct-def"}, {"name", $1["name"]}, {"fields", move($1["fields"])}};
			LOC($$, @$);
		} else {
			$$ = {{"stmt-type", "not-impl"}};
			LOC($$, @$);
		}
	}
	| interface_decl
	{ $$ = {{"stmt-type", "not-impl"}}; LOC($$, @$); }
	| expression
	{
		string et = $1.value("expr-type", "");
		if (et == "assign-expr") {
			if ($1["value"].value("expr-type", "") != "not-impl") {
				$$ = {{"stmt-type", "assign"}, {"name", $1["name"]}, {"value", move($1["value"])}};
				LOC($$, @$);
			} else {
				$$ = {{"stmt-type", "not-impl"}};
				LOC($$, @$);
			}
		} else if (et == "arr-assign-expr") {
			$$ = {{"stmt-type", "arr-assign"}, {"target", move($1["target"])}, {"value", move($1["value"])}};
			if ($1.value("ownership-transfer", false)) $$["ownership-transfer"] = true;
			LOC($$, @$);
		} else if (et == "field-assign-expr") {
			$$ = {{"stmt-type", "field-assign"},
				  {"object", storeLocToExpr($1["base"])}, {"field", move($1["field"])},
				  {"value", move($1["value"])}};
			if ($1.value("ownership-transfer", false)) $$["ownership-transfer"] = true;
			LOC($$, @$);
		} else if (et == "tapple-assign-expr") {
			$$ = {{"stmt-type", "tapple-assign"}, {"targets", move($1["targets"])}, {"value", move($1["value"])}};
			LOC($$, @$);
		} else if (et != "not-impl") {
			$$ = {{"stmt-type", "expr"}, {"body", move($1)}};
			LOC($$, @$);
		} else {
			$$ = {{"stmt-type", "not-impl"}};
			LOC($$, @$);
		}
	}
	| return
	{
		$$ = {{"stmt-type", "return"}};
		if ($1.contains("values")) {
			bool all_ok = true;
			for (auto& v : $1["values"])
				if (v.value("expr-type", "") == "not-impl") { all_ok = false; break; }
			if (all_ok) {
				$$["values"] = move($1["values"]);
				LOC($$, @$);
			} else {
				$$ = {{"stmt-type", "not-impl"}};
				LOC($$, @$);
			}
		} else {
			LOC($$, @$);
		}
	}
	| term DBL_PLUS
	{ $$ = {{"stmt-type", "not-impl"}}; LOC($$, @$); }
	| KW_BREAK
	{ $$ = {{"stmt-type", "break"}}; LOC($$, @$); }
	| KW_CONTINUE
	{ $$ = {{"stmt-type", "continue"}}; LOC($$, @$); }
	;

stmt_list_e: expr_stmt
	{ $$ = PList<NodeRef>().push(leafNode($1)); }
	| stmt_list_b expr_stmt
	{ $$ = $1.push(leafNode($2)); }
	| stmt_list_b ';' expr_stmt
	{ $$ = $1.push(leafNode($3)); }
	| stmt_list_e ';' expr_stmt
	{ $$ = $1.push(leafNode($3)); }
	;

stmt_list_b: block_stmt
	{ $$ = PList<NodeRef>().push($1); }
	| func_item
	{
		if (!$1->self.count("not-impl"))
			ast["ast"]["functions"].push_back($1->toJson());
	}
	| stmt_list_b block_stmt
	{ $$ = $1.push($2); }
	| stmt_list_b func_item
	{
		$$ = $1;
		if (!$2->self.count("not-impl"))
			ast["ast"]["functions"].push_back($2->toJson());
	}
	| stmt_list_b ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_b ';' func_item
	{
		$$ = $1;
		if (!$3->self.count("not-impl"))
			ast["ast"]["functions"].push_back($3->toJson());
	}
	| stmt_list_e ';' block_stmt
	{ $$ = $1.push($3); }
	| stmt_list_e ';' func_item
	{
		$$ = $1;
		if (!$3->self.count("not-impl"))
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
		if ($2.size) { $$["targets"] = $2.toVector(); }
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
		if ($4.size()) {
			$$["libs"] = move($4);
		}
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
		$$.body = $$.body.push(leafNode($1));
	}
	| body_list_b expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode($2));
	}
	| body_list_b ';' expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode($3));
	}
	| body_list_e ';' expr_stmt
	{
		$$ = $1;
		$$.body = $$.body.push(leafNode($3));
	}
	;

body_list_b: block_stmt
	{
		$$.body = $$.body.push($1);
	}
	| func_item
	{
		if (!$1->self.count("not-impl"))
			$$.functions = $$.functions.push($1);
	}
	| body_list_b block_stmt
	{
		$$ = $1;
		$$.body = $$.body.push($2);
	}
	| body_list_b func_item
	{
		$$ = $1;
		if (!$2->self.count("not-impl"))
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
		if (!$3->self.count("not-impl"))
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
		if (!$3->self.count("not-impl"))
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
		LOC(blk.self, @$);
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
		if (isDeclarableVarType($1))
			$$ = {{"name", $2}, {"var-type", move($1)}};
		else
			$$ = {{"not-impl", true}};
	}
	| type_expr DBL_GRTR ID
	{ $$ = {{"not-impl", true}}; }
	| type_expr ID '=' expression
	{
		if (isDeclarableVarType($1))
			$$ = {{"name", $2}, {"var-type", move($1)}, {"init", move($4)}};
		else
			$$ = {{"not-impl", true}};
	}
	| ID '=' expression
	{ $$ = {{"not-impl", true}, {"untyped-var", $1}}; }
	| tapple_decl '=' expression
	{
		string et = $3.value("expr-type","");
		if ($1.is_array() && (et == "call" || et == "member-call"))
			$$ = {{"stmt-type", "tapple-decl"}, {"vars", $1}, {"value", $3}};
		else
			$$ = {{"not-impl", true}};
	}
	;

tapple_decl: '(' tapple_decl_inner ')'
	{ $$ = $2.toVector(); }
	;

tapple_decl_inner: type_expr ID
	{ $$ = PList<json>().push({{"var-name", $2}, {"var-type", $1}}); }
	| KW_VOID
	{ }
	| tapple_decl_inner ',' type_expr ID
	{ $$ = $1.push({{"var-name", $4}, {"var-type", $3}}); }
	| tapple_decl_inner ',' KW_VOID
	{ $$ = $1; }
	| tapple_decl_inner ',' ID   %dprec 1
	{
	  if (auto typed = $1.findLast([](const json& v) { return v.contains("var-type"); }))
	      $$ = $1.push({{"var-name", $3}, {"var-type", (*typed)["var-type"]}});
	  else
	      $$ = $1;
	}
	;

const_decl: KW_CONST ID '=' expression
	{ $$ = {{"name", $2}, {"value", move($4)}}; LOC($$, @$); }
	;

type_decl: KW_TYPE ID implememts '{' type_members '}'
	{ $$ = {{"name", $2}, {"fields", $5.toVector()}}; LOC($$, @$); }
	| KW_EXPORT KW_TYPE ID implememts '{' type_members '}'
	{ $$ = {{"name", $3}, {"fields", $6.toVector()}}; LOC($$, @$); }
	| KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $2}, {"alias-of", move($4)}}; LOC($$, @$); }
	| KW_EXPORT KW_TYPE ID '=' type_expr
	{ $$ = {{"name", $3}, {"alias-of", move($5)}}; LOC($$, @$); }
	| KW_TYPE ID
	{ $$ = json{}; }
	| KW_EXPORT KW_TYPE ID
	{ $$ = json{}; }
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
		$$ = $2.value("not-impl", false) ? $1 : $1.push($2);
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
	| KW_INTERFACE ID '<' temp_ids '>' '{' interface_methods '}'
	| KW_EXPORT KW_INTERFACE ID '<' temp_ids '>' '{' interface_methods '}'
	;

interface_methods: /* empty */ 
	| interface_methods KW_FUNC ID '(' paramaters ')' return_def ';'
	| interface_methods KW_FUNC ID '(' paramaters ')' return_def block
	;

expression: term
	{ $$ = move($1); }
	| func_call
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
	| '@' store_loc
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", false}}; LOC($$, @$); }
	| AT_EXCL store_loc
	{ $$ = {{"expr-type", "addr-of"}, {"object", storeLocToExpr($2)}, {"mutable", true}}; LOC($$, @$); }
	| expression ARROW store_loc
	{
		if ($3.value("kind", "") == "var") {
			$$ = {{"expr-type", "assign-expr"}, {"name", move($3["name"])}, {"value", move($1)}};
			LOC($$, @$);
		} else if ($3.value("kind", "") == "arr-index") {
			json arr_node = {{"expr-type", "arr-index"},
							 {"array", move($3["array"])},
							 {"index", move($3["index"])}};
			LOC(arr_node, @3);
			$$ = {{"expr-type", "arr-assign-expr"}, {"target", move(arr_node)}, {"value", move($1)}};
			LOC($$, @$);
		} else if ($3.value("kind", "") == "field") {
			$$ = {{"expr-type", "field-assign-expr"},
				  {"base", move($3["base"])}, {"field", move($3["field"])},
				  {"value", move($1)}};
			LOC($$, @$);
		} else if ($3.value("kind", "") == "tapple") {
			string et = $1.value("expr-type", "");
			if (et == "call" || et == "member-call") {
				$$ = {{"expr-type", "tapple-assign-expr"}, {"targets", move($3["targets"])}, {"value", move($1)}};
				LOC($$, @$);
			} else {
				$$ = {{"expr-type", "not-impl"}};
			}
		} else {
			$$ = {{"expr-type", "not-impl"}};
		}
	}
	| expression DBL_ARROW store_loc
	{
		if ($3.value("kind", "") == "arr-index") {
			json arr_node = {{"expr-type", "arr-index"},
							 {"array", move($3["array"])},
							 {"index", move($3["index"])}};
			LOC(arr_node, @3);
			$$ = {{"expr-type", "arr-assign-expr"}, {"ownership-transfer", true},
				  {"target", move(arr_node)}, {"value", move($1)}};
			LOC($$, @$);
		} else if ($3.value("kind", "") == "field") {
			$$ = {{"expr-type", "field-assign-expr"}, {"ownership-transfer", true},
				  {"base", move($3["base"])}, {"field", move($3["field"])},
				  {"value", move($1)}};
			LOC($$, @$);
		} else {
			$$ = {{"expr-type", "not-impl"}};
		}
	}
	| noname_func
	{ $$ = {{"expr-type", "not-impl"}}; }
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
	| ID
	{ $$ = {{"expr-type", "id"}, {"name", move($1)}}; LOC($$, @$); }
	| '(' tapple_inner ')'
	{
		// Single-expression grouping (e.g. -(2+3)): pass the inner expression through.
		// Multi-expression tapple (e.g. (a, b)) is only a multiple-assignment target.
		if ($2.count("not-impl") || $2.count("tapple-items"))
			$$ = {{"expr-type", "not-impl"}};
		else
			$$ = $2;
	}
	| term '.' ID
	{ $$ = {{"expr-type", "field-access"}, {"object", move($1)}, {"field", move($3)}}; LOC($$, @$); }
	| term '[' expression ']'
	{
		$$ = {{"expr-type", "arr-index"}, {"array", move($1)}, {"index", move($3)}};
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

func_call: ID '(' arguments ')'
	{
		if (typeNames.count($1)) {
			if ($3.size == 1) {
				$$ = {{"expr-type",   "cast"},
				      {"target-type", {{"type-kind", "prim"}, {"type-name", move($1)}}},
				      {"src",         $3.last->item}};
				LOC($$, @$);
			} else {
				$$ = {{"expr-type", "not-impl"}};
			}
		} else {
			$$ = {{"expr-type", "call"}, {"name", move($1)}, {"args", $3.toVector()}};
			LOC($$, @$);
		}
	}
	| expression '.' ID '(' arguments ')'
	{
		$$ = {{"expr-type", "member-call"},
		      {"object", move($1)},
		      {"method", move($3)},
		      {"args", $5.toVector()}};
		LOC($$, @$);
	}
	;

arguments: /* empty */
	{ }
	| expression
	{ $$ = PList<json>().push($1); }
	| expression DBL_GRTR
	{ $$ = PList<json>().push($1); }
	| arguments ',' expression
	{ $$ = $1.push($3); }
	| arguments ',' expression DBL_GRTR
	{ $$ = $1.push($3); }
	;

array_desc: array_row
	{ $$ = move($1); }
	| array_rows
	{
		// The concatenated form [a,b][c,d] yields the same AST as the nested [[a,b],[c,d]].
		$$ = {{"expr-type", "arr-lit"}, {"items", $1.toVector()}};
		LOC($$, @$);
	}
	;

array_rows: array_row array_row
	{ $$ = PList<json>().push($1).push($2); }
	| array_rows array_row
	{ $$ = $1.push($2); }
	;

array_row: '[' array_items ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); }
	| '[' array_items ',' ']'
	{ $$ = {{"expr-type", "arr-lit"}, {"items", $2.toVector()}}; LOC($$, @$); }
	;

array_items: expression
	{ $$ = PList<json>().push($1); }
	| array_items ',' expression
	{ $$ = $1.push($3); }
	;

dict_desc: '{' dict_items '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); }
	| '{' dict_items ',' '}'
	{ $$ = {{"expr-type", "dict-lit"}, {"items", $2.toVector()}}; LOC($$, @$); }
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
		if ($2.count("tapple-items")) {
			bool all_ok = true;
			for (auto& t : $2["tapple-items"]) {
				string et = t.value("expr-type", "");
				if (et != "id" && et != "arr-index" && et != "field-access") { all_ok = false; break; }
			}
			if (all_ok) $$ = {{"kind", "tapple"}, {"targets", move($2["tapple-items"])}};
		}
	}
	| func_call
	{ $$ = {{"kind", "not-impl"}}; }
	;

func_def: KW_FUNC ID '(' paramaters ')' return_def block_obj
	{
		json fn = funcDecl(ast, false, {{"name", $2}, {"func-type", "palan"}}, $4, $6, @$);
		$$ = fn.is_null() ? leafNode({{"not-impl", true}})
		                  : std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $7}}, {} });
	}
	| KW_EXPORT KW_FUNC ID '(' paramaters ')' return_def block_obj
	{
		json fn = funcDecl(ast, true, {{"name", $3}, {"func-type", "palan"}}, $5, $7, @$);
		$$ = fn.is_null() ? leafNode({{"not-impl", true}})
		                  : std::make_shared<const LazyNode>(LazyNode{ move(fn), {{"block", $8}}, {} });
	}
	;

func_item: func_def
	{ $$ = $1; }
	| syscall_decl
	{ $$ = $1; }
	;

// A syscall declaration binds a number to a name callable like any other
// function (Linux syscall ABI, not System V -- see doc/SpecAndDesign.md).
// "=" is otherwise unused between ')' and ';', so return_def's own optional
// "= expr" tail (named-return initializer) is forced to fail at ';' instead
// of surviving as a second GLR parse -- adding parameter defaults or an "="
// expression operator would break this forced split.
syscall_decl: KW_SYSCALL ID '(' paramaters ')' return_def '=' expression ';'
	{
		json fn = funcDecl(ast, false, {{"name", $2}, {"func-type", "syscall"}, {"syscall-number", $8}}, $4, $6, @$);
		$$ = leafNode(fn.is_null() ? json{{"not-impl", true}} : move(fn));
	}
	| KW_EXPORT KW_SYSCALL ID '(' paramaters ')' return_def '=' expression ';'
	{
		json fn = funcDecl(ast, true, {{"name", $3}, {"func-type", "syscall"}, {"syscall-number", $9}}, $5, $7, @$);
		$$ = leafNode(fn.is_null() ? json{{"not-impl", true}} : move(fn));
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
	{
		vector<json> rets = $2.toVector();
		bool all_ok = true;
		for (auto& v : rets)
			if (v.count("not-impl") || v.value("inherit-type", false)) { all_ok = false; break; }
		if (all_ok)
			$$["rets"] = move(rets);
	}
	| ARROW type_expr
	{
		if (isDeclarableVarType($2))
			$$["ret-type"] = move($2);
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

for_loop: KW_FOR ID ':' expression block
	;

while_loop: KW_WHILE expression block
	{
		json w = {{"stmt-type", "while"}, {"cond", $2}};
		LOC(w, @$);
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(w), {}, {{"body", $3}} });
	}
	;

if_stmt: KW_IF expression block else_stmt
	{
		json i = {{"stmt-type", "if"}, {"cond", $2}};
		LOC(i, @$);
		vector<std::pair<string, NodeRef>> children = {{"then", blockStmtNode($3)}};
		if ($4) children.push_back({"else", $4});
		$$ = std::make_shared<const LazyNode>(LazyNode{ move(i), move(children), {} });
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
	| ID '<' temp_ids '>'
	{ $$ = {{"not-impl",true}}; }
	| '@' type_expr
	{ $$ = pntrTypeExpr(move($2), false); }
	| AT_EXCL type_expr
	{ $$ = pntrTypeExpr(move($2), true); }
	| '@' KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), false); }
	| AT_EXCL KW_VOID
	{ $$ = pntrTypeExpr(voidTypeExpr(), true); }
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
	{ $$ = {{"type-kind","arr"},{"specifier","variable"},{"size-expr",nullptr},{"base-type",move($4)}}; }
	| '[' '+' expression ']' type_expr
	{ $$ = {{"type-kind","arr"},{"specifier","variable"},{"size-expr",move($3)},{"base-type",move($5)}}; }
	;

temp_ids: ID | temp_ids ',' ID
	;


%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cerr << PlnGenAstMessage::locatedError(lexer.inputFile, l.begin.line, l.begin.column, m) << endl;
}

