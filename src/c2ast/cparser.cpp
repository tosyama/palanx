#include <vector>
#include <string>
#include <iostream>
#include <boost/assert.hpp>

using namespace std;

#include "cfileinfo.h"
#include "ctoken.h"
#include "clexer.h"
#include "cpreprocessor.h"
#include "cparser.h"

void CParser::debug_token(const CToken* token)
{
	CLexer* lexer = lexers[token->lexer_no];
	CToken0& token0 = lexer->tokens[token->token0_no];
	cout << lexer->infile.fname << ":" << token0.line_no << ":" << token0.pos+1 << ": ";

	switch (token->type) {
		case TT_ID:
			cout << "ID ";
			cout << *(token->info.id) << endl;
			break;
		case TT_KEYWORD:
			cout << "KEYWORD ";
			switch (token->info.keyword) {
				case TK_TYPEDEF:
					cout << "TYPEDEF" << endl;
					break;
				case TK_STRUCT:
					cout << "STRUCT" << endl;
					break;
				default:
					cout << "UNKNOWN_KEYWORD" << token->info.keyword << endl;
					break;
			}
			break;
		case TT_PUNCTUATOR:
			{
				cout << "PUNCTUATOR ";
				char c0 = token->info.punc & 0xFF;
				char c1 = (token->info.punc >> 8) & 0xFF;
				if (c1) cout << c1;
				cout << c0 << endl;
				break;
			}
		case TT_INCLUDE:
		{
			cout << "INCLUDE " << endl; 
			break;
		}
		default:
			cout << "OTHER ";
			cout << token->type << endl;
			break;
	}


}

CParser::CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers)
	: top_tokens(top_tokens), lexers(lexers)
{
}

bool consume(CTokenType expected_type, const vector<CToken*> &tokens, int &index) {
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == expected_type) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME(t) consume(t, tokens, index)


bool consume_kw(CTokenKeyword expected_keyword, const vector<CToken*> &tokens, int &index) {
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_KEYWORD && token->info.keyword == expected_keyword) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME_KW(kw) consume_kw(kw, tokens, index)

bool consume_punc(int expected_punc, const vector<CToken*> &tokens, int &index) {
	if (index < tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_PUNCTUATOR && token->info.punc == expected_punc) {
			index++;
			return true;
		}
	}
	return false;
}
#define CONSUME_PUNC(punc) consume_punc(punc, tokens, index)

#define EXPECT_PUNC(punc) if (!CONSUME_PUNC(punc)) { return false; }


const bool defalut_char_is_signed = true; 

bool unsigned_char(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if(CONSUME_KW(TK_CHAR))	{
			return true;
		}
		index--; // backtrack

	} else if (!defalut_char_is_signed) {
		return CONSUME_KW(TK_CHAR);
	}
	return false;
}

bool signed_char(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if(CONSUME_KW(TK_CHAR))	{
			return true;
		}
		index--; // backtrack

	} else if (defalut_char_is_signed) {
		return CONSUME_KW(TK_CHAR);
	}
	return false;
}

bool unsigned_long_long(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			if (CONSUME_KW(TK_LONG)) {
				return true;
			}
			index--; // backtrack
		}
		index--; // backtrack

	} 

	return false;
}

bool signed_long_long(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			if (CONSUME_KW(TK_LONG)) {
				return true;
			}
			index--; // backtrack
		}
		index--; // backtrack

	} 

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_LONG)) {
			return true;
		}
		index--; // backtrack
	}

	return false;
}

bool unsigned_long(const vector<CToken*> &tokens, int &index)
{
	// do check for long long first
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}
	return false;
}

bool signed_long(const vector<CToken*> &tokens, int &index)
{
	// do check for long long first
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_LONG)) {
		CONSUME_KW(TK_INT); // optional
		return true;
	}
	return false;
}

bool unsigned_short(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_SHORT)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}
	return false;
}

bool signed_short(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_SHORT)) {
			CONSUME_KW(TK_INT); // optional
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_SHORT)) {
		CONSUME_KW(TK_INT); // optional
		return true;
	}
	return false;
}

bool unsigned_int(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_UNSIGNED)) {
		if (CONSUME_KW(TK_INT)) {
			return true;
		}
		index--; // backtrack
	}
	return false;
}

bool signed_int(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_INT)) {
			return true;
		}
		index--; // backtrack
	}

	if (CONSUME_KW(TK_INT)) {
		return true;
	}
	return false;
}

bool CParser::struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (CONSUME(TT_ID)) {
		// struct with tag
		if (!CONSUME_PUNC('{')) {
			// struct with tag only
			result_index = index;
			return true;
		}
	} else {
		// anonymous struct
		EXPECT_PUNC('{');
	}

	do {
		if (declaration_specifiers(ast, tokens, index)) {
			if (!declarator(ast, tokens, index, false)) {
				return false;
			}
			EXPECT_PUNC(';');
		} else {
			break;
		}
	} while (true);

	EXPECT_PUNC('}');

	result_index = index;
	return true;
}

bool CParser::declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (CONSUME(TT_ID)) {	// typedef name
		// TODO: Check defined type
		result_index = index;
		return true;
	}

	if (unsigned_char(tokens, index)) {
		result_index = index;
		return true;
	}

	if (signed_char(tokens, index)) {
		result_index = index;
		return true;
	}

	if (unsigned_long_long(tokens, index)) {
		result_index = index;
		return true;
	}

	if (signed_long_long(tokens, index)) {
		result_index = index;
		return true;
	}

	if (unsigned_long(tokens, index)) {
		result_index = index;
		return true;
	}

	if (signed_long(tokens, index)) {
		result_index = index;
		return true;
	}

	if (unsigned_short(tokens, index)) {
		result_index = index;
		return true;
	}

	if (signed_short(tokens, index)) {
		result_index = index;
		return true;
	}

	if (unsigned_int(tokens, index)) {
		result_index = index;
		return true;
	}

	if (signed_int(tokens, index)) {
		result_index = index;
		return true;
	}

	if (CONSUME_KW(TK_STRUCT)) { // struct definition
		if (struct_union_definition(ast, tokens, index)) {
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_UNION)) { // union definition
		if (struct_union_definition(ast, tokens, index)) {
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_VOID)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::parameter_list(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	bool is_const = CONSUME_KW(TK_CONST);
	
	if (declaration_specifiers(ast, tokens, index)) {
		if (!declarator(ast, tokens, index, false)) {
			if (!declarator(ast, tokens, index, true)) { // abstract declarator
				debug_token(tokens[index]);
				return false;
			}
		} 

		while (CONSUME_PUNC(',')) {
			is_const = CONSUME_KW(TK_CONST);
			if (declaration_specifiers(ast, tokens, index)) {
				if (!declarator(ast, tokens, index, false)) {
					if (!declarator(ast, tokens, index, true)) { // abstract declarator
						debug_token(tokens[index]);
						return false;
					}
				}
			} else if (CONSUME_PUNC('...')) {
				EXPECT_PUNC(')');
				index--;	// backtrack for ')'
			} else {
				debug_token(tokens[index]);
				return false;
			}
		}

		result_index = index;
		return true;
	}
	return false;
}


bool CParser::declarator_tail(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	while (true) {
		if (CONSUME_PUNC('[')) {
			constant_expression(ast, tokens, index);
			
			//EXPECT_PUNC(']');
			if (!CONSUME_PUNC(']')) {
				debug_token(tokens[index]);
				return false;
			}
		
		} else if (CONSUME_PUNC('(')) {
			if (CONSUME_PUNC(')')) {
				// empty parameter list
				result_index = index;
				return true;
			}
			// function declarator
			parameter_list(ast, tokens, index);
			EXPECT_PUNC(')');
		
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

bool CParser::declarator(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_typeonly)
{
	int index = result_index;

	if (CONSUME_PUNC('*')) {
		if (!declarator(ast, tokens, index, is_typeonly)) {
			return false;
		}
		result_index = index;
		return true;
	}
	
	if (CONSUME_PUNC('(')) {
		if (!declarator(ast, tokens, index, is_typeonly)) {
			return false;
		}
		EXPECT_PUNC(')');

	} else if (!is_typeonly) {
		if (CONSUME(TT_ID)) {
			CToken *token = tokens[index-1];
			if (*(token->info.id) == "restrict") { // C99 restrict content keyword
				// try again for identifier
				CONSUME(TT_ID);
			}
		} else {
			return false;
		}
	}

	declarator_tail(ast, tokens, index);

	result_index = index;
	return true;
}

bool CParser::declaration(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	bool is_extern = false;
	bool is_typedef = CONSUME_KW(TK_TYPEDEF);
	if (!is_typedef) {
		is_extern = CONSUME_KW(TK_EXTERN);
	}

	if (declaration_specifiers(ast, tokens, index)) {
		if (declarator(ast, tokens, index, false)) {
			// parsed declarator
			EXPECT_PUNC(';');
			result_index = index;
			return true;
		}
	}

	index = result_index; // backtrack

	// just declaration of struct
	if (CONSUME_KW(TK_STRUCT)) {
		if (struct_union_definition(ast, tokens, index)) {
			EXPECT_PUNC(';');
			result_index = index;
			return true;
		}
	}

	return false;
		
}

bool CParser::primary_expression(json &ast, const vector<CToken*> &tokens, int &index)
{
	if (CONSUME(TT_ID)) {
		return true;
	}

	if (CONSUME(TT_INT)) {
		return true;
	}

	return false;
}

bool CParser::postfix_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (primary_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::unary_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (postfix_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	if (CONSUME_KW(TK_SIZEOF)) {
		EXPECT_PUNC('(');
	
		if (!declaration_specifiers(ast, tokens, index)) {
			return false;
		}

		if (!declarator(ast, tokens, index, true)) {
			return false;
		}

		EXPECT_PUNC(')');
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::cast_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	if (unary_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::multiplicative_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!cast_expression(ast, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('*') || CONSUME_PUNC('/') || CONSUME_PUNC('%')) {
		if (!multiplicative_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::additive_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!multiplicative_expression(ast, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('+') || CONSUME_PUNC('-')) {
		if (!additive_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::shift_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (additive_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::relational_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (shift_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::equality_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (relational_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::and_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (equality_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::exclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (and_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::inclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (exclusive_or_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::logical_and_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (inclusive_or_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::logical_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (logical_and_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::conditional_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (logical_or_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::constant_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (conditional_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

// Starting point of parsing (top level & included file)
int CParser::parse(json &ast, const vector<CToken*> &tokens)
{
	int index = 0;
	int debug_count = 0;

	if (tokens.size() == 0)
		return 0;

	while(index < tokens.size()) {
		if (index >= tokens.size()) return 0;

		if (CONSUME(TT_INCLUDE)) {
			CToken* token = tokens[index - 1];
			if (parse(ast, *(token->info.tokens))) return 1;

		} else if (declaration(ast, tokens, index)) {
			// parsed declaration
			
		} else {
			cout << "Unhandled token: ";
			debug_token(tokens[index]);
			return 1;
		}
	}
	return 0;
}

// Entry point of parsing
int CParser::parse(json &ast)
{
	return parse(ast, top_tokens);
}
