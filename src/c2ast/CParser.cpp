#include <vector>
#include <list>
#include <set>
#include <string>
#include <iostream>
#include <utility>
#include <climits>
#include <cstdint>
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

// Single point where a struct/union tag registers into capturedStructs_, called
// from every place struct_union_definition() parses one (standalone declaration,
// type-specifier reference in a field/parameter/return type, and recursively
// for a nested struct-typed field) and, for an anonymous struct/union body, from
// declaration()'s tag synthesis (the typedef name stands in for the missing
// tag). `fields` is null for a tag-only reference or forward declaration; a
// later full definition of the same tag promotes an existing tag-only entry
// in place (preserving its first-seen position in the output) rather than
// adding a duplicate. A first-seen full definition is never overwritten by a
// later one (first definition wins) -- declaration() relies on this by
// checking structIndex_ itself before calling, so a synthesized name never
// collides with an unrelated struct already holding it.
void CParser::captureStructTag(const string &name, const json *fields, bool is_union)
{
	auto it = structIndex_.find(name);
	if (it == structIndex_.end()) {
		json entry = {{"name", name}};
		if (is_union) entry["union"] = true;
		if (fields) entry["fields"] = *fields;
		structIndex_[name] = (int)capturedStructs_.size();
		capturedStructs_.push_back(move(entry));
		return;
	}
	json &entry = capturedStructs_[it->second];
	if (fields && !entry.contains("fields"))
		entry["fields"] = *fields;
}

// Named after the body's source location rather than a per-run counter:
// each cinclude runs its own c2ast process and SA keeps the first complete
// definition of a name, so counters would collide across headers while a
// location names the same body identically in every run. Contains characters
// no C identifier can, so it never shadows a real tag.
string CParser::synthesizeAnonTag(const CToken* at)
{
	CLexer* lexer = lexers[at->lexer_no];
	const CToken0& t0 = lexer->tokens[at->token0_no];
	string base = "anon@" + lexer->infile.fname + ":" + to_string(t0.line_no)
		+ ":" + to_string(t0.pos + 1);
	// Tokens from one macro body share a location across expansions.
	string tag = base;
	for (int n = 2; structIndex_.count(tag); n++)
		tag = base + "#" + to_string(n);
	return tag;
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

// Tries each punctuator in turn and consumes the first one that matches, returning
// its code (or 0, which no punctuator encodes, if none matched). Used by the binary
// expression levels below to consume one operator per left-associative loop iteration.
int consume_punc_any(std::initializer_list<int> puncs, const vector<CToken*> &tokens, int &index) {
	for (int p : puncs) {
		if (consume_punc(p, tokens, index)) return p;
	}
	return 0;
}
#define CONSUME_PUNC_ANY(...) consume_punc_any({__VA_ARGS__}, tokens, index)


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

bool CParser::struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_struct)
{
	int index = result_index;

	if (CONSUME(TT_ID)) {
		// struct with tag
		ast["struct-name"] = *tokens[index-1]->info.id;
		if (!CONSUME_PUNC('{')) {
			// struct with tag only (reference, not definition)
			captureStructTag(ast["struct-name"].get<string>(), nullptr, !is_struct);
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
		int field_start = index;
		json flocal;
		if (declaration_specifiers(flocal, tokens, index)) {
			json base_vt = flocal.value("var-type", json{});
			// "struct { ... } name;" member: without a tag the owner's layout can't
			// reference the body. Synthesized on base_vt, so pointer/array
			// declarators wrap an already-named type.
			string btk = base_vt.value("type-kind", "");
			if ((btk == "strct" || btk == "union") && !base_vt.contains("type-name")) {
				string tag = synthesizeAnonTag(tokens[field_start]);
				captureStructTag(tag, &flocal["fields"], btk == "union");
				base_vt["type-name"] = tag;
			}
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
	string tagName = ast.value("struct-name", "");
	if (!tagName.empty())
		captureStructTag(tagName, &ast["fields"], !is_struct);
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
		if (struct_union_definition(ast, tokens, index, true)) {
			json vt = {{"type-kind", "strct"}};
			string tagName = ast.value("struct-name", "");
			if (!tagName.empty()) {
				vt["type-name"] = tagName;
			}
			set_vt(move(vt));
			result_index = index;
			return true;
		}
		return false;
	}

	if (CONSUME_KW(TK_UNION)) {
		if (struct_union_definition(ast, tokens, index, false)) {
			json vt = {{"type-kind", "union"}};
			string tagName = ast.value("struct-name", "");
			if (!tagName.empty()) {
				vt["type-name"] = tagName;
			}
			set_vt(move(vt));
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

		// C's `(void)` prototype means "no parameters" -- normalize it to the
		// same empty list a `f()` prototype and a Palan 0-arg function already
		// carry (ASTSpec.md: "empty array when no parameters"), so no
		// consumer needs to know about this pseudo-parameter shape. Applies
		// equally to a function-pointer's own inner `(void)` parameter list,
		// since that goes through this same function.
		if (params.size() == 1 && !params[0].contains("name")
				&& params[0]["var-type"].value("type-kind", "") == "prim"
				&& params[0]["var-type"].value("type-name", "") == "void")
			params.clear();

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
			// Capturing into capturedStructs_ happens inside struct_union_definition()
			// itself (the single point every struct tag reference goes through);
			// nothing to do here beyond clearing the scratch keys it wrote into ast.
			if (struct_union_definition(ast, tokens, index, is_struct_kw) && CONSUME_PUNC(';')) {
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
		vector<json> decls;
		decls.push_back({{"var-type", base_vt}});
		if (declarator(decls.back(), tokens, index, false)) {
			// parsed first declarator

			// Comma-separated additional declarators ("extern int a, b;",
			// "typedef int A, B;") are classified the same way as the first,
			// below -- all go through emitDeclarator() rather than only the
			// first being visible.
			while (CONSUME_PUNC(',')) {
				decls.push_back({{"var-type", base_vt}});
				bool ok = declarator(decls.back(), tokens, index, false);
				BOOST_ASSERT(ok);
			}

			if (CONSUME_PUNC(';')) {
				// simple declaration(s)

				// "typedef struct/union { ... } Name;" -- an anonymous body given a
				// name only through the typedef, not a tag. Synthesize that name as
				// the tag so the body enters capturedStructs_/typedefs_ through the
				// same single channel a tagged one would, rather than staying
				// unresolved as "user" at every reference site. Guarded to a single,
				// non-derived declarator (decls.size()==1, var-type still bare "strct"/"union"
				// after declarator() -- a pointer/array declarator would have wrapped
				// it in "pntr") whose var-type has no type-name yet (tagged structs and
				// typedef-name lookups of an already-registered strct always carry one).
				// Skipped outright if the name is already taken by a real C tag
				// (forward-declared or defined): Palan's struct type names are a single
				// namespace (unlike C's separate tag/typedef namespaces), so a name
				// already bound to one struct cannot also be re-bound to this unrelated
				// anonymous one -- doing so would let captureStructTag's promote-in-place
				// behavior attach this body's fields to that other struct's entry.
				if (is_typedef && decls.size() == 1 && local.contains("fields")) {
					json& vt0 = decls[0]["var-type"];
					string tk0 = vt0.value("type-kind", "");
					if ((tk0 == "strct" || tk0 == "union") && !vt0.contains("type-name")) {
						const string& name = decls[0]["name"].get<string>();
						if (structIndex_.find(name) == structIndex_.end()) {
							captureStructTag(name, &local["fields"], tk0 == "union");
							vt0["type-name"] = name;
						}
					}
				}

				for (auto &d : decls) {
					emitDeclarator(ast, d, is_typedef, is_static, is_extern, is_top_level);
				}
				result_index = index;
				return true;

			} else if (is_top_level && decls.size() == 1 && CONSUME_PUNC('{')) {
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

// Classifies one completed declarator (the first in a declaration, or any
// comma-separated successor) and emits it into the appropriate AST channel,
// or registers it into typedefs_, or discards it. This is the single point
// every declarator passes through, so multi-declarator lists are classified
// uniformly instead of only the first declarator being visible to callers.
void CParser::emitDeclarator(json &ast, json &decl,
		bool is_typedef, bool is_static, bool is_extern, bool is_top_level)
{
	auto& vt = decl["var-type"];
	BOOST_ASSERT(vt.is_object());
	string tk = vt.value("type-kind", "");

	if (!is_static && !is_typedef && tk == "func") {
		BOOST_ASSERT(decl.contains("name"));
		BOOST_ASSERT(vt.contains("ret-type") && !vt["ret-type"].is_null());
		ast["ast"]["functions"].push_back({
			{"name", move(decl["name"])},
			{"func-type", "c"},
			{"ret-type", move(vt["ret-type"])},
			{"parameters", move(vt["parameters"])}
		});
	} else if (is_typedef) {
		if (tk == "prim" || tk == "pntr") {
			registerTypedef(decl["name"].get<string>(), vt);
		} else if ((tk == "strct" || tk == "union") && vt.contains("type-name")) {
			// typedef struct/union Tag X; -- register X as an alias for the tag
			// itself (SA resolves it the same way it resolves any other
			// struct-bottomed type alias). This also covers a single,
			// non-derived typedef of an anonymous body (typedef struct/union {...}
			// X;): declaration() has already synthesized X itself as the tag
			// (see its call site) and written it into vt's "type-name" before
			// this function runs, so that case and the tagged one are
			// indistinguishable here.
			registerTypedef(decl["name"].get<string>(), vt);
		} else if (tk == "user") {
			auto it = typedefs_.find(vt["type-name"].get<string>());
			if (it != typedefs_.end()) {
				registerTypedef(decl["name"].get<string>(), it->second);
			}
		}
		// Remaining anonymous strct/union/enum/func underlying types -- a
		// multi/derived-declarator anonymous struct/union body, any enum body,
		// or a name declaration()'s tag synthesis skipped because it was
		// already taken by an unrelated struct -- are not registered, left as
		// unresolved "user" at reference sites (unchanged behavior).
	} else if (is_extern && is_top_level && (tk == "prim" || tk == "pntr")) {
		// A file-scope "extern" object declaration with external linkage, of a
		// type Palan can represent without heap/embedded-array semantics
		// (e.g. "extern FILE *stdout;"). Array and by-value struct/union/enum
		// globals are left unregistered -- same non-goal as elsewhere in this
		// iteration.
		ast["ast"]["globals"].push_back({
			{"name", move(decl["name"])},
			{"var-type", move(vt)}
		});
	}
	// else: static function, non-func/non-typedef/non-extern-object
	// declaration, or a shape outside what the globals channel represents --
	// discarded, matching pre-existing behavior for everything but
	// "func"/typedef.
}

void CParser::registerTypedef(const string &name, json vt)
{
	vt.erase("typedef-name");
	typedefs_[name] = move(vt);
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

// A value-AST "lit-int" follows the Palan AST convention for "value-type": present only
// when C fixed the type (a suffixed literal, an integer cast, or an operation with such an
// operand). An untyped node is a plain unsuffixed literal and folds with exact int64
// arithmetic. The value string is always within the range of its value-type.

static bool isLitInt(const json &node)
{
	return node.is_object() && node.value("expr-type", "") == "lit-int";
}

static bool intTypeInfo(const json &type, int &width, bool &is_signed)
{
	if (!type.is_object() || type.value("type-kind", "") != "prim") return false;
	static const map<string, pair<int, bool>> infos = {
		{"int8", {8, true}}, {"int16", {16, true}}, {"int32", {32, true}}, {"int64", {64, true}},
		{"uint8", {8, false}}, {"uint16", {16, false}}, {"uint32", {32, false}}, {"uint64", {64, false}},
	};
	auto it = infos.find(type.value("type-name", ""));
	if (it == infos.end()) return false;
	width = it->second.first;
	is_signed = it->second.second;
	return true;
}

static json intPrimType(int width, bool is_signed)
{
	return {{"type-kind", "prim"}, {"type-name", (is_signed ? "int" : "uint") + to_string(width)}};
}

// Two's complement bit pattern of a lit-int's value.
static unsigned long long litIntBits(const json &node)
{
	const string &s = node["value"].get_ref<const string&>();
	return s[0] == '-' ? (unsigned long long)stoll(s) : stoull(s);
}

// Builds a lit-int of `type` from `bits`, wrapping to the type's width the way GCC
// converts between integer types.
static json makeTypedLitInt(unsigned long long bits, const json &type)
{
	int width;
	bool is_signed;
	intTypeInfo(type, width, is_signed);
	if (width < 64) {
		unsigned long long mask = (1ULL << width) - 1;
		bits &= mask;
		if (is_signed && (bits >> (width - 1)))
			bits |= ~mask;
	}
	string value = is_signed ? to_string((long long)bits) : to_string(bits);
	return {{"expr-type", "lit-int"}, {"value", value}, {"value-type", type}};
}

// C11 6.4.4.1 integer constant typing (LP64). An unsuffixed literal stays untyped.
static json parseIntLiteral(const string &text)
{
	size_t consumed;
	unsigned long long v;
	try {
		v = stoull(text, &consumed, 0);
	} catch (...) {
		return json{};
	}

	string suffix;
	for (size_t i = consumed; i < text.size(); i++) suffix += tolower(text[i]);
	bool has_u = false;
	int l_count = 0;
	if (!suffix.empty() && suffix.front() == 'u') { has_u = true; suffix.erase(0, 1); }
	if (!suffix.empty() && suffix.back() == 'u' && !has_u) { has_u = true; suffix.pop_back(); }
	if (suffix == "l") l_count = 1;
	else if (suffix == "ll") l_count = 2;
	else if (!suffix.empty()) return json{}; // float literal or invalid suffix

	bool is_decimal = text.size() == 1 || text[0] != '0';
	if (!has_u && !l_count) {
		if (v > LLONG_MAX) return json{};
		return {{"expr-type", "lit-int"}, {"value", to_string(v)}};
	}
	if (has_u) {
		bool fits32 = !l_count && v <= UINT32_MAX;
		return makeTypedLitInt(v, intPrimType(fits32 ? 32 : 64, false));
	}
	if (v > LLONG_MAX && is_decimal) return json{};
	return makeTypedLitInt(v, intPrimType(64, v <= LLONG_MAX));
}

// Width and signedness a lit-int operand takes in C arithmetic, after integer promotion.
// An untyped operand is taken as int, or long when its value doesn't fit int.
static void promotedIntType(const json &node, int &width, bool &is_signed)
{
	if (node.contains("value-type")) {
		intTypeInfo(node["value-type"], width, is_signed);
		if (width < 32) { width = 32; is_signed = true; }
		return;
	}
	long long v = stoll(node["value"].get<string>());
	width = (v >= INT32_MIN && v <= INT32_MAX) ? 32 : 64;
	is_signed = true;
}

static bool fitsSigned(long long v, int width)
{
	return width == 64 || (v >= INT32_MIN && v <= INT32_MAX);
}

static json foldUntypedBinaryInt(long long l, int op, long long r)
{
	long long v;
	switch (op) {
	case '|': v = l | r; break;
	case '&': v = l & r; break;
	case '^': v = l ^ r; break;
	case '+': if (__builtin_add_overflow(l, r, &v)) return json{}; break;
	case '-': if (__builtin_sub_overflow(l, r, &v)) return json{}; break;
	case '*': if (__builtin_mul_overflow(l, r, &v)) return json{}; break;
	case '/':
		if (r == 0 || (l == LLONG_MIN && r == -1)) return json{};
		v = l / r;
		break;
	case '%':
		if (r == 0 || (l == LLONG_MIN && r == -1)) return json{};
		v = l % r;
		break;
	case '<<':
		if (l < 0 || r < 0 || r >= 63 || l > (LLONG_MAX >> r)) return json{};
		v = l << r;
		break;
	case '>>':
		if (l < 0 || r < 0 || r >= 64) return json{};
		v = l >> r;
		break;
	default:
		return json{};
	}
	return {{"expr-type", "lit-int"}, {"value", to_string(v)}};
}

// Shift: the result takes the promoted left operand's type (not the usual arithmetic
// conversions), and a count outside [0, width) is undefined behavior.
static json foldTypedShift(const json &lhs, int op, const json &rhs)
{
	int width, r_width;
	bool is_signed, r_signed;
	promotedIntType(lhs, width, is_signed);
	promotedIntType(rhs, r_width, r_signed);
	unsigned long long l = litIntBits(lhs), r = litIntBits(rhs);
	if ((r_signed && (long long)r < 0) || r >= (unsigned long long)width) return json{};

	json type = intPrimType(width, is_signed);
	if (is_signed) {
		long long sl = (long long)l;
		long long max = width == 64 ? LLONG_MAX : INT32_MAX;
		if (sl < 0 || (op == '<<' && sl > (max >> r))) return json{};
	}
	return makeTypedLitInt(op == '<<' ? l << r : l >> r, type);
}

// Folds with C's usual arithmetic conversions: unsigned results wrap to their width,
// signed results that overflow their width don't fold.
static json foldTypedBinaryInt(const json &lhs, int op, const json &rhs)
{
	if (op == '<<' || op == '>>') return foldTypedShift(lhs, op, rhs);

	int l_width, r_width;
	bool l_signed, r_signed;
	promotedIntType(lhs, l_width, l_signed);
	promotedIntType(rhs, r_width, r_signed);
	int width = max(l_width, r_width);
	bool is_signed = !((l_width == width && !l_signed) || (r_width == width && !r_signed));
	json type = intPrimType(width, is_signed);
	unsigned long long l = litIntBits(lhs), r = litIntBits(rhs);

	if (!is_signed) {
		unsigned long long mask = width == 64 ? ~0ULL : (1ULL << width) - 1;
		l &= mask;
		r &= mask;
		unsigned long long v;
		switch (op) {
		case '|': v = l | r; break;
		case '&': v = l & r; break;
		case '^': v = l ^ r; break;
		case '+': v = l + r; break;
		case '-': v = l - r; break;
		case '*': v = l * r; break;
		case '/': if (r == 0) return json{}; v = l / r; break;
		case '%': if (r == 0) return json{}; v = l % r; break;
		default: return json{};
		}
		return makeTypedLitInt(v, type);
	}

	json folded = foldUntypedBinaryInt((long long)l, op, (long long)r);
	if (!isLitInt(folded)) return json{};
	long long v = stoll(folded["value"].get<string>());
	if (!fitsSigned(v, width)) return json{};
	return makeTypedLitInt((unsigned long long)v, type);
}

// Folds one binary C operation on two value-AST nodes into a new "lit-int" node.
// Returns null -- the expression chain's single "not evaluable" signal -- when either
// operand is null (not a lit-int), when the operator isn't one of the folded arithmetic
// or bitwise operators (relational/equality are recognized syntactically by their
// caller but intentionally not folded), or when evaluating would be undefined behavior
// in this arithmetic itself (div/mod by zero, out-of-range shift count, overflow).
// Folding these to null rather than a wrong value keeps them in the same "not a
// compile-time constant we track" vocabulary as an unresolved identifier.
static json foldBinaryInt(const json &lhs, int op, const json &rhs)
{
	if (!isLitInt(lhs) || !isLitInt(rhs)) return json{};
	if (lhs.contains("value-type") || rhs.contains("value-type"))
		return foldTypedBinaryInt(lhs, op, rhs);
	return foldUntypedBinaryInt(stoll(lhs["value"].get<string>()), op,
		stoll(rhs["value"].get<string>()));
}

static json foldNegate(const json &node)
{
	if (!isLitInt(node)) return json{};
	if (!node.contains("value-type")) {
		long long v = stoll(node["value"].get<string>());
		if (v == LLONG_MIN) return json{};
		return {{"expr-type", "lit-int"}, {"value", to_string(-v)}};
	}
	int width;
	bool is_signed;
	promotedIntType(node, width, is_signed);
	unsigned long long bits = litIntBits(node);
	if (is_signed) {
		long long v = (long long)bits;
		if (v == LLONG_MIN || !fitsSigned(-v, width)) return json{};
	}
	return makeTypedLitInt(0 - bits, intPrimType(width, is_signed));
}

// Each function in this expression chain (primary_expression .. expression) recognizes
// C expression grammar and, when the (sub)expression is one of a small set of computable
// forms (integer literal, unary +/-, parenthesization, explicit cast, or a folded binary
// arithmetic/bitwise operation), also builds a value-AST node into `value`
// ("expr-type": "lit-int" | "cast", mirroring the convention documented in
// doc/ASTSpec.md). Anything else (calls, identifiers, sizeof, ...) is still recognized
// syntactically (grammar TODOs elsewhere in this chain are unaffected), but `value` is
// left null to signal "not a compile-time constant we can evaluate".
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
		value = parseIntLiteral(*tokens[index-1]->info.str);
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
			value = foldNegate(inner_value);
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
			int width;
			bool is_signed;
			if (isLitInt(inner_value) && intTypeInfo(*it, width, is_signed))
				inner_value = makeTypedLitInt(litIntBits(inner_value), *it);
			else
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('*', '/', '%');
		if (!op) break;

		json rhs_value;
		if (!cast_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('+', '-');
		if (!op) break;

		json rhs_value;
		if (!multiplicative_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('<<', '>>');
		if (!op) break;

		json rhs_value;
		if (!additive_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('<', '>', '<=', '>=');
		if (!op) break;

		json rhs_value;
		if (!shift_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('==', '!=');
		if (!op) break;

		json rhs_value;
		if (!relational_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('&');
		if (!op) break;

		json rhs_value;
		if (!equality_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('^');
		if (!op) break;

		json rhs_value;
		if (!and_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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

	for (;;) {
		int op = CONSUME_PUNC_ANY('|');
		if (!op) break;

		json rhs_value;
		if (!exclusive_or_expression(rhs_value, tokens, index)) {
			return false;
		}
		value = foldBinaryInt(value, op, rhs_value);
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
		if (node.contains("value-type")) {
			type = node["value-type"];
			return true;
		}
		long long v = stoll(node["value"].get<string>());
		const char* type_name = (v >= INT32_MIN && v <= INT32_MAX) ? "int32" : "int64";
		type = {{"type-kind", "prim"}, {"type-name", type_name}};
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
	int ret = parse(ast, top_tokens);
	if (ret == 0) {
		if (!capturedStructs_.empty())
			ast["ast"]["structs"] = capturedStructs_;

		json typedefs = json::array();
		for (const auto &[name, vt] : typedefs_) {
			// Pointer-bottomed typedefs stay unregistered this version: naming
			// one would force a decision about which side of the @/@! mutability
			// split a bare pointer alias falls on.
			string tk = vt.value("type-kind", "");
			if (tk == "prim" || ((tk == "strct" || tk == "union") && vt.contains("type-name")))
				typedefs.push_back({{"name", name}, {"var-type", vt}});
		}
		if (!typedefs.empty())
			ast["ast"]["typedefs"] = move(typedefs);
	}
	return ret;
}
