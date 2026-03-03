#include <string>
#include <vector>
#include <list>
#include <set>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <boost/assert.hpp>

using namespace std;
#include "cfileinfo.h"
#include "ctoken.h"
#include "clexer.h"
#include "cpreprocessor.h"

// utilities
class IfInfo {
public:
	CToken0* start;
	bool is_fulfilled;
	bool is_else;
	IfInfo(CToken0* t): start(t), is_fulfilled(false), is_else(false) {}
};

static int process_include(CPreprocessor& cpp, CLexer& lexer, int n, vector<CToken*>* tokens);
static int process_define(CPreprocessor& cpp, CLexer& lexer, int n);
static int process_undef(CPreprocessor& cpp, CLexer& lexer, int n);
static int process_if(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n);
static int process_ifdef(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n, bool is_ndef = false);
static int process_elif(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n);
static int process_else(vector<IfInfo> &ifstack, CLexer& lexer, int n);
static int process_endif(vector<IfInfo> &ifstack, CLexer& lexer, int n);

static int next_pos(const vector<CToken0>& token0s, int n);

//== CMacro ==
CMacro::CMacro(CMacroType type, string name, int lexer_no)
	: type(type), name(name), lexer_no(lexer_no)
{
}

//== CPreprocessor ==
bool CPreprocessor::loadPredefined(const string& filepath)
{
	CFileInfo finfo(filepath);
	CLexer lexer(finfo);

	vector<CToken0>& token0s = lexer.scan();
	for (int n=0; n<token0s.size(); n++) {
		CToken0 *t = &token0s[n];
		if (t->type == TT0_MACRO) {		// '#'
			n = next_pos(token0s, n);
			BOOST_ASSERT(t != &token0s[n]);
			t = &token0s[n];
			if (t->type == TT0_ID && lexer.get_str(t) == "define") {
				n = process_define(*this, lexer, n);
			} else {
				BOOST_ASSERT(false);
			}
		}
	}

	return true;
}

bool CPreprocessor::preprocess(const string& filepath, vector<CToken*> *tokens)
{
	if (!tokens) {
		tokens = &top_tokens;
	}
	int lexer_no = lexers.size();
	CFileInfo finfo(filepath);
	lexers.push_back(new CLexer(finfo));
	CLexer& lexer = *lexers.back();
	lexer.no = lexer_no;

	vector<IfInfo> ifstack;

	vector<CToken0>& token0s = lexer.scan();

	bool debug = false;

	for (int n=0; n<token0s.size(); n++) {
		BOOST_ASSERT(!macro_stack.size());
		CToken0 *t0 = &token0s[n];
		if (t0->type == TT0_MACRO) {	// '#'
			if (t0->is_eol) continue;	// null directive

			n = next_pos(token0s, n);
			BOOST_ASSERT(t0 != &token0s[n]);
			t0 = &token0s[n];

			if (t0->type == TT0_ID) {
				string directive = lexer.get_str(t0);
				if (directive == "include") {
					n = process_include(*this, lexer, n, tokens); 
					
				} else if (directive == "include_next") {

				} else if (directive == "define") {
					n = process_define(*this, lexer, n);

				} else if (directive == "undef") {
					n = process_undef(*this, lexer, n);

				} else if (directive == "if") {
					n = process_if(ifstack, *this, lexer, n);

				} else if (directive == "ifdef") {
					n = process_ifdef(ifstack, *this, lexer, n);

				} else if (directive == "ifndef") {
					n = process_ifdef(ifstack, *this, lexer, n, true);

				} else if (directive == "elif") {
					n = process_elif(ifstack, *this, lexer, n);

				} else if (directive == "else") {
					n = process_else(ifstack, lexer, n);

				} else if (directive == "endif") {
					n = process_endif(ifstack, lexer, n);

				} else if (directive == "line") {
				} else if (directive == "pragma") {
					n = next_pos(token0s, n);
					CToken0 &prag_t = token0s[n];
					if (prag_t.type == TT0_ID && lexer.get_str(&prag_t) == "once") {
						once_included.insert(lexer.infile.fname);
					}
					while (!token0s[n].is_eol) n = next_pos(token0s, n);
				} else if (directive == "error") {
					cout << "Error at " << lexer.infile.fname << ":" << t0->line_no << endl;
					BOOST_ASSERT(false);	// error
				} 

			} else {
				BOOST_ASSERT(false);
			}

		} else {
			list<CToken* > unprocessed_tokens;

			// collect tokens until next macro directive
			do {
				BOOST_ASSERT(t0->type != TT0_MACRO);
				CToken* t = lexer.createToken(n);
				if (t)	// skip comment
					unprocessed_tokens.push_back(t);

				n++;

				if (n >= token0s.size())
					break;
				t0 = &token0s[n];
			} while (t0->type != TT0_MACRO);

			n--; // for n++ in for loop

			// process unprocessed tokens
			vector<CToken*> processed_tokens = scan_macro(unprocessed_tokens);
			tokens->insert(tokens->end(), processed_tokens.begin(), processed_tokens.end() );
		}
	}
	return true;
}

vector<list<CToken*> > extract_macro_func_args(list<CToken*> &tokens)
{
	// Extract macro function arguments from tokens starting at front (the position of '(')
	// Remove argument tokens from tokens and return them as vector<list<CToken*>>

	vector<list<CToken*> > args;
	BOOST_ASSERT(tokens.front()->type == TT_PUNCTUATOR && tokens.front()->info.punc == '(');
	delete tokens.front();
	tokens.pop_front(); // remove '('
	int blace_level = 1;
	list<CToken*> arg;

	while (!tokens.empty()) {
		CToken* t = tokens.front();
		tokens.pop_front();

		if (t->type == TT_PUNCTUATOR) {
			if (t->info.punc == '(') {
				blace_level++;
			} else if (t->info.punc == ')') {
				blace_level--;
				if (blace_level == 0) {
					// end of arguments
					delete t;
					args.push_back(arg);
					break;
				}
			}

			if (blace_level == 1 && t->info.punc == ',') {
				// end of current argument
				delete t;
				args.push_back(arg);
				arg.clear();
				continue;
			}
		}
		arg.push_back(t);
	}

	return args;
}

CToken* get_defined_id_token(list<CToken*> &unprocessed_tokens)
{
	// expect '(' ID ')' or ID
	if (unprocessed_tokens.empty()) return NULL;

	CToken* t = unprocessed_tokens.front();
	if (t->type == TT_ID) {
		unprocessed_tokens.pop_front();
		return t;
	}

	// expect '('
	if (t->type != TT_PUNCTUATOR) return NULL;
	if (t->info.punc != '(') return NULL;
	unprocessed_tokens.pop_front(); // remove '('
	delete t;

	// expect ID
	if (unprocessed_tokens.empty()) return NULL;
	CToken* id_token = unprocessed_tokens.front();
	if (id_token->type != TT_ID) return NULL;
	unprocessed_tokens.pop_front(); // remove ID

	// expect ')'
	// TODO: handle error(delete id_token)
	if (unprocessed_tokens.empty()) return NULL;
	t = unprocessed_tokens.front();
	if (t->type != TT_PUNCTUATOR) return NULL;
	if (t->info.punc != ')') return NULL;
	unprocessed_tokens.pop_front(); // remove ')'
	delete t;

	return id_token;
}

vector<CToken*> CPreprocessor::scan_macro(list<CToken*> &unprocessed_tokens, bool in_if_directive)
{
	vector<CToken*> result_tokens;
	while (!unprocessed_tokens.empty()) {
		CToken* t = unprocessed_tokens.front();

		if (t->type == TT_ID) {
			CMacro* m = macro_exists(*t->info.id);
			if (m && !t->does_hide(m)) {
				unprocessed_tokens.pop_front(); // remove macro name token
				vector<list<CToken*> > args;
				if (m->type == MT_FUNC) {
					args = extract_macro_func_args(unprocessed_tokens);
				}
				vector<CToken*> expanded_tokens = expand_macro_tokens(m, args, t->hide_set);

				//result_tokens.insert(result_tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				unprocessed_tokens.insert(unprocessed_tokens.begin(), expanded_tokens.begin(), expanded_tokens.end());
				delete t;
				continue; // re-scan from the top
			}
			if (in_if_directive && *t->info.id == "defined") {
				unprocessed_tokens.pop_front(); // remove "defined" token

				// expect '(' ID ')' or ID
				CToken* id_token = get_defined_id_token(unprocessed_tokens);
				if (!id_token) {
					delete t;
					BOOST_ASSERT(false);	// syntax error
				}

				CMacro* dm = macro_exists(*id_token->info.id);
				if (dm) {
					CToken* one_token = new CToken(TT_PP_NUMBER, id_token->lexer_no, t->token0_no);
					one_token->info.str = new string("1");
					result_tokens.push_back(one_token);
				} else {
					CToken* zero_token = new CToken(TT_PP_NUMBER, id_token->lexer_no, t->token0_no);
					zero_token->info.str = new string("0");
					result_tokens.push_back(zero_token);
				}

				delete t;
				delete id_token;
				continue; // re-scan from the top
			}
		}

		unprocessed_tokens.pop_front();
		result_tokens.push_back(t);
	}
	return result_tokens;
}

vector<CToken*> replace_parameter_tokens(CToken* t, const vector<string>& params, const vector<list<CToken*> >& args)
{
	// replace parameter tokens in t and return as vector<CToken*>
	// if t is not parameter token, return vector with t itself
	vector<CToken*> result;
	if (t->type == TT_ID) {
		for (int p=0; p < params.size(); p++) {
			if (*t->info.id == params[p]) {
				// replace parameter by argument tokens
				// copy args[p] tokens
				for (CToken* at: args[p]) {
					result.push_back(new CToken(*at));
				}

				if (result.empty()) {
					// if argument is empty, insert empty token
					CToken* empty_token = new CToken(TT_ID, t->lexer_no, t->token0_no);
					empty_token->info.id = new string("");
					result.push_back(empty_token);
				}
				return result;
			}
		}
	}
	// not parameter token
	result.push_back(t);
	return result;
}

static string token_paste_str(CToken* t)
{
	switch (t->type) {
		case TT_ID: return *t->info.id;
		case TT_KEYWORD: return CLexer::keywords[t->info.keyword];
		case TT_PP_NUMBER: return *t->info.str;
		default:
			BOOST_ASSERT(false);
			return "";
	}
}

bool process_paste(vector<CToken*>& body_tokens, const vector<string>& params, const vector<list<CToken*> >& args)
{
	if (body_tokens.size() >= 3) {
		if (body_tokens[0]->type == TT_PUNCTUATOR && body_tokens[0]->info.punc == '##') {
			BOOST_ASSERT(false); // '##' at beginning is not allowed
		}

		for (int n=1; n < body_tokens.size(); n++) {
			CToken *t = body_tokens[n];
			if (t->type == TT_PUNCTUATOR && t->info.punc == '##') {
				if (n == body_tokens.size() - 1) {
					BOOST_ASSERT(false); // '##' at end is not allowed
				}
				CToken *t_prev = body_tokens[n-1];
				CToken *t_next = body_tokens[n+1];

				vector<CToken*> prev_tokens = replace_parameter_tokens(t_prev, params, args);
				vector<CToken*> next_tokens = replace_parameter_tokens(t_next, params, args);
				t_prev = prev_tokens.back();
				prev_tokens.pop_back();
				t_next = next_tokens.front();
				next_tokens.erase(next_tokens.begin());

				string pasted_str = token_paste_str(t_prev) + token_paste_str(t_next);
				CToken* pasted_token;
				if (isdigit(pasted_str[0]) || (pasted_str[0] == '.' && pasted_str.size() > 1 && isdigit(pasted_str[1]))) {
					pasted_token = new CToken(TT_PP_NUMBER, t->lexer_no, t->token0_no);
					pasted_token->info.str = new string(move(pasted_str));
				} else {
					pasted_token = new CToken(TT_ID, t->lexer_no, t->token0_no);
					pasted_token->info.id = new string(pasted_str);
					for (int i = 0; i < TK_NOT_KEYWORD; i++) {
						if (pasted_str == CLexer::keywords[i]) {
							pasted_token->type = TT_KEYWORD;
							delete pasted_token->info.id;
							pasted_token->info.keyword = (CTokenKeyword)i;
							break;
						}
					}
				}
				
				body_tokens.erase(body_tokens.begin() + n - 1, body_tokens.begin() + n + 2);
				body_tokens.insert(body_tokens.begin() + n - 1, prev_tokens.begin(), prev_tokens.end());
				body_tokens.insert(body_tokens.begin() + n - 1 + prev_tokens.size(), pasted_token);
				body_tokens.insert(body_tokens.begin() + n - 1 + prev_tokens.size() + 1, next_tokens.begin(), next_tokens.end());
				return true;
			}
		}
	}

	return false;
}

vector<CToken*> CPreprocessor::expand_macro_tokens(CMacro *m, vector<list<CToken*> > &args, vector<CMacro*> *hide_set)
{
	vector<CToken*> body_tokens;
	// copy macro body
	for (int n=0; n < m->body.size(); n++) {
		CToken *t = new CToken(*m->body[n]);
		t->add_to_hide_set(m); // add to hide set
		if (hide_set) {
			for (CMacro* hm: *hide_set) {
				t->add_to_hide_set(hm);
			}
		}
		body_tokens.push_back(t);
	}

	// process stringizing '#'
	// TODO: implement stringizing

	// process token pasting '##'
	while(process_paste(body_tokens, m->params, args))
		;

	// expand arguments
	vector<vector<CToken*> > args_expanded;
	for (list<CToken*> &arg: args) {
		vector<CToken*> arg_tokens = scan_macro(arg);
		args_expanded.push_back(arg_tokens);
	}

	// replace parameters in body_tokens
	for (int n=0; n<body_tokens.size(); n++) {
		CToken *t = body_tokens[n];
		if (t->type == TT_ID) {
			if (*t->info.id == "__VA_ARGS__") {
				int va_idx = -1;
				for (int p = 0; p < (int)m->params.size(); p++) {
					if (m->params[p] == "...") { va_idx = p; break; }
				}
				if (va_idx >= 0) {
					vector<CToken*> va_tokens;
					for (int p = va_idx; p < (int)args_expanded.size(); p++) {
						if (p > va_idx) {
							CToken* comma = new CToken(TT_PUNCTUATOR, t->lexer_no, t->token0_no);
							comma->info.punc = ',';
							va_tokens.push_back(comma);
						}
						for (CToken* at: args_expanded[p]) {
							va_tokens.push_back(new CToken(*at));
						}
					}
					delete t;
					body_tokens.erase(body_tokens.begin() + n);
					body_tokens.insert(body_tokens.begin() + n, va_tokens.begin(), va_tokens.end());
					n += (int)va_tokens.size() - 1;
					continue;
				}
			}
			for (int p=0; p < m->params.size(); p++) {
				if (*t->info.id == m->params[p]) {
					// replace parameter by argument tokens
					// copy args[p] tokens
					vector<CToken*> arg_copied;
					for (CToken* at: args_expanded[p]) {
						arg_copied.push_back(new CToken(*at));
					}
					delete t;
					body_tokens.erase(body_tokens.begin() + n);
					body_tokens.insert(body_tokens.begin() + n, arg_copied.begin(), arg_copied.end());
					n += arg_copied.size() - 1;
					break;
				}
			}
		}
	}

	for (vector<CToken*> &arg: args_expanded) {
		for (CToken* at: arg) {
			delete at;
		}
	}

	return body_tokens;
}

//== utilities ==
void is_not_eol(CToken0& t)
{
	if (t.is_eol) {
		BOOST_ASSERT(false);
	}
}

int process_include(CPreprocessor& cpp, CLexer& lexer, int n, vector<CToken*>* tokens)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	int start = n;
	n = next_pos(token0s, n);
	CToken0 &fname_t = token0s[n];

	if (fname_t.type != TT0_STR) {	// filename should be <...> or "..."
		BOOST_ASSERT(false);
	}

	string inc_path = lexer.get_str(&fname_t);
	string foundpath = CFileInfo::getFilePath(inc_path, lexer.infile.fname, cpp.include_paths);

	if (foundpath == "") {
		cout << inc_path << " is not found." << endl;
		BOOST_ASSERT(false);
	}

	tokens->push_back(new CToken(TT_INCLUDE, lexer.no, start));
	if (cpp.once_included.count(foundpath) == 0) {
		cpp.preprocess(foundpath, tokens->back()->info.tokens);
	}
	
	if (!fname_t.is_eol) {
		n = next_pos(token0s, n);
		if (!token0s[n].type == TT0_COMMENT || !token0s[n].is_eol) {
			BOOST_ASSERT(false);
		}
	}

	return n;
}

static void delete_macro(vector<CMacro*> &macros, string &macro_name);
static int parse_params(vector<string>& params, CLexer& lexer, int n);
static int set_macro_body(vector<CToken*> &body, CLexer& lexer, int n);

int process_define(CPreprocessor& cpp, CLexer& lexer, int n)
{
	CFileInfo &curfile = lexer.infile;
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	n = next_pos(token0s, n);
	CToken0 &t = token0s[n];

	if (t.type != TT0_ID) {
		BOOST_ASSERT(false);
	}
	string macro_name = lexer.get_str(&t);
	delete_macro(cpp.macros, macro_name);

	if (t.is_eol) {	// define only name
		cpp.macros.push_back(new CMacro(MT_OBJ, lexer.get_str(&t), lexer.no));
		return n;
	}

	n = next_pos(token0s, n);
	CToken0 &t1 = token0s[n];

	if (t1.type == TT0_COMMENT) {	// define only name
		BOOST_ASSERT(t1.is_eol);
		cpp.macros.push_back(new CMacro(MT_OBJ, macro_name, lexer.no));
		return n;

	} else if (t1.type == TT0_MACRO_ARGS) {	// define function like macro
		CMacro* func_macro = new CMacro(MT_FUNC, macro_name, lexer.no);
		n = parse_params(func_macro->params, lexer, n);
		n = set_macro_body(func_macro->body, lexer, n);
		cpp.macros.push_back(func_macro);

		return n;

	} else {
		CMacro* macro = new CMacro(MT_OBJ, macro_name, lexer.no);
		CToken* t = lexer.createToken(n);
		macro->body.push_back(t);
		n = set_macro_body(macro->body, lexer, n);
		cpp.macros.push_back(macro);

		return n;
	}
}

void delete_macro(vector<CMacro*> &macros, string &macroname)
{
	auto macro_i = find_if(macros.begin(), macros.end(), [&](CMacro* m) { return m->name == macroname; } );
	if (macro_i != macros.end()) {
		delete *macro_i;
		macros.erase(macro_i);
	}
}

int parse_params(vector<string>& params, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);	// NG: #define hoge(

	for (;;) {
		n = next_pos(token0s, n);
		CToken0 &t = token0s[n];

		if (t.type == TT0_ID || (t.type == TT0_PUNCTUATOR && lexer.get_ch(&t) == '...')) {
			is_not_eol(t);
			params.push_back(lexer.get_str(&t));
			n = next_pos(token0s, n);

			CToken0 &t_next = token0s[n];
			if (t_next.type == TT0_PUNCTUATOR) {
				int c = lexer.get_ch(&t_next);
				if (c == ',') { // next arg
					continue;

				} else if (c == ')') { // end arg
					break;

				} else {
					BOOST_ASSERT(false);	// error
				}
			} else {
				BOOST_ASSERT(false);	// error
			}
		} else if (t.type == TT0_PUNCTUATOR && lexer.get_ch(&t) == ')') {
			break;
		} else {
			BOOST_ASSERT(false);	// error
		}
	}

	return n;
}

int set_macro_body(vector<CToken*> &body, CLexer& lexer, int n)
{
	CToken0 *t0 = &lexer.tokens[n];
	if (t0->is_eol)
		return n;

	do {
		n++;
		CToken* t = lexer.createToken(n);
		if (t) {
			body.push_back(t);
		}
		t0 = &lexer.tokens[n];
	} while (!t0->is_eol);

	return n;
}

int process_undef(CPreprocessor& cpp, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;

	is_not_eol(token0s[n]);

	n = next_pos(token0s, n);
	CToken0 &t = token0s[n];

	if (t.type != TT0_ID) {
		BOOST_ASSERT(false);
	}

	string macro_name = lexer.get_str(&t);
	delete_macro(cpp.macros, macro_name);

	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			BOOST_ASSERT(false); 	// output warning
		}
	}

	return n;
}

static int skip_to_nextif(vector<IfInfo>& ifstack, CLexer& lexer, int n);
static bool evaluate_condition(vector<CToken*> &tokens);

int process_if(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	ifstack.emplace_back(&token0s[n]);
	list<CToken*> unprocessed_tokens;
	vector<CToken*> condition_tokens;

	do {
		n++;
		CToken *t = lexer.createToken(n);
		if (t) {
			unprocessed_tokens.push_back(t);
		}

	} while (!token0s[n].is_eol);

	condition_tokens = cpp.scan_macro(unprocessed_tokens, true);

	ifstack.back().is_fulfilled = evaluate_condition(condition_tokens);

	if (!ifstack.back().is_fulfilled) {
		n = skip_to_nextif(ifstack, lexer, n);
	}

	return n;
}

CMacro* CPreprocessor::macro_exists(const string& id)
{
	auto mi = find_if(macros.begin(), macros.end(),
			[id](CMacro* m) { return m->name == id; });

	if (mi != macros.end()) {
		return *mi;
	}
	return NULL;
}

// Condition parser definition
// expr     = lor ("?" expr ":" expr)?
// lor      = land ("||" land)*
// land     = bor ("&&"  bor)*
// bor      = bxor ("|" bxor)*
// bxor     = band ("^" band)*
// band     = equality ("&" equality)*
// equality   = relational ("==" relational | "!=" relational)*
// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
// shift      = add ("<<" add | ">>" add)*
// add        = mul ("+" mul | "-" mul)*
// mul        = unary ("*" unary | "/" unary | "%" unary)*
// unary      = ("+" | "-" | "~" | "!")? primary
// primary    = num | "(" expr ")" | id(=0)

static long expr(vector<CToken*> &tokens, int &n);
static long lor(vector<CToken*> &tokens, int &n);
static long land(vector<CToken*> &tokens, int &n);
static long bor(vector<CToken*> &tokens, int &n);
static long bxor(vector<CToken*> &tokens, int &n);
static long band(vector<CToken*> &tokens, int &n);
static long equality(vector<CToken*> &tokens, int &n);
static long relational(vector<CToken*> &tokens, int &n);
static long shift(vector<CToken*> &tokens, int &n);
static long add(vector<CToken*> &tokens, int &n);
static long mul(vector<CToken*> &tokens, int &n);
static long unary(vector<CToken*> &tokens, int &n);
static long primary(vector<CToken*> &tokens, int &n);
static bool consume(vector<CToken*> &tokens, int &n, int ch);

#define END_CHECK	{if(n>=tokens.size())BOOST_ASSERT(false);}
#define CONSUME(ch)	consume(tokens, n, (ch))

bool evaluate_condition(vector<CToken*> &tokens)
{
	int n = 0;
	int x = expr(tokens, n);
	if (n < tokens.size()) {
		BOOST_ASSERT(false);
	}
	return x != 0;
}

long expr(vector<CToken*> &tokens, int &n)
{
	int x = lor(tokens, n);
	if (CONSUME('?')) {
		int y = expr(tokens, n);
		if (!CONSUME(':')) {
			BOOST_ASSERT(false);
		}
		int z = expr(tokens, n);
		return x ? y : z;
	}
	return x;
}

long lor(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = land(tokens, n);
	for (;;) {
		if (CONSUME('||')) {
			long rhs = land(tokens, n);
			x = x || rhs;
		} else {
			break;
		}
	}
	return x;
}

long land(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = bor(tokens, n);
	for (;;) {
		if (CONSUME('&&')) {
			long rhs = bor(tokens, n);
			x = x && rhs;
		} else {
			break;
		}
	}
	return x;
}

long bor(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = bxor(tokens, n);
	for (;;) {
		if (CONSUME('|')) {
			x = x | bxor(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long bxor(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = band(tokens, n);
	for (;;) {
		if (CONSUME('^')) {
			x = x ^ band(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long band(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = equality(tokens, n);
	for (;;) {
		if (CONSUME('&')) {
			x = x & equality(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long equality(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = relational(tokens, n);
	for (;;) {
		if (CONSUME('==')) {
			x = x == relational(tokens, n);
		} else if (CONSUME('!=')) {
			x = x != relational(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long relational(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = shift(tokens, n);
	for (;;) {
		if (CONSUME('<')) {
			x = x < shift(tokens, n);
		} else if (CONSUME('<=')) {
			x = x <= shift(tokens, n);
		} else if (CONSUME('>')) {
			x = x > shift(tokens, n);
		} else if (CONSUME('>=')) {
			x = x >= shift(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long shift(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = add(tokens, n);
	for (;;) {
		if (CONSUME('<<')) {
			x = x << add(tokens, n);
		} else if (CONSUME('>>')) {
			x = x >> add(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long add(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = mul(tokens, n);
	for (;;) {
		if (CONSUME('+')) {
			x = x + mul(tokens, n);
		} else if (CONSUME('-')) {
			x = x - mul(tokens, n);
		} else {
			break;
		}
	}

	return x;
}

long mul(vector<CToken*> &tokens, int &n)
{
	END_CHECK;
	int x = unary(tokens, n);
	for (;;) {
		if (CONSUME('*')) {
			x = x * unary(tokens, n);
		} else if (CONSUME('/')) {
			x = x / unary(tokens, n);
		} else if (CONSUME('%')) {
			x = x % unary(tokens, n);
		} else {
			break;
		}
	}
	return x;
}

long unary(vector<CToken*> &tokens, int &n)
{
	END_CHECK;

	if (CONSUME('+')) {
		return primary(tokens, n);
	} else if (CONSUME('-')) {
		return -primary(tokens, n);
	} else if (CONSUME('~')) {
		return ~primary(tokens, n);
	} else if (CONSUME('!')) {
		return !primary(tokens, n);
	}

	return primary(tokens, n);
}

long primary(vector<CToken*> &tokens, int &n)
{
	END_CHECK;

	long x = 0;

	if (CONSUME('(')) {
		x = expr(tokens, n);
		if (!CONSUME(')')) {
			BOOST_ASSERT(false);
		}
		return x;
	}

	CToken *t = tokens[n];
	CTokenType tt = t->type;

	if (tt == TT_PP_NUMBER) {
		x = stoll(*t->info.str, nullptr, 0);
	} else if (tt == TT_ID) {
		x = 0;
	} else {
		BOOST_ASSERT(false);
	}

	n++;
	return x;
}

bool consume(vector<CToken*> &tokens, int &n, int ch)
{
	if (n >= tokens.size()) {
		return false;
	}
	CToken *t = tokens[n];
	if (t->type == TT_PUNCTUATOR && t->info.punc == ch) {
		n++;
		return true;
	}
	return false;
}

int process_ifdef(vector<IfInfo>& ifstack, CPreprocessor& cpp, CLexer& lexer, int n, bool is_ndef)
{
	vector<CToken0> &token0s = lexer.tokens;

	is_not_eol(token0s[n]);

	ifstack.emplace_back(&token0s[n]);
	n = next_pos(token0s, n);
	
	CToken0 &t = token0s[n];
	if (t.type != TT0_ID) {
		BOOST_ASSERT(false);
	}

	string macro_name = lexer.get_str(&t);
	auto& macros = cpp.macros;
	auto macro_i = find_if(macros.begin(), macros.end(), [&](CMacro* m) { return m->name == macro_name; } );
	if (is_ndef) {
		ifstack.back().is_fulfilled = macro_i == macros.end();
	} else {
		ifstack.back().is_fulfilled = macro_i != macros.end();
	}

	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			BOOST_ASSERT(false); 	// output warning
		}
	}

	if (!ifstack.back().is_fulfilled) {
		n = skip_to_nextif(ifstack, lexer, n);
	}

	return n;
}

int process_elif(vector<IfInfo> &ifstack, CPreprocessor &cpp, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		BOOST_ASSERT(false);
	}

	if (ifstack.back().is_fulfilled) {
		return skip_to_nextif(ifstack, lexer, n);
	}

	BOOST_ASSERT(!ifstack.back().is_else);

	ifstack.pop_back();
	return process_if(ifstack, cpp, lexer, n);
}

int process_else(vector<IfInfo> &ifstack, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		BOOST_ASSERT(false);
	}

	vector<CToken0> &token0s = lexer.tokens;
	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			BOOST_ASSERT(false); 	// output warning
		}
	}

	if (ifstack.back().is_fulfilled) {
		ifstack.back().is_else = true;
		n = skip_to_nextif(ifstack, lexer, n);
	} else {
		ifstack.back().is_fulfilled = true;
		ifstack.back().is_else = true;
	}

	return n;
}

int process_endif(vector<IfInfo> &ifstack, CLexer& lexer, int n)
{
	if (!ifstack.size()) {
		BOOST_ASSERT(false);
	}

	ifstack.pop_back();

	vector<CToken0> &token0s = lexer.tokens;
	while (!token0s[n].is_eol) {
		n = next_pos(token0s, n);
		if (token0s[n].type != TT0_COMMENT) {
			BOOST_ASSERT(false); 	// output warning
		}
	}

	return n;
}

int skip_to_nextif(vector<IfInfo>& ifstack, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	n++;

	while (n < token0s.size()) {
		CToken0 *t = &token0s[n];
		while (t->type != TT0_MACRO) {
			n++;
			if (n >= token0s.size())
				return n;
			t = &token0s[n];
		}

		BOOST_ASSERT(t->type == TT0_MACRO);

		if (t->is_eol) {
			n++;
			continue;
		}

		int m = next_pos(token0s, n);

		t = &token0s[m];
		if (t->type != TT0_ID) {
			n = m;
			continue;
		}

		string directive = lexer.get_str(t);
		if (directive == "if"
				|| directive == "ifdef"
				|| directive == "ifndef") {
			ifstack.emplace_back(t);
			ifstack.back().is_fulfilled = true;
			n = skip_to_nextif(ifstack, lexer, n);

		} else if (directive == "elif"
				|| directive == "else") {
			if (ifstack.back().is_else) {
				BOOST_ASSERT(false);
			}
			if (ifstack.back().is_fulfilled) {
				n = m+1;
				continue;	// skip
			}
			return n-1;	// back to before '#'

		} else if (directive == "endif") {
			return process_endif(ifstack, lexer, m);
		}
		n++;
	}

	return n;
}

int next_pos(const vector<CToken0>& token0s, int n)
{
	BOOST_ASSERT(!token0s[n].is_eol);

	// Search next token (Skip comment and end search at eol.)
	const CToken0 *t = NULL;
	do {
		n++;
		t = &token0s[n];
	} while(t->type == TT0_COMMENT && !t->is_eol);

	return n;
}

void CPreprocessor::outputError(const CPreprocessError& err)
{
	if (err.lexer_no >= 0 && err.token0_start >= 0) {
		CLexer &lexer = *lexers[err.lexer_no];
		CToken0 &t = lexer.tokens[err.token0_start];
		string& line = lexer.infile.lines[t.line_no-1];
		cout << lexer.infile.fname << ":"
			<< t.line_no << ":" << t.pos+1 << ": "
			<< err.what() << endl;
		cout << line << endl;

		for (int i=0; i<t.pos; i++) {
			cout << ' ';
		}
		cout << '^' << endl;

	} else {
		cout << err.what() << endl;
	}
}

CPreprocessError::CPreprocessError(const string& what, int lexer_no, int start, int end)
	: runtime_error(what), lexer_no(lexer_no), token0_start(start), token0_end(end)
{
}

void CPreprocessor::dumpPreprocessed(ostream& os, vector<CToken*> *tokens)
{
	if (!tokens) {
		tokens = &top_tokens;
	}

	for (CToken* t: *tokens) {
		if (t->type == TT_INCLUDE) {
			dumpPreprocessed(os, t->info.tokens);
		} else if (t->type == TT_ID) {
			os << *t->info.id;
		} else if (t->type == TT_PP_NUMBER) {
			os << *t->info.str;
		} else if (t->type == TT_PUNCTUATOR) {
			os << (char)(t->info.punc);
		} else if (t->type == TT_STR) {
			os << *t->info.str;
		} else if (t->type == TT_KEYWORD) {
			BOOST_ASSERT(t->info.keyword < TK_NOT_KEYWORD);
			os << CLexer::keywords[t->info.keyword] << " ";
		} else if (t->type == TT_STR) {
			os << *t->info.str;	
		} else {
			os << ".";
		}
	}

}


