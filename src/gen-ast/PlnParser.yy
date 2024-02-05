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
%token KW_IMPORT	"import"
%token KW_FROM	"from"
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
	| expression ';'
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

declaration: var_type ID
	| var_type ID '=' expression
	| ID '=' expression
	| tapple_decl '=' expression
	;

tapple_decl: '(' tapple_decl_inner ')'
	;

tapple_decl_inner: var_type IDs
	| tapple_decl_inner ',' var_type IDs
	;

expression: term
	| func_call
	| expression '+' expression
	| expression '-' expression
	| expression ARROW expression
	;

term: INT | UINT | STRING | ID
	| '(' tapple_inner ')'
	;

tapple_inner: expression 
	| ','
	| tapple_inner ',' expression
	| tapple_inner ','
	;

func_call: term '(' arguments ')'
	;

arguments: expression
	| arguments ',' expression
	;

var_type: ID
	;

IDs: ID
	| IDs ',' ID
	;

%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cout << "err:" << l.begin.line << "," << l.begin.column << m << endl;
}
