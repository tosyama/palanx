%{
/// Palan Lexical analyzer.
/// 
/// PlnLexer.cpp will be created by flex.
///
/// @file PlnLexer.ll
/// @copyright 2024 YAMAGUCHI Toshinobu

#include "PlnParser.h"
#include "PlnLexer.h"
#include <string>

using std::string;
using namespace palan;

#undef	YY_DECL
#define	YY_DECL int PlnLexer::yylex(PlnParser::semantic_type& lval, PlnParser::location_type& loc)
#define YY_USER_ACTION	loc.columns(yyleng);

enum {
	INT =	PlnParser::token::INT,
	UINT =	PlnParser::token::UINT,
	ID =	PlnParser::token::ID,
	STRING =	PlnParser::token::STRING
};

static string& unescape(string& str);

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
%{
	loc.step();
%}

{DIGIT} {
		lval.build<int64_t>() = std::stoll(yytext); 
		return INT;
	}
{ID} {
		string id = yytext;
		lval.build<string>() = move(id);
		return ID;
	}
{STRING}	{
		string str(yytext+1, yyleng-2);
		lval.build<string>() = move(unescape(str));
		return STRING;
	}
{COMMENT1}	{ loc.lines(); loc.step(); }
{DEMILITER} { return yytext[0]; }
[ \t]+	{ loc.step(); }
\r\n|\r|\n	{ loc.lines(); loc.step(); }
<<EOF>>	{ return 0; }

%%

static int hexc(int c)
{
	if (c>='0' && c<='9') return c-'0';
	if (c>='a' && c<='f') return 10+c-'a';
	if (c>='A' && c<='F') return 10+c-'A';
	return -1;
}

static string& unescape(string& str)
{
	int sz = str.size();
	int d=0;
	for (int s=0; s<sz; ++s,++d) {
		if (str[s] != '\\') {
			str[d] = str[s];
			continue;
		}

		++s;

		switch(str[s]) {
			case 'a': str[d] = '\a'; break;
			case 'b': str[d] = '\b'; break;
			case 'n': str[d] = '\n'; break;
			case 'r': str[d] = '\r'; break;
			case 't': str[d] = '\t'; break;
			case 'v': str[d] = '\v'; break;
			case '0': str[d] = '\0'; break;

			case 'x': {
						  int h1 = hexc(str[s+1]), h2 = hexc(str[s+2]);
						  if (h1 >= 0 && h2 >= 0) {
							  str[d] = 16*h1 + h2; s+=2;
						  } else {
							  str[d] = 'x';
						  }
					  } break;

			default: str[d] = str[s];
		} /* switch */
	}
	str.resize(d);
	return str;
}

