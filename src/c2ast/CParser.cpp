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
		ast["struct-name"] = *tokens[index-1]->info.id;
		if (!CONSUME_PUNC('{')) {
			// struct with tag only (reference, not definition)
			result_index = index;
			return true;
		}
	} else {
		// anonymous struct
		EXPECT_PUNC('{');
	}

	vector<json> fields;
	do {
		// Qualifiers ("const"/"volatile"/"inline") are consumed by
		// declaration_specifiers() itself (in any order); don't pre-consume them
		// here, or its "const" capture never sees them.
		json flocal;
		if (declaration_specifiers(flocal, tokens, index)) {
			json base_vt = flocal.value("var-type", json{});
			json field = {{"var-type", base_vt}};
			if (!declarator(field, tokens, index, false)) {
				return false;
			}
			fields.push_back(field);
			while (CONSUME_PUNC(',')) {
				json field2 = {{"var-type", base_vt}};
				if (!declarator(field2, tokens, index, false)) {
					return false;
				}
				fields.push_back(field2);
			}
			EXPECT_PUNC(';');
		} else {
			break;
		}
	} while (true);

	EXPECT_PUNC('}');

	ast["fields"] = move(fields);
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
				json enum_value;
				if (!constant_expression(enum_value, tokens, index)) {
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

	// "inline"/"volatile"/"const" may appear in any order (e.g. "const volatile
	// int", "volatile const int"), so consume them in a loop rather than a fixed
	// sequence -- a fixed sequence would silently miss "const" in the other order.
	bool is_const = false;
	for (;;) {
		if (CONSUME_KW(TK_INLINE)) continue;
		if (CONSUME_KW(TK_VOLATILE)) continue;
		if (CONSUME_KW(TK_CONST)) { is_const = true; continue; }
		break;
	}

	// Single point applying `const` to the resolved var-type, so every branch
	// below (prim/typedef/struct/union/enum) reflects it the same way instead of
	// each carrying its own copy of "if (is_const) ...".
	auto set_vt = [&](json vt) {
		if (is_const) vt["const"] = true;
		ast["var-type"] = move(vt);
	};
	auto set_prim = [&](const char* name) {
		set_vt({{"type-kind", "prim"}, {"type-name", name}});
	};

	if (CONSUME(TT_ID)) {	// typedef name
		string name = *tokens[index-1]->info.id;
		auto it = typedefs_.find(name);
		if (it != typedefs_.end()) {
			json vt = it->second;
			vt["typedef-name"] = name;
			set_vt(move(vt));
		} else {
			set_vt({{"type-kind", "user"}, {"type-name", name}});
		}
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
			json vt = {{"type-kind", "strct"}};
			string tagName = ast.value("struct-name", "");
			if (!tagName.empty() && definedStructs_.count(tagName)) {
				vt["type-name"] = tagName;
			}
			set_vt(move(vt));
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_UNION)) {
		if (struct_union_definition(ast, tokens, index)) {
			set_vt({{"type-kind", "union"}});
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_ENUM)) {
		if (enum_definition(ast, tokens, index)) {
			set_vt({{"type-kind", "enum"}});
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_VOID)) { set_prim("void"); result_index = index; return true; }

	return false;
}

// C array-parameter decay: "T name[n]...[m]" as a parameter type means "pointer to
// T[m]..." -- only the outermost dimension decays to a pointer, any inner dimensions
// stay as the pointee's array type (e.g. "char buf[2][3]" -> pntr(arr(size=3, ...))).
static void decayArrayParam(json &param)
{
	json &vt = param["var-type"];
	if (vt.value("type-kind", "") == "arr") {
		vt = {{"type-kind", "pntr"}, {"base-type", vt["base-type"]}};
	}
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
		decayArrayParam(param);
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
				decayArrayParam(param2);
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

// Wraps decl["var-type"] with pending array dimensions (outermost-first in `dims`),
// nesting from the innermost (last-parsed) dimension outward so "T m[2][3]" becomes
// arr(size=2, base=arr(size=3, base=T)) -- matching C array-of-array semantics.
// embedded:true / specifier:"raw" mark this as inline storage (no separate heap
// allocation), matching Palan's native [n]$T field vocabulary (ASTSpec.md "arr").
static void wrapArrayDims(json &var_type, vector<json> &dims)
{
	for (int i = (int)dims.size() - 1; i >= 0; --i) {
		var_type = {{"type-kind", "arr"}, {"base-type", var_type},
			{"size-expr", dims[i]}, {"embedded", true}, {"specifier", "raw"}};
	}
	dims.clear();
}

bool CParser::declarator_tail(json &decl, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	vector<json> dims;   // pending array dimensions, outermost (leftmost) first

	while (true) {
		if (CONSUME_PUNC('[')) {
			json arr_size_value;
			constant_expression(arr_size_value, tokens, index);
			if (!CONSUME_PUNC(']')) {
				// debug_token(tokens[index]);
				return false;
			}
			dims.push_back(move(arr_size_value));

		} else if (CONSUME_PUNC('(')) {
			wrapArrayDims(decl["var-type"], dims);
			vector<json> params;
			if (!CONSUME_PUNC(')')) {
				parameter_list(params, tokens, index);
				EXPECT_PUNC(')');
			}
			decl["var-type"] = {{"type-kind", "func"}, {"ret-type", decl["var-type"]}, {"parameters", params}};

		} else {
			break;
		}
	}
	wrapArrayDims(decl["var-type"], dims);

	result_index = index;
	return true;
}

// Advances `index` from the token just after an already-consumed '(' to the position
// just after its matching ')', tracking nested parens (a parameter list inside the
// group, e.g. "(*signal(int))(int)", may itself contain '(' / ')'). Sets `close_index`
// to the token index of that matching ')' itself, which declarator() uses to confirm a
// grouped declarator's inner parse consumed exactly up to it (see declarator() below).
static bool skipToMatchingParen(const vector<CToken*> &tokens, int &index, int &close_index)
{
	int depth = 1;
	while (index < (int)tokens.size()) {
		CToken* token = tokens[index];
		if (token->type == TT_PUNCTUATOR) {
			if (token->info.punc == '(') {
				depth++;
			} else if (token->info.punc == ')') {
				depth--;
				if (depth == 0) {
					close_index = index;
					index++;
					return true;
				}
			}
		}
		index++;
	}
	return false;
}

// declarator() parses a declarator, building up decl:
//   decl["name"]     - declared identifier
//   decl["var-type"] - complete type (caller initializes with base type from declaration_specifiers)
//
// C declarator precedence: the postfix suffixes '[n]' and '(params)' bind tighter than
// the prefix '*', so prefix pointers at this level are applied to the base type first
// (innermost), suffixes then wrap outside them, and a parenthesized inner declarator
// (a new level) receives the type built so far as ITS base type -- e.g. "int (*a)[3]"
// wraps int in arr(3) first (the suffix, at this level), then the inner "*a" wraps
// that in pntr (one level in); "int *a[3]" has no group, so the prefix pntr wraps int
// directly and the suffix arr(3) wraps outside that, giving arr(3, pntr(int)).
//
// This mutates decl["var-type"] before knowing whether the parse will succeed (the
// prefix '*' loop below writes it immediately), so any failure must roll back to
// `saved` -- callers such as parameter_list() retry a failed declarator() call on the
// very same decl object (first as a named declarator, then as an abstract one), and
// would otherwise see a partially-wrapped type left over from the failed attempt.
bool CParser::declarator(json &decl, const vector<CToken*> &tokens, int &result_index, bool is_typeonly)
{
	int index = result_index;
	json saved = decl;

	while (CONSUME_PUNC('*')) {
		bool ptr_const = CONSUME_KW(TK_CONST);       // e.g. int * const p
		CONSUME_KW(TK_VOLATILE);
		decl["var-type"] = {{"type-kind", "pntr"}, {"base-type", move(decl["var-type"])}};
		if (ptr_const) decl["var-type"]["const"] = true;
	}

	int group_index = -1, close_index = -1;
	if (CONSUME_PUNC('(')) {
		group_index = index;
		if (!skipToMatchingParen(tokens, index, close_index)) {
			decl = move(saved);
			return false;
		}

	} else if (!is_typeonly) {
		CONSUME_KW(TK_RESTRICT); // C99 restrict qualifier
		if (!CONSUME(TT_ID)) {
			decl = move(saved);
			return false;
		}
		decl["name"] = *tokens[index-1]->info.id;
	}

	if (!declarator_tail(decl, tokens, index)) {
		decl = move(saved);
		return false;
	}

	if (group_index >= 0) {
		// Parse the group's interior as a nested declarator level, now that the type
		// it should wrap (this level's suffixes, applied above) is in decl["var-type"].
		// Requiring it to consume exactly up to the already-located close_index (rather
		// than just trusting its own success) rejects garbage inside the group, e.g.
		// "int (*x y)" -- and rejects a parameter list mistaken for a group, e.g.
		// "int(char*)", the same shapes the old EXPECT_PUNC(')')-right-after-recursing
		// check used to reject.
		int gi = group_index;
		if (!declarator(decl, tokens, gi, is_typeonly) || gi != close_index) {
			decl = move(saved);
			return false;
		}
	}

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
		int struct_union_save_index = index;
		bool is_struct_kw = CONSUME_KW(TK_STRUCT);
		bool is_union_kw = !is_struct_kw && CONSUME_KW(TK_UNION);
		if (is_struct_kw || is_union_kw) {
			if (struct_union_definition(ast, tokens, index) && CONSUME_PUNC(';')) {
				if (is_struct_kw && ast.contains("fields")) {
					string structName = ast.value("struct-name", "");
					if (!structName.empty()) {
						ast["ast"]["structs"].push_back({
							{"name", structName},
							{"fields", ast["fields"]}
						});
						definedStructs_.insert(structName);
					}
				}
				ast.erase("struct-name");
				ast.erase("fields");
				result_index = index;
				return true;
			}
			// Not a standalone struct/union declaration (e.g. "struct Tag func(...)")
			// — backtrack and let it fall through to be parsed as a type specifier.
			index = struct_union_save_index;
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
				} else if (is_typedef) {
					string tk = vt.value("type-kind", "");
					if (tk == "prim" || tk == "pntr") {
						typedefs_[decl["name"].get<string>()] = vt;
					} else if (tk == "user") {
						auto it = typedefs_.find(vt["type-name"].get<string>());
						if (it != typedefs_.end()) {
							typedefs_[decl["name"].get<string>()] = it->second;
						}
					}
					// strct/union/enum/func underlying types: not registered,
					// left as unresolved "user" at reference sites (unchanged behavior)
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
		json return_value;
		constant_expression(return_value, tokens, index);
		EXPECT_PUNC(';');
		result_index = index;
		return true;
	}

	return false;
}

// Each function in this expression chain (primary_expression .. expression) recognizes
// C expression grammar and, when the (sub)expression is one of a small set of computable
// forms (integer literal, unary +/-, parenthesization, explicit cast), also builds a
// value-AST node into `value` ("expr-type": "lit-int" | "cast", mirroring the convention
// documented in doc/ASTSpec.md). Anything else (arithmetic, calls, identifiers, ...) is
// still recognized syntactically (grammar TODOs elsewhere in this chain are unaffected),
// but `value` is left null to signal "not a compile-time constant we can evaluate".
bool CParser::primary_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: string literal, character constant, floating constant
	int index = result_index;

	if (CONSUME(TT_ID)) {
		value = json{};
		result_index = index;
		return true;
	}

	if (CONSUME(TT_PP_NUMBER)) {
		const string& text = *tokens[index-1]->info.str;
		try {
			size_t consumed;
			long long v = stoll(text, &consumed, 0);
			if (consumed == text.size()) {
				value = {{"expr-type", "lit-int"}, {"value", to_string(v)}};
			} else {
				value = json{}; // suffix like L/U, or a float literal: not a simple int
			}
		} catch (...) {
			value = json{};
		}
		result_index = index;
		return true;
	}

	if (CONSUME_PUNC('(')) {
		if (expression(value, tokens, index)) {
			EXPECT_PUNC(')');
			result_index = index;
			return true;
		}
	}

	return false;
}

bool CParser::postfix_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: array subscripting, structure and union member access, postfix increment and decrement
	int index = result_index;

	if (!primary_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC('(')) {
			json arg_value;
			while (assignment_expression(arg_value, tokens, index)) {
				if (!CONSUME_PUNC(',')) {
					break;
				}
			}

			EXPECT_PUNC(')');
			value = json{}; // function call result is not a compile-time constant
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

bool CParser::unary_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: prefix increment and decrement, unary &, unary *, bitwise NOT, logical NOT
	int index = result_index;

	if (postfix_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	// Unary + and - (e.g. enum initializers like MCHECK_DISABLED = -1)
	bool has_plus = CONSUME_PUNC('+');
	bool has_minus = !has_plus && CONSUME_PUNC('-');
	if (has_plus || has_minus) {
		json inner_value;
		if (!cast_expression(inner_value, tokens, index)) {
			return false;
		}
		if (has_minus) {
			if (inner_value.is_object() && inner_value.value("expr-type", "") == "lit-int") {
				long long v = stoll(inner_value["value"].get<string>());
				value = {{"expr-type", "lit-int"}, {"value", to_string(-v)}};
			} else {
				value = json{};
			}
		} else {
			value = inner_value; // unary plus: value unchanged
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
		value = json{}; // sizeof value not computed (no target type-size table)
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::cast_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;
	int save_index = index;
	vector<json> cast_types;

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

		cast_types.push_back(cdecl["var-type"]);
		save_index = index;
	}

	// for after cast(s) expression
	json inner_value;
	if (unary_expression(inner_value, tokens, index)) {
		for (auto it = cast_types.rbegin(); it != cast_types.rend(); ++it) {
			inner_value = {{"expr-type", "cast"}, {"target-type", *it}, {"src", inner_value}};
		}
		value = inner_value;
		result_index = index;
		return true;
	}

	// for not a cast expression
	if (index != result_index) {
		json retry_value;
		if (unary_expression(retry_value, tokens, result_index)) {
			value = retry_value;
			return true;
		}
	}

	return false;
}

bool CParser::multiplicative_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!cast_expression(value, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('*') || CONSUME_PUNC('/') || CONSUME_PUNC('%')) {
		json rhs_value;
		if (!multiplicative_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{}; // arithmetic result not evaluated
	}

	result_index = index;
	return true;
}

bool CParser::additive_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!multiplicative_expression(value, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('+') || CONSUME_PUNC('-')) {
		json rhs_value;
		if (!additive_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{}; // arithmetic result not evaluated
	}

	result_index = index;
	return true;
}

bool CParser::shift_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!additive_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('<<') || CONSUME_PUNC('>>')) {
		json rhs_value;
		if (!shift_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::relational_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!shift_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('<') || CONSUME_PUNC('>') || CONSUME_PUNC('<=') || CONSUME_PUNC('>=')) {
		json rhs_value;
		if (!relational_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::equality_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!relational_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('==') || CONSUME_PUNC('!=')) {
		json rhs_value;
		if (!equality_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::and_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!equality_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('&')) {
		json rhs_value;
		if (!and_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::exclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!and_expression(value, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('^')) {
		json rhs_value;
		if (!exclusive_or_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::inclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!exclusive_or_expression(value, tokens, index)) {
		return false;
	}

	if (CONSUME_PUNC('|')) {
		json rhs_value;
		if (!inclusive_or_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = json{};
	}

	result_index = index;
	return true;
}

bool CParser::logical_and_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical AND operator
	int index = result_index;

	if (inclusive_or_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::logical_or_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement logical OR operator
	int index = result_index;

	if (logical_and_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::conditional_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!logical_or_expression(value, tokens, index)) {
		return false;
	}
	if (CONSUME_PUNC('?')) {
		json then_value;
		if (!expression(then_value, tokens, index)) {
			return false;
		}
		if (!CONSUME_PUNC(':')) {
			return false;
		}
		json else_value;
		if (!conditional_expression(else_value, tokens, index)) {
			return false;
		}
		value = json{}; // ternary result not evaluated (condition not evaluated)
	}

	result_index = index;
	return true;
}

bool CParser::constant_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (conditional_expression(value, tokens, index)) {
		result_index = index;
		return true;
	}

	return false;
}

bool CParser::assignment_expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	// TODO: implement assignment operators
	return conditional_expression(value, tokens, result_index);
}

bool CParser::expression(json &value, const vector<CToken*> &tokens, int &result_index)
{
	int index = result_index;

	if (!assignment_expression(value, tokens, index)) {
		return false;
	}

	for (;;) {
		if (CONSUME_PUNC(',')) {
			// Comma operator: result is the last operand's value (matches C semantics),
			// which falls out naturally since each call below overwrites `value`.
			if (!assignment_expression(value, tokens, index)) {
				return false;
			}
		} else {
			break;
		}
	}

	result_index = index;
	return true;
}

// Interprets a value-AST node built by the expression chain above into a (value, type)
// pair, for the narrow set of forms useful as an exported constant: an integer literal,
// or a cast of one (e.g. NULL == ((void *)0)). Anything else (the node is null because
// the source expression wasn't a compile-time constant we track) is rejected.
bool CParser::resolveConstValue(const json &node, json &value, json &type)
{
	if (!node.is_object()) return false;

	string expr_type = node.value("expr-type", "");
	if (expr_type == "lit-int") {
		value = node["value"];
		type = {{"type-kind", "prim"}, {"type-name", "int32"}};
		return true;
	}
	if (expr_type == "cast") {
		json inner_type;
		if (!resolveConstValue(node["src"], value, inner_type)) return false;
		type = node["target-type"];
		return true;
	}

	return false;
}

void CParser::exportMacroConstants(json &ast, const vector<CMacro*> &macros, CPreprocessor &cpp)
{
	for (CMacro* m : macros) {
		if (m->type != MT_OBJ) continue;

		vector<CToken*> expanded = cpp.expandObjectMacroBody(m);

		json expr_value;
		int index = 0;
		bool ok = constant_expression(expr_value, expanded, index) && index == (int)expanded.size();

		json value, type;
		if (ok) ok = resolveConstValue(expr_value, value, type);

		for (CToken* t : expanded) delete t;
		if (!ok) continue;

		ast["ast"]["constants"].push_back({
			{"name", m->name},
			{"value", value},
			{"value-type", type}
		});
	}
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
