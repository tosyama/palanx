#ifndef FLEX_SCANNER
#include <FlexLexer.h>
#endif

class PlnLexer : public yyFlexLexer {
public:
	int yylex();
};

