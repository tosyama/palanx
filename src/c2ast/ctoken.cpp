#include <vector>
#include <string>
#include <boost/assert.hpp>
using namespace std;
#include "ctoken.h"

CToken0::CToken0(CToken0Type type, int line_no, int pos, int len)
	: type(type), is_eol(false)
{
	this->line_no = line_no;
	this->pos = pos;
	this->len = len;
}

CToken::CToken(CTokenType type, int lexer_no, int token0_no)
	: type(type), lexer_no(lexer_no), token0_no(token0_no)
{
	if (type == TT_INCLUDE) {
		info.tokens = new vector<CToken*>();
	}
}

CToken::CToken(const CToken& token)
	: type(token.type), lexer_no(token.lexer_no), token0_no(token.token0_no)
{
	switch (type) {
		case TT_ID:
			info.id = new string(*token.info.id); break;
		case TT_KEYWORD:
			info.keyword = token.info.keyword; break;
		case TT_PUNCTUATOR:
			info.punc = token.info.punc; break;
		case TT_INT: case TT_LONG:
			info.intval = token.info.intval; break;
		case TT_UINT: case TT_ULONG:
			info.uintval = token.info.uintval; break;
		case TT_FLOAT:
			info.floval = token.info.floval; break;
		case TT_DOUBLE:
			info.dblval = token.info.dblval; break;
		case TT_LDOUBLE:
			info.ldblval = token.info.ldblval; break;
		case TT_STR:
			info.str = new string(*token.info.str); break;
	
		default: // TT_INCLUDE
			BOOST_ASSERT(false);
	}

}

CToken::CToken(const string& s, int lexer_no, int token0_no)
	: type(TT_STR), lexer_no(lexer_no), token0_no(token0_no)
{
	info.str = new string(s);
}

CToken::~CToken()
{
	if (type == TT_INCLUDE) {
		delete info.tokens;
	}
}
