/// Palan Parser
///
/// @file PlnParser.yy
/// @copyright 2024 YAMAGUCHI Toshinobu

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
#include "../../lib/json/single_include/nlohmann/json.hpp"

using std::vector;
using std::string;
using std::cout;
using std::cerr;
using std::endl;
using json = nlohmann::json;

class PlnLexer;
}

%code
{
	#include "PlnLexer.h"

	int yylex(
		palan::PlnParser::value_type* yylval,
		palan::PlnParser::location_type* location,
		PlnLexer& lexer)
	{
		return lexer.yylex(*yylval, *location);
	}
}

%locations
%define api.namespace	{palan}
%define api.value.type	variant
%define parse.error	verbose

%token <int64_t>	INT	"integer"
%token <uint64_t>	UINT	"unsigned integer"
%token <string>	STRING	"string"
%token <string>	ID	"identifier"
%token <string>	PATH	"path"
%token <string>	INCLUDE_FILE	"include file"
%token KW_EXPORT	"export"
%token KW_IMPORT	"import"
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
%type <json>	import import_path
%type <vector<string>>	import_ids
%type <string>	import_as

%left ARROW DBL_ARROW
%left '<' '>' OPE_LE OPE_GE
%left '+' '-'
%left '*' '/' '%' '&' '|'
%left '.'

%start module

%%
module: statements
	{
		ast["ast"]["statements"] = move($1);
	}
	;

statements: /* empty */
	{ }
	| statements statement
	{
		$$ = move($1);
		$$.emplace_back($2);
	}
	;

statement: import ';'
	{
		$$ = move($1);
		$$["stmt-type"] = "import";
	}
	| block
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| var_declarations ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
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
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| func_def
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| construct_def 
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
	}
	| return ';'
	{
		json temp = {{"stmt-type", "not-impl"}};
		$$ = move(temp);
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

block: '{' statements '}'
	;

var_declarations: var_declaration
	| var_declarations ',' var_declaration
	;

var_declaration: var_type move_owner_r var_prefix ID var_postfix
	| var_type var_prefix ID var_postfix '=' expression
	| var_prefix ID var_postfix '=' expression
	| tapple_decl '=' expression
	;

tapple_decl: '(' tapple_decl_inner ')'
	;

tapple_decl_inner: var_type vars
	| KW_VOID
	| tapple_decl_inner ',' var_type vars
	| tapple_decl_inner ',' KW_VOID
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
	| func_call
	| array_desc
	| dict_desc
	| expression '+' expression
	| expression '-' expression
	| expression '*' expression
	| expression '/' expression
	| expression '%' expression
	| expression '&' expression
	| expression '|' expression
	| expression OPE_LE expression
	| expression OPE_GE expression
	| expression '<' expression
	| expression '>' expression
	| expression ARROW expression
	| expression DBL_ARROW expression
	| noname_func
	;

term: INT | UINT | STRING | ID
	| '(' tapple_inner ')'
	| term '.' ID
	| term array_desc
	;

tapple_inner: expression 
	| '-'
	| tapple_inner ',' expression
	| tapple_inner ',' '-'
	;

func_call: term '(' arguments ')'
	;

arguments: /* empty */
	| expression move_owner_r
	| arguments ',' expression move_owner_r
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

func_def: do_export KW_FUNC ID '(' paramaters ')' return_def block
	;

paramaters: /* empty */ | var_declarations
	;

return_def: /* empty */
	| ARROW var_declarations
	| ARROW var_type
	;

noname_func: KW_FUNC '(' paramaters ')'
	return_def block
	;

construct_def: do_export KW_CONSTRUCT var_type '(' paramaters ')' block
	;

return: KW_RETURN
	| KW_RETURN expressions
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
	| expressions ',' expression
	;

var_type: ID
	| ID '<' temp_ids '>'
	;

temp_ids: ID | temp_ids ',' ID
	;

vars: ID var_postfix
	| vars ',' ID var_postfix
	;

do_export: /* empty */ | KW_EXPORT
	;

move_owner_r:	/* empty */ | DBL_GRTR
	;

var_prefix:	/* empty */ | '@' | '$' | AT_EXCL
	;

var_postfix: /* empty */ | array_postfix
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

