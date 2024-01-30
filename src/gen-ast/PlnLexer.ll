%{
/// Palan Lexer
/// 
/// @file PlnLexer.ll
/// @copyright 2024 YAMAGUCHI Toshinobu

#include "PlnLexer.h"
#include <string>

using std::string;

#undef	YY_DECL
#define	YY_DECL int PlnLexer::yylex()

enum {
	INT = 1,
	UINT,
	ID,
	STRING
};

%}

%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
UDIGIT	[0-9]+"u"
ID	[a-zA-Z_][0-9a-zA-Z]*
DEMILITER	"{"|"}"|"("|")"|"["|"]"|","|";"|":"|"="|"+"|"-"|"*"|"/"|"%"|"<"|">"|"!"|"?"|"&"|"@"|"."|"$"
STRING	"\""(\\.|\\\n|[^\\\"])*"\""
COMMENT1	\/\/[^\n]*\n

%%

{DIGIT} {
	//	lval.build<int64_t> = std::stoll(yytext); 
		return INT;
	}
{ID} {
		string id = yytext;
	//	lval.build<string> = move(id);
		return ID;
	}
{STRING}	{ return STRING; }
{COMMENT1}	{ }
{DEMILITER} { return yytext[0]; }
[ \t]+ { }
\r\n|\r|\n	{ }
<<EOF>>	{ return 0; }

%%

