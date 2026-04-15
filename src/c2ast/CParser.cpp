#include <vector>
#include <list>
#include <set>
#include <string>
#include <iostream>
#include <utility>
#include <boost/assert.hpp>

using namespace std;

#include "CFileInfo.h"
#include "CToken.h"
#include "CLexer.h"
#include "CPreprocessor.h"
#include "CParser.h"
#include "PlnC2AstMessage.h"

// void CParser::debug_token(const CToken* token)
// {
// 	CLexer* lexer = lexers[token->lexer_no];
// 	CToken0& token0 = lexer->tokens[token->token0_no];
// 	cout << lexer->infile.fname << ":" << token0.line_no << ":" << token0.pos+1 << ": ";
//
// 	switch (token->type) {
// 		case TT_ID:
// 			cout << "ID ";
// 			cout << *(token->info.id) << endl;
// 			break;
// 		case TT_KEYWORD:
// 			cout << "KEYWORD ";
// 			switch (token->info.keyword) {
// 				case TK_TYPEDEF:
// 					cout << "TYPEDEF" << endl;
// 					break;
// 				case TK_STRUCT:
// 					cout << "STRUCT" << endl;
// 					break;
// 				default:
// 					cout << "UNKNOWN_KEYWORD" << token->info.keyword << endl;
// 					break;
// 			}
// 			break;
// 		case TT_PUNCTUATOR:
// 			{
// 				cout << "PUNCTUATOR ";
// 				char c0 = token->info.punc & 0xFF;
// 				char c1 = (token->info.punc >> 8) & 0xFF;
// 				if (c1) cout << c1;
// 				cout << c0 << endl;
// 				break;
// 			}
// 		case TT_INCLUDE:
// 		{
// 			cout << "INCLUDE " << endl;
// 			break;
// 		}
// 		default:
// 			cout << "OTHER ";
// 			cout << token->type << endl;
// 			break;
// 	}
// }

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
				CONSUME_KW(TK_INT); // optional
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
				CONSUME_KW(TK_INT); // optional
				return true;
			}
			index--; // backtrack
		}
		index--; // backtrack

	} 

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_LONG)) {
			CONSUME_KW(TK_INT); // optional
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

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_UNSIGNED)) {
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
		return true; // unsigned long/short/char should be processed before this function
	}
	return false;
}

bool signed_int(const vector<CToken*> &tokens, int &index)
{
	if (CONSUME_KW(TK_SIGNED)) {
		if (CONSUME_KW(TK_INT)) {
			return true;
		}
		return true; // signed long/short/char should be processed before this function
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
		bool is_const = CONSUME_KW(TK_CONST);
		bool is_volatile = CONSUME_KW(TK_VOLATILE);
		json flocal;
		if (declaration_specifiers(flocal, tokens, index)) {
			json base_vt = flocal.value("var-type", json{});
			json field = {{"var-type", base_vt}};
			if (!declarator(field, tokens, index, false)) {
				return false;
			}
			while (CONSUME_PUNC(',')) {
				json field2 = {{"var-type", base_vt}};
				if (!declarator(field2, tokens, index, false)) {
					return false;
				}
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

bool CParser::enum_definition(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (CONSUME(TT_ID)) {
		// enum with tag
		if (!CONSUME_PUNC('{')) {
			// enum with tag only
			result_index = index;
			return true;
		}
	} else {
		// anonymous enum
		EXPECT_PUNC('{');
	}

	do {
		if (CONSUME(TT_ID)) {
			if (CONSUME_PUNC('=')) {
				if (!constant_expression(ast, tokens, index)) {
					return false;
				}
			}
		} else {
			break;
		}

		if (!CONSUME_PUNC(',')) {
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

	CONSUME_KW(TK_INLINE);
	CONSUME_KW(TK_VOLATILE);
	bool is_const = CONSUME_KW(TK_CONST);

	auto set_prim = [&](const char* name) {
		ast["var-type"] = {{"type-kind", "prim"}, {"type-name", name}};
		if (is_const) ast["var-type"]["const"] = true;
	};

	if (CONSUME(TT_ID)) {	// typedef name
		// TODO: Check defined type
		ast["var-type"] = {{"type-kind", "user"}, {"type-name", *tokens[index-1]->info.id}};
		result_index = index;
		return true;
	}

	if (unsigned_char(tokens, index)) { set_prim("uint8"); result_index = index; return true; }
	if (signed_char(tokens, index))   { set_prim("int8");  result_index = index; return true; }
	if (unsigned_long_long(tokens, index)) { set_prim("uint64"); result_index = index; return true; }
	if (signed_long_long(tokens, index))   { set_prim("int64");  result_index = index; return true; }

	if (CONSUME_KW(TK_LONG)) {
		if (CONSUME_KW(TK_DOUBLE)) { set_prim("flt128"); result_index = index; return true; }
		index--; // backtrack
	}

	if (unsigned_long(tokens, index))  { set_prim("uint64"); result_index = index; return true; }
	if (signed_long(tokens, index))    { set_prim("int64");  result_index = index; return true; }
	if (unsigned_short(tokens, index)) { set_prim("uint16"); result_index = index; return true; }
	if (signed_short(tokens, index))   { set_prim("int16");  result_index = index; return true; }
	if (unsigned_int(tokens, index))   { set_prim("uint32"); result_index = index; return true; }
	if (signed_int(tokens, index))     { set_prim("int32");  result_index = index; return true; }

	if (CONSUME_KW(TK_DOUBLE)) { set_prim("flo64"); result_index = index; return true; }
	if (CONSUME_KW(TK_FLOAT))  { set_prim("flo32"); result_index = index; return true; }

	if (CONSUME_KW(TK_STRUCT)) {
		if (struct_union_definition(ast, tokens, index)) {
			ast["var-type"] = {{"type-kind", "strct"}};
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_UNION)) {
		if (struct_union_definition(ast, tokens, index)) {
			ast["var-type"] = {{"type-kind", "union"}};
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_ENUM)) {
		if (enum_definition(ast, tokens, index)) {
			ast["var-type"] = {{"type-kind", "enum"}};
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_VOID)) { set_prim("void"); result_index = index; return true; }

	return false;
}

bool CParser::parameter_list(vector<json> &params, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	json local;
	if (declaration_specifiers(local, tokens, index)) {
		json param = {{"var-type", local.value("var-type", json{})}};
		if (!declarator(param, tokens, index, false)) {
			if (!declarator(param, tokens, index, true)) { // abstract declarator
				// debug_token(tokens[index]);
				return false;
			}
		}
		params.push_back(param);

		while (CONSUME_PUNC(',')) {
			json local2;
			if (declaration_specifiers(local2, tokens, index)) {
				json param2 = {{"var-type", local2.value("var-type", json{})}};
				if (!declarator(param2, tokens, index, false)) {
					if (!declarator(param2, tokens, index, true)) { // abstract declarator
						// debug_token(tokens[index]);
						return false;
					}
				}
				params.push_back(param2);
			} else if (CONSUME_PUNC('...')) {
				params.push_back({{"name", "..."}});
				EXPECT_PUNC(')');
				index--;	// backtrack for ')'
			} else {
				// debug_token(tokens[index]);
				return false;
			}
		}

		result_index = index;
		return true;
	}
	return false;
}

bool CParser::declarator_tail(json &decl, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	while (true) {
		if (CONSUME_PUNC('[')) {
			constant_expression(decl, tokens, index);
			if (!CONSUME_PUNC(']')) {
				// debug_token(tokens[index]);
				return false;
			}

		} else if (CONSUME_PUNC('(')) {
			vector<json> params;
			if (!CONSUME_PUNC(')')) {
				parameter_list(params, tokens, index);
				EXPECT_PUNC(')');
			}
			json& vt = decl["var-type"];
			if (vt.is_object() && vt.value("type-kind", "") == "pntr") {
				// (*fp)(params) → fp is a pointer to function
				// Move func inside the pntr, using pntr's base-type as return type.
				vt = {{"type-kind", "pntr"}, {"base-type",
					{{"type-kind", "func"}, {"ret-type", vt["base-type"]}, {"parameters", params}}}};
			} else {
				vt = {{"type-kind", "func"}, {"ret-type", vt}, {"parameters", params}};
			}

		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

// declarator() parses a declarator, building up decl:
//   decl["name"]     - declared identifier
//   decl["var-type"] - complete type (caller initializes with base type from declaration_specifiers;
//                      declarator wraps with pntr for *, declarator_tail wraps with func/array)
bool CParser::declarator(json &decl, const vector<CToken*> &tokens, int &result_index, bool is_typeonly)
{
	int index = result_index;

	if (CONSUME_PUNC('*')) {
		bool ptr_const = CONSUME_KW(TK_CONST);       // e.g. int * const p
		bool ptr_volatile = CONSUME_KW(TK_VOLATILE);
		if (!declarator(decl, tokens, index, is_typeonly))
			return false;
		json& vt = decl["var-type"];
		if (vt.is_object() && vt.value("type-kind", "") == "func") {
			// char *f(params) → f is a function returning char*
			// The * modifies the return type, not the function itself.
			vt["ret-type"] = {{"type-kind", "pntr"}, {"base-type", vt["ret-type"]}};
			if (ptr_const) vt["ret-type"]["const"] = true;
		} else {
			vt = {{"type-kind", "pntr"}, {"base-type", vt}};
			if (ptr_const) vt["const"] = true;
		}
		result_index = index;
		return true;
	}

	if (CONSUME_PUNC('(')) {
		if (!declarator(decl, tokens, index, is_typeonly))
			return false;
		EXPECT_PUNC(')');

	} else if (!is_typeonly) {
		CONSUME_KW(TK_RESTRICT); // C99 restrict qualifier
		if (!CONSUME(TT_ID))
			return false;
		decl["name"] = *tokens[index-1]->info.id;
	}

	declarator_tail(decl, tokens, index);

	result_index = index;
	return true;
}

bool CParser::declaration(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_top_level)
{
	int index = result_index;

	bool is_extern = false;
	bool is_static = false;
	bool is_typedef = CONSUME_KW(TK_TYPEDEF);

	if (!is_typedef) {
		is_extern = CONSUME_KW(TK_EXTERN);
		if (!is_extern) {
			is_static = CONSUME_KW(TK_STATIC);
		}
	}

	if (!(is_typedef || is_extern || is_static)) {
		// just declaration of struct or union
		if (CONSUME_KW(TK_STRUCT) || CONSUME_KW(TK_UNION)) {
			if (struct_union_definition(ast, tokens, index)) {
				EXPECT_PUNC(';');
				result_index = index;
				return true;
			}
		}

		// just declaration of enum
		if (CONSUME_KW(TK_ENUM)) {
			if (enum_definition(ast, tokens, index)) {
				EXPECT_PUNC(';');
				result_index = index;
				return true;
			}
		}
	}

	json local;
	if (declaration_specifiers(local, tokens, index)) {
		json base_vt = local.value("var-type", json{});
		json decl = {{"var-type", base_vt}};
		if (declarator(decl, tokens, index, false)) {
			// parsed declarator

			// consume additional comma-separated declarators (AST not emitted)
			// Function prototypes must not appear in comma-separated lists.
			if (CONSUME_PUNC(',')) {
				BOOST_ASSERT(decl["var-type"].value("type-kind", "") != "func");
				do {
					json decl2 = {{"var-type", base_vt}};
					bool ok = declarator(decl2, tokens, index, false);
					BOOST_ASSERT(ok);
					BOOST_ASSERT(decl2["var-type"].value("type-kind", "") != "func");
				} while (CONSUME_PUNC(','));
			}

			if (CONSUME_PUNC(';')) {
				// simple declaration
				auto& vt = decl["var-type"];
				BOOST_ASSERT(vt.is_object());
				if (!is_static && !is_typedef
						&& vt.value("type-kind", "") == "func") {
					BOOST_ASSERT(decl.contains("name"));
					BOOST_ASSERT(vt.contains("ret-type") && !vt["ret-type"].is_null());
					ast["ast"]["functions"].push_back({
						{"name", move(decl["name"])},
						{"func-type", "c"},
						{"ret-type", move(vt["ret-type"])},
						{"parameters", move(vt["parameters"])}
					});
				}
				result_index = index;
				return true;

			} else if (is_top_level && CONSUME_PUNC('{')) {
				// function definition
				for (;;) {
					if (declaration(ast, tokens, index, false))
						continue;

					if (statement(ast, tokens, index))
						continue;

					break;
				}
				
				EXPECT_PUNC('}');
				result_index = index;
				return true;
			}
			return false;
		}
	}

	return false;

}

bool CParser::statement(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (expression(ast, tokens, index)) {
		EXPECT_PUNC(';');
		result_index = index;
		return true;
	}
	if (jump_statement(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::jump_statement(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (CONSUME_KW(TK_RETURN)) {
		constant_expression(ast, tokens, index);
		EXPECT_PUNC(';');
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::primary_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: string literal, character constant, floating constant
	int index = result_index;

	if (CONSUME(TT_ID)) {
		result_index = index;
		return true;
	}

	if (CONSUME(TT_PP_NUMBER)) {
		result_index = index;
		return true;
	}

	if (CONSUME_PUNC('(')) {
		if (expression(ast, tokens, index)) {
			EXPECT_PUNC(')');
			result_index = index;
			return true;
		}
	}

	return false;
}

bool CParser::postfix_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: array subscripting, structure and union member access, postfix increment and decrement
	int index = result_index;

	if (!primary_expression(ast, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC('(')) {
			while (assignment_expression(ast, tokens, index)) {
				if (!CONSUME_PUNC(',')) {
					break;
				}
			}
			
			EXPECT_PUNC(')');
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

bool CParser::unary_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: prefix increment and decrement, unary &, unary *, bitwise NOT, logical NOT
	int index = result_index;

	if (postfix_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	// Unary + and - (e.g. enum initializers like MCHECK_DISABLED = -1)
	if (CONSUME_PUNC('+') || CONSUME_PUNC('-')) {
		if (!cast_expression(ast, tokens, index)) {
			return false;
		}
		result_index = index;
		return true;
	}

	if (CONSUME_KW(TK_SIZEOF)) {
		EXPECT_PUNC('(');

		json slocal;
		if (!declaration_specifiers(slocal, tokens, index)) {
			return false;
		}
		json sdecl = {{"var-type", slocal.value("var-type", json{})}};
		if (!declarator(sdecl, tokens, index, true)) {
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
	int save_index = index;

	for (;;) {
		if (!CONSUME_PUNC('(')) {
			break;
		}
		json clocal;
		if (!declaration_specifiers(clocal, tokens, index)) {
			index = save_index; // backtrack
			break;
		}
		json cdecl = {{"var-type", clocal.value("var-type", json{})}};
		if (!declarator(cdecl, tokens, index, true)) {
			index = save_index; // backtrack
			break;
		}

		if (!CONSUME_PUNC(')')) {
			index = save_index; // backtrack
			break;
		}

		save_index = index;
	}

	// for after cast(s) expression
	if (unary_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	// for not a cast expression
	if (index != result_index) {
		if (unary_expression(ast, tokens, result_index)) {
			return true;
		}
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

	if (!additive_expression(ast, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('<<') || CONSUME_PUNC('>>')) {
		if (!shift_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::relational_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement relational operators
	int index = result_index;

	if (shift_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::equality_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement equality operators
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

	if (!equality_expression(ast, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('&')) {
		if (!and_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::exclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!and_expression(ast, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('^')) {
		if (!exclusive_or_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::inclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!exclusive_or_expression(ast, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('|')) {
		if (!inclusive_or_expression(ast, tokens, index)) {
			return false;
		}
	}

	result_index = index;
	return true;
}

bool CParser::logical_and_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical AND operator
	int index = result_index;

	if (inclusive_or_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::logical_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical OR operator
	int index = result_index;

	if (logical_and_expression(ast, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::conditional_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement conditional operator
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

bool CParser::assignment_expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement assignment operators
	return conditional_expression(ast, tokens, result_index);
}

bool CParser::expression(json &ast, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!assignment_expression(ast, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC(',')) {
			if (!assignment_expression(ast, tokens, index)) {
				return false;
			}
		} else {
			break;
		}
	}

	result_index = index;
	return true;
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

		} else if (declaration(ast, tokens, index, true)) {
			// parsed declaration and function definition
			
		} else {
			CLexer* err_lexer = lexers[tokens[index]->lexer_no];
			CToken0& err_t0 = err_lexer->tokens[tokens[index]->token0_no];
			cerr << err_lexer->infile.fname << ":" << err_t0.line_no << ":" << err_t0.pos + 1
				<< ": error: " << PlnC2AstMessage::getMessage(E_UnhandledToken) << endl;
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
