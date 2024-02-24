/// Palan Lexer
/// 
/// @file PlnLexer.h
/// @copyright 2024 YAMAGUCHI Toshinobu

#ifndef FLEX_SCANNER
	#include <FlexLexer.h>
#endif

class PlnLexer : public yyFlexLexer {
public:
	int yylex(
		palan::PlnParser::value_type& yylval,
		palan::PlnParser::location_type& location
	);
};

