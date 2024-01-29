%{
/// Palan Lexer

#include "PlnLexer.h"
#include <string>

using std::string;

#undef	YY_DECL
#define	YY_DECL int PlnLexer::yylex()

enum {
	INT = 1,
	UINT,
	ID
};

%}

%option c++
%option yyclass="PlnLexer"
%option noyywrap

DIGIT	[0-9]+
UDIGIT	[0-9]+"u"
ID	[a-zA-Z_][0-9a-zA-Z]*
DEMILITER	"{"|"}"

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
{DEMILITER} { return yytext[0]; }
[ \t]+ { }
\r\n|\r|\n	{ }
<<EOF>>	{ return 0; }

%%


