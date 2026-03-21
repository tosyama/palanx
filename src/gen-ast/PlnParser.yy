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
%parse-param	{PlnLexer& lexer}	{json& ast}
%lex-param	{PlnLexer& lexer}

%code requires
{
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <cstdio>

#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "../common/PlnFileUtils.h"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using json = nlohmann::json;
namespace fs = std::filesystem;

class PlnLexer;
}

%code
{
	#include <set>
	#include "PlnLexer.h"

	static std::set<std::string> typeNames = {
		"int8",  "int16",  "int32",  "int64",
		"uint8", "uint16", "uint32", "uint64",
		"flo32", "flo64"
	};

	int yylex(
		palan::PlnParser::value_type* yylval,
		palan::PlnParser::location_type* location,
		PlnLexer& lexer)
	{
		return lexer.yylex(*yylval, *location);
	}

	static json execute_c2ast(const string& path_type, const string& path)
	{
		fs::path exec_file_path = fs::canonical("/proc/self/exe");
		string exec_path = exec_file_path.parent_path().string();
		string c2ast_path = exec_path + "/palan-c2ast";

		string cmd;
		if (path_type == "inc") {
			cmd = c2ast_path + " -s " + path;
		} else {
			cmd = c2ast_path + " " + path;
		}

		FILE* pipe = popen(cmd.c_str(), "r");
		if (!pipe) {
			cerr << "palan-c2ast: popen failed: " << cmd << endl;
			return json{};
		}

		string result;
		char buf[4096];
		while (fgets(buf, sizeof(buf), pipe)) result += buf;
		int status = pclose(pipe);
		if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
			cerr << "palan-c2ast: exited with " << WEXITSTATUS(status) << ": " << cmd << endl;
			return json{};
		}

		if (result.empty()) return json{};
		json parsed = json::parse(result, nullptr, false);  // no exception
		if (parsed.is_discarded()) {
			cerr << "palan-c2ast: JSON parse failed" << endl;
			return json{};
		}
		return parsed;
	}
}

%locations
%define api.namespace	{palan}
%define api.value.type	variant
%define parse.error	verbose

%token <string>	INT	"integer"
%token <string>	UINT	"unsigned integer"
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
%token OPE_LE	"<="
%token OPE_GE	">="
%token DBL_GRTR	">>"
%token ARROW	"->"
%token DBL_ARROW	"->>"
%token AT_EXCL	"@!"
%token DBL_PLUS	"++"

%type <vector<json>>	statements
%type <json>	statement
%type <json>	import cinclude import_path
%type <vector<string>>	import_ids
%type <string>	import_as
%type <json>	expression func_call term
%type <vector<json>>	arguments
%type <string>	var_type var_prefix
%type <bool>	var_postfix
%type <json>	var_declaration inherit_var_decl
%type <vector<json>>	var_declarations
%type <json>	func_def return_def
%type <json>	return
%type <vector<json>>	block paramaters expressions block_statements
%type <json>	block_obj standalone_block block_body_items
%type <json>	statement_or_funcdef
%type <bool>	move_owner_r do_export
%type <json>	tapple_decl tapple_decl_inner

%left ARROW DBL_ARROW
%left '<' '>' OPE_LE OPE_GE
%left '+' '-'
%left '*' '/' '%' '&' '|'
%left '.'

%start module

%%
module: statements
	{
		if (!ast["ast"].contains("functions"))
			ast["ast"]["functions"] = json::array();
		ast["ast"]["statements"] = move($1);
	}
	;

statements: /* empty */
	{ }
	| statements statement
	{
		$$ = move($1);
		if ($2.value("stmt-type", "") != "func-def")
			$$.emplace_back(move($2));
	}
	;

statement: import ';'
	{
		$$ = move($1);
		$$["stmt-type"] = "import";
	}
	| cinclude ';'
	{
		$$ = move($1);
		$$["stmt-type"] = "cinclude";

		json c_ast = execute_c2ast($$["path-type"], $$["path"]);
		if (c_ast.is_object() && c_ast.contains("ast")) {
			$$["functions"] = move(c_ast["ast"]["functions"]);
		}
	}
	| standalone_block
		{ $$ = move($1); }
	| var_declarations ';'
	{
		// Detect a tapple-decl emitted by var_declaration
		if ($1.size() == 1 && $1[0].value("stmt-type", "") == "tapple-decl") {
			$$ = move($1[0]);
		} else {
			bool all_ok = true;
			for (auto& v : $1) {
				if (v.count("not-impl") || v.value("stmt-type","") == "tapple-decl"
					|| v.value("inherit-type", false)) { all_ok = false; break; }
			}
			if (all_ok) {
				$$ = {{"stmt-type", "var-decl"}, {"vars", move($1)}};
			} else {
				$$ = {{"stmt-type", "not-impl"}};
			}
		}
	}
	| const_decl ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| type_decl ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| interface_decl ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| expression ';'
	{
		string et = $1.value("expr-type", "");
		if (et == "assign-expr") {
			if ($1["value"].value("expr-type", "") != "not-impl")
				$$ = {{"stmt-type", "assign"}, {"name", $1["name"]}, {"value", move($1["value"])}};
			else
				$$ = {{"stmt-type", "not-impl"}};
		} else if (et != "not-impl") {
			$$ = {{"stmt-type", "expr"}, {"body", move($1)}};
		} else {
			$$ = {{"stmt-type", "not-impl"}};
		}
	}
	| func_def
	{
		if (!$1.count("not-impl"))
			ast["ast"]["functions"].push_back(move($1));
		$$ = {{"stmt-type", "func-def"}};
	}
	| construct_def 
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| return ';'
	{
		$$ = {{"stmt-type", "return"}};
		if ($1.contains("values")) {
			bool all_ok = true;
			for (auto& v : $1["values"])
				if (v.value("expr-type", "") == "not-impl") { all_ok = false; break; }
			if (all_ok)
				$$["values"] = move($1["values"]);
			else
				$$ = {{"stmt-type", "not-impl"}};
		}
	}
	| for_loop
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| while_loop
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| if_stmt
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| term DBL_PLUS ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
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
		if ($2.size()) { $$["targets"] = move($2); }
		if ($5.size()) { $$["alias"] = $5; }
	}
	; 

import_ids: ID
	{ $$.emplace_back($1); }
	| import_ids ',' ID
	{
		$$ = move($1);
		$$.emplace_back($3);
	}
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

cinclude: KW_CINCLUDE import_path import_as
	{
		$$ = move($2);
		if ($3.size()) {
			$$["alias"] = $3;
		}
	}
	;

block: '{' statements '}'
	{ $$ = move($2); }
	;

block_statements: /* empty */
	{ }
	| block_statements statement_or_funcdef
	{
		$$ = move($1);
		$$.emplace_back(move($2));
	}
	;

statement_or_funcdef: statement    %dprec 1
	{ $$ = move($1); }
	| func_def                     %dprec 2
	{
		$$ = {{"stmt-type", "func-def"}};
		for (auto& [k, v] : $1.items())
			$$[k] = move(v);
	}
	;

block_body_items: /* empty */
	{ $$ = {{"functions", json::array()}, {"body", json::array()}}; }
	| block_body_items statement_or_funcdef
	{
		$$ = move($1);
		json item = move($2);
		if (item.value("stmt-type", "") == "func-def") {
			item.erase("stmt-type");
			$$["functions"].push_back(move(item));
		} else {
			$$["body"].push_back(move(item));
		}
	}
	;

block_obj: '{' block_body_items '}'
	{ $$ = move($2); }
	;

standalone_block: block_obj
	{ json blk = move($1); blk["stmt-type"] = "block"; $$ = move(blk); }
	;

var_declarations: var_declaration
	{ $$ = { $1 }; }
	| var_declarations ',' var_declaration   %dprec 1
	{ $$ = move($1); $$.push_back($3); }
	| var_declarations ',' inherit_var_decl  %dprec 2
	{
		$$ = move($1);
		json next = move($3);
		for (int i = (int)$$.size() - 1; i >= 0; i--) {
			if ($$[i].contains("var-type")) {
				next["var-type"] = $$[i]["var-type"];
				next.erase("inherit-type");
				break;
			}
		}
		$$.push_back(next);
	}
	;

inherit_var_decl: ID
	{ $$ = {{"name", $1}, {"inherit-type", true}}; }
	| ID '=' expression
	{ $$ = {{"name", $1}, {"inherit-type", true}, {"init", $3}}; }
	;

var_declaration: var_type move_owner_r var_prefix ID var_postfix
	{
		if (!$2 && $3.empty() && !$5)
			$$ = {{"name", $4}, {"var-type", {{"type-kind", "prim"}, {"type-name", $1}}}};
		else
			$$ = {{"not-impl", true}};
	}
	| var_type var_prefix ID var_postfix '=' expression
	{
		if (!$1.empty() && $2.empty() && !$4) {
			$$ = { {"name", $3},
			       {"var-type", {{"type-kind", "prim"}, {"type-name", $1}}},
			       {"init",     $6} };
		} else {
			$$ = {{"not-impl", true}};
		}
	}
	| var_prefix ID var_postfix '=' expression
	{ $$ = {{"not-impl", true}}; }
	| tapple_decl '=' expression
	{
		if ($1.is_array() && $3.value("expr-type","") == "call")
			$$ = {{"stmt-type", "tapple-decl"}, {"vars", $1}, {"value", $3}};
		else
			$$ = {{"not-impl", true}};
	}
	;

tapple_decl: '(' tapple_decl_inner ')'
	{ $$ = $2; }
	;

tapple_decl_inner: var_type ID var_postfix
	{ $$ = json::array();
	  $$.push_back({{"var-name", $2}, {"var-type", {{"type-kind","prim"},{"type-name",$1}}}}); }
	| KW_VOID
	{ $$ = json::array(); }
	| tapple_decl_inner ',' var_type ID var_postfix
	{ $$ = move($1);
	  $$.push_back({{"var-name", $4}, {"var-type", {{"type-kind","prim"},{"type-name",$3}}}}); }
	| tapple_decl_inner ',' KW_VOID
	{ $$ = move($1); }
	| tapple_decl_inner ',' ID var_postfix
	{ $$ = move($1);
	  if (!$4) {
	      json vtype;
	      for (int i = (int)$$.size() - 1; i >= 0; i--) {
	          if ($$[i].contains("var-type")) { vtype = $$[i]["var-type"]; break; }
	      }
	      if (!vtype.is_null())
	          $$.push_back({{"var-name", $3}, {"var-type", vtype}});
	  }
	}
	;

const_decl: KW_CONST ID '=' expression
	;

type_decl: do_export KW_TYPE ID implememts '{' type_members '}'
	| do_export KW_TYPE ID '=' var_type 
	| do_export KW_TYPE ID 
	;

implememts: /* empty */
	| ':' implements_types
	;

implements_types: implements_type
	| implements_types ',' implements_type
	;

implements_type: var_type
	| var_type KW_AS ID
	;

type_members: type_member
	| type_members type_member
	;

type_member: var_declaration ';'
	| KW_FUNC long_func_name '(' paramaters ')' return_def block
	;

long_func_name: ID
	| long_func_name '.' ID
	;

interface_decl: do_export KW_INTERFACE ID '{' interface_methods '}'
	| do_export KW_INTERFACE ID '<' temp_ids '>' '{' interface_methods '}'
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
	{ $$ = {{"expr-type", "not-impl"}}; }
	| dict_desc
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '+' expression
	{ $$ = {{"expr-type", "add"}, {"left", $1}, {"right", $3}}; }
	| expression '-' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '*' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '/' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '%' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '&' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '|' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression OPE_LE expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression OPE_GE expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '<' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression '>' expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| expression ARROW expression
	{
		if ($3.value("expr-type", "") == "id")
			$$ = {{"expr-type", "assign-expr"}, {"name", $3["name"]}, {"value", move($1)}};
		else
			$$ = {{"expr-type", "not-impl"}};
	}
	| expression DBL_ARROW expression
	{ $$ = {{"expr-type", "not-impl"}}; }
	| noname_func
	{ $$ = {{"expr-type", "not-impl"}}; }
	;

term: INT
	{ $$ = {{"expr-type", "lit-int"}, {"value", move($1)}}; }
	| UINT
	{ $$ = {{"expr-type", "lit-uint"}, {"value", move($1)}}; }
	| STRING
	{ $$ = {{"expr-type", "lit-str"}, {"value", move($1)}}; }
	| ID
	{ $$ = {{"expr-type", "id"}, {"name", move($1)}}; }
	| '(' tapple_inner ')'
	{ $$ = {{"expr-type", "not-impl"}}; }
	| term '.' ID
	{ $$ = {{"expr-type", "not-impl"}}; }
	| term array_desc
	{ $$ = {{"expr-type", "not-impl"}}; }
	;

tapple_inner: expression
	| '-'
	| tapple_inner ',' expression
	| tapple_inner ',' '-'
	;

func_call: term '(' arguments ')'
	{
		string name = $1.value("name", "");
		if (typeNames.count(name)) {
			if ($3.size() == 1) {
				$$ = {{"expr-type",   "cast"},
				      {"target-type", {{"type-kind", "prim"}, {"type-name", name}}},
				      {"src",         $3[0]}};
			} else {
				$$ = {{"expr-type", "not-impl"}};
			}
		} else if ($1.value("expr-type", "") == "id") {
			$$ = {{"expr-type", "call"}, {"name", name}, {"args", move($3)}};
		} else {
			$$ = {{"expr-type", "not-impl"}};
		}
	}
	;

arguments: /* empty */
	{ }
	| expression move_owner_r
	{ $$.push_back(move($1)); }
	| arguments ',' expression move_owner_r
	{
		$$ = move($1);
		$$.push_back(move($3));
	}
	;

array_desc: '[' array_items ']'
	| array_desc '[' array_items ']'
	;

array_items: expression
	| array_items ',' expression
	;

dict_desc: '{' dict_items '}'
	;

dict_items: ID ':' expression
	| dict_items ',' ID ':' expression
	;

func_def: do_export KW_FUNC ID '(' paramaters ')' return_def block_obj
	{
		bool all_ok = true;
		for (auto& p : $5)
			if (p.count("not-impl")) { all_ok = false; break; }
		if (all_ok) {
			$$ = {{"name", $3}, {"func-type", "palan"}, {"block", move($8)}, {"parameters", move($5)}};
			if ($7.contains("rets"))
				$$["rets"] = move($7["rets"]);
			else if ($7.contains("ret-type"))
				$$["ret-type"] = move($7["ret-type"]);
			if ($1) {
				$$["export"] = true;
				json sig = $$;
				sig.erase("block");
				ast["export"].push_back(move(sig));
			}
		} else {
			$$ = {{"not-impl", true}};
		}
	}

paramaters: /* empty */
	{ }
	| var_declarations
	{ $$ = move($1); }
	;

return_def: /* empty */
	{ }
	| ARROW var_declarations
	{
		bool all_ok = true;
		for (auto& v : $2)
			if (v.count("not-impl") || v.value("inherit-type", false)) { all_ok = false; break; }
		if (all_ok)
			$$["rets"] = move($2);
	}
	| ARROW var_type
	{
		if (!$2.empty())
			$$["ret-type"] = {{"type-kind", "prim"}, {"type-name", $2}};
	}
	;

noname_func: KW_FUNC '(' paramaters ')'
	return_def block
	;

construct_def: do_export KW_CONSTRUCT var_type '(' paramaters ')' block
	;

return: KW_RETURN
	{ }
	| KW_RETURN expressions
	{ $$["values"] = move($2); }
	;

for_loop: KW_FOR ID ':' expression block
	;

while_loop: KW_WHILE expression block
	;

if_stmt: KW_IF expression block else_stmt
	;

else_stmt: /* empty */
	| KW_ELSE block
	| KW_ELSE if_stmt
	;

expressions: expression
	{ $$.push_back(move($1)); }
	| expressions ',' expression
	{ $$ = move($1); $$.push_back(move($3)); }
	;

var_type: ID
	{ $$ = $1; }
	| ID '<' temp_ids '>'
	{ $$ = ""; }
	;

temp_ids: ID | temp_ids ',' ID
	;

vars: ID var_postfix
	| vars ',' ID var_postfix
	;

do_export: /* empty */ { $$ = false; }
	| KW_EXPORT        { $$ = true; }
	;

move_owner_r: /* empty */ { $$ = false; }
	| DBL_GRTR            { $$ = true; }
	;

var_prefix:	/* empty */ { $$ = ""; }
	| '@'      { $$ = "@"; }
	| '$'      { $$ = "$"; }
	| AT_EXCL  { $$ = "@!"; }
	;

var_postfix: /* empty */   { $$ = false; }
	| array_postfix        { $$ = true; }
	;

array_postfix: '[' array_type emitable_exps ']' var_prefix
	| array_postfix '[' array_type emitable_exps ']' var_prefix
	;

array_type: /* empty */ | '#' | '+'
	;

emitable_exps: emitable_exp
	| emitable_exps ',' emitable_exp
	;

emitable_exp: /* empty */ | expression
	;

%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cerr << l.begin.line << ":" << l.begin.column << "-" << l.end.line << ":" << l.end.column << m << endl;
}

