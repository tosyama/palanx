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
%token KW_FUNC	"func"
%token KW_TYPE	"type"
%token KW_CONST	"const"
%token KW_VOID	"void"
%token KW_RETURN	"return"
%token ARROW	"->"

%left ARROW
%left '+' '-'

%start module

%%
module: statements
	;

statements: /* empty */
	| statements statement
	;

statement: import ';'
	| block
	| declarations ';'
	| const_decl ';'
	| type_decl ';'
	| expression ';'
	| func_def
	| return ';'
	;

import: KW_IMPORT PATH
	| KW_IMPORT INCLUDE_FILE
	| KW_IMPORT import_ids KW_FROM PATH
	| KW_IMPORT import_ids KW_FROM INCLUDE_FILE
	; 

import_ids: ID | import_ids ',' ID
	;

block: '{' statements '}'
	;

declarations: declaration
	| declarations ',' declaration
	;

declaration: var_type var_prefix ID var_postfix
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

type_decl: do_export KW_TYPE ID '{' type_members '}'
	;

type_members: var_type var_prefix ID ';'
	| type_members var_type var_prefix ID ';'
	;

expression: term
	| func_call
	| array_desc
	| dict_desc
	| expression '+' expression
	| expression '-' expression
	| expression ARROW expression
	| noname_func
	;

term: INT | UINT | STRING | ID var_index
	| '(' tapple_inner ')'
	;

var_index: /* empty */
	| array_desc
	; 

tapple_inner: expression 
	| '-'
	| tapple_inner ',' expression
	| tapple_inner ',' '-'
	;

func_call: term '(' arguments ')'
	;

arguments: /* empty */
	| expression
	| arguments ',' expression
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

func_def:do_export KW_FUNC ID '(' paramaters ')'
	return_def block
	;

paramaters: /* empty */ | declarations
	;

return_def: /* empty */
	| ARROW declarations
	;

noname_func: KW_FUNC '(' paramaters ')'
	return_def block
	;

return: KW_RETURN
	| KW_RETURN expressions
	;

expressions: expression
	| expressions ',' expression
	;

var_type: ID
	;

vars: ID var_postfix
	| vars ',' ID var_postfix
	;

do_export: /* empty */ | KW_EXPORT
	;

var_prefix:	/* empty */ | '@' | '$'
	;

var_postfix: /* empty */ | array_postfix
	;

array_postfix: '[' emitable_exps ']' var_prefix
	| array_postfix '[' emitable_exps ']' var_prefix
	;

emitable_exps: emitable_exp
	| emitable_exps ',' emitable_exp
	;

emitable_exp: /* empty */ | expression
	;

%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cout << "err:" << l.begin.line << "," << l.begin.column << m << endl;
}
