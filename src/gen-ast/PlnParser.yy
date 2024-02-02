/// Palan Parser
///
/// @file PlnParser.yy
/// @copyright 2024 YAMAGUCHI Toshinobu

%skeleton "lalr1.cc"
%require "3.0.2"
%defines
%define parser_class_name {PlnParser}
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
		palan::PlnParser::semantic_type* yylval,
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

%start module

%%
module: /* empty */
	| module statements
	;

statements: statement
	| statements statement
	;

statement: block | term
	;

block: '{' statements '}'

term: INT
	{
	}| UINT | STRING | ID
	;
%%

void palan::PlnParser::error(const location_type& l, const string& m)
{
	cout << "err:" << m << endl;
}
