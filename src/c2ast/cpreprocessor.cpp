#include <string>
#include <vector>
#include <list>
#include <iostream>
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

static CMacro* macro_exists(CPreprocessor& cpp, const string& id);
static vector<CToken*> expand_macro_obj(CPreprocessor& cpp, CMacro *m);
static vector<vector<CToken*>> extruct_macro_func_args(CLexer &lexer, int &n, bool single_line=true);
static vector<CToken*> expand_macro_func(CPreprocessor& cpp, CMacro *mi, vector<vector<CToken*>> args);

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
				} else if (directive == "error") {
					cout << "Error at " << lexer.infile.fname
						<< ":" << t0->line_no << endl;
					BOOST_ASSERT(false);	// error
				} 

			} else {
				BOOST_ASSERT(false);
			}

		} else {
			if (t0->type == TT0_ID) {
				string s = lexer.get_str(t0);
				CMacro* m = macro_exists(*this, s);
				if (m) {
					if (m->type == MT_OBJ) {
						vector<CToken*> expanded_tokens = expand_macro_obj(*this, m);
						tokens->insert(tokens->end(), expanded_tokens.begin(), expanded_tokens.end());

					} else {
						BOOST_ASSERT(m->type == MT_FUNC);
						vector<vector<CToken*>> args = extruct_macro_func_args(lexer, n, false);
						vector<CToken*> expanded_tokens = expand_macro_func(*this, m, args);
						tokens->insert(tokens->end(), expanded_tokens.begin(), expanded_tokens.end());

					}
					continue;
				}
			}

			CToken* t = lexer.createToken(n);
			if (t) {	// skip comment
				tokens->push_back(t);
			}
		}
	}
	return true;
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
	cpp.preprocess(foundpath, tokens->back()->info.tokens);
	
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

		if (t.type == TT0_ID) {
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
static vector<vector<CToken*>> extruct_macro_func_args2(vector<CToken*> &tokens, int &n);
static bool evaluate_condition(vector<CToken*> &tokens);

int process_if(vector<IfInfo> &ifstack, CPreprocessor& cpp, CLexer& lexer, int n)
{
	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	ifstack.emplace_back(&token0s[n]);
	vector<CToken*> condition_tokens;

	while (!token0s[n].is_eol) {	// Read condition expression.
		n = next_pos(token0s, n);
		CToken* t = lexer.createIfMacroToken(n);

		if (t->type == TT_ID) {
			CMacro* m = macro_exists(cpp, *t->info.id);
			if (m) {
				if (m->type == MT_OBJ) {
					vector<CToken*> expanded_tokens = expand_macro_obj(cpp, m);
					condition_tokens.insert(condition_tokens.end(), expanded_tokens.begin(), expanded_tokens.end());

				} else {
					BOOST_ASSERT(m->type == MT_FUNC);
					vector<vector<CToken*>> args = extruct_macro_func_args(lexer, n);
					vector<CToken*> expanded_tokens = expand_macro_func(cpp, m, args);
					condition_tokens.insert(condition_tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				}
				continue;
			}

		} else if(t->type == TT_KEYWORD && t->info.keyword == TK_DEFINED) {
			// process of "defined" expression
			int nn = n;
			delete t;
			is_not_eol(token0s[n]);
			string macro_name;
			n = next_pos(token0s, n);
			CToken0& t0 = token0s[n];
			if (t0.type == TT0_ID) {
				// defined ID
				macro_name = lexer.get_str(&t0);

			} else if (t0.type == TT0_PUNCTUATOR && lexer.get_ch(&t0) == '(' && !t0.is_eol) {
				// defined (ID)
				n = next_pos(token0s, n);
				t0 = token0s[n];
				is_not_eol(token0s[n]);
				if (t0.type == TT0_ID) {
					macro_name = lexer.get_str(&t0);
				} else {
					BOOST_ASSERT(false);
				}
				n = next_pos(token0s, n);
				t0 = token0s[n];
				if (t0.type != TT0_PUNCTUATOR || lexer.get_ch(&t0) != ')') {
					BOOST_ASSERT(false);
				}
				
			} else {
				BOOST_ASSERT(false);
			}

			t = new CToken(TT_INT, lexer.no, nn);
			CMacro* m = macro_exists(cpp, macro_name);
			if (m) {
				t->info.intval = 1;
			} else {
				t->info.intval = 0;
			}

		}

		condition_tokens.push_back(t);
	}

/*
	cout << "if:";
	for (CToken* t: condition_tokens) {
		if (t->type == TT_ID)
			cout << " " << *t->info.id;
		else if (t->type == TT_PUNCTUATOR) {
			cout << " ";
			char c0 = t->info.punc & 0xFF;
			char c1 = (t->info.punc >> 8) & 0xFF;
			if (c1) cout << c1;
			cout << c0;

		} else if (t->type == TT_INT || t->type == TT_UINT || t->type == TT_LONG || t->type == TT_ULONG)
			cout << " " << t->info.intval;
		else if (t->type == TT_KEYWORD)
			cout << " d";
		else 
			cout << " ?";
	}
	cout << endl;
	*/
	
	ifstack.back().is_fulfilled = evaluate_condition(condition_tokens);

	if (!ifstack.back().is_fulfilled) {
		n = skip_to_nextif(ifstack, lexer, n);
	}

	return n;
}

vector<CToken*> expand_macro_obj(CPreprocessor& cpp, CMacro *m)
{

	auto mi = find(cpp.macro_stack.begin(), cpp.macro_stack.end(), m);
	if (mi != cpp.macro_stack.end()) {
		BOOST_ASSERT(false); 	// recursive macro
	}

	vector<CToken*> tokens;

	cpp.macro_stack.push_back(m);

	for (int n=0; n<m->body.size(); n++) {
		CToken *t = m->body[n];
		if (t->type == TT_ID) {
			CMacro* mm = macro_exists(cpp, *t->info.id);
			if (mm) {
				if (mm->type == MT_OBJ) {
					vector<CToken*> expanded_tokens = expand_macro_obj(cpp, mm);
					tokens.insert(tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				} else {
					BOOST_ASSERT(mm->type == MT_FUNC);
					vector<vector<CToken*>> args = extruct_macro_func_args2(m->body, n);
					vector<CToken*> expanded_tokens = expand_macro_func(cpp, mm, args);
					tokens.insert(tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				}
				continue;
			}
		}
		CToken *tt = new CToken(*t);
		tokens.push_back(tt);

	}

	cpp.macro_stack.pop_back();

	return tokens;
}

static string get_oristr(CPreprocessor& ppc, CToken *t);

vector<CToken*> expand_macro_func(CPreprocessor& cpp, CMacro *m, vector<vector<CToken*>> args)
{
	auto mi = find(cpp.macro_stack.begin(), cpp.macro_stack.end(), m);
	if (mi != cpp.macro_stack.end()) {
		BOOST_ASSERT(false); 	// recursive macro
	}

	if (m->params.size() != args.size()) {
		CPreprocessError err("argment error: " + m->name);
		if (args.size() && args[0].size()) {
			err.lexer_no = args[0][0]->lexer_no;
			err.token0_start = args[0][0]->token0_no;
		}
		cpp.outputError(err);
		BOOST_ASSERT(false);
	}

	vector<CToken*> pre_tokens;
	cpp.macro_stack.push_back(m);

	for (int n=0; n<m->body.size(); n++) {
		CToken *t = m->body[n];

		if (t->type == TT_PUNCTUATOR && t->info.punc == '#') {	// stringnize
			n++;
			if (n>=m->body.size()) {
				BOOST_ASSERT(false);
			}
			t = m->body[n];
			if (t->type != TT_ID) {
				BOOST_ASSERT(false);
			}
			int p;
			bool matched = false;
			for (p=0; p < m->params.size(); p++) {
				if (*t->info.id == m->params[p]) {
					matched = true;
					break;
				}
			}
			if (!matched) {
				BOOST_ASSERT(false);
			}

			CToken &arg_start = *args[p][0];
			CToken &arg_end = **(args[p].end()-1);
			CLexer &lexer = *cpp.lexers[arg_start.lexer_no];

			string s = lexer.get_oristr(&arg_start, &arg_end);

			pre_tokens.push_back(new CToken(s, arg_start.lexer_no, arg_start.token0_no));

		} else if (t->type == TT_ID) {	// expand argument
			bool matched = false;
			for (int p=0; p < m->params.size(); p++) {
				if (*t->info.id == m->params[p]) {
					for (CToken *at: args[p]) {
						pre_tokens.push_back(new CToken(*at));
					}
					matched = true;
					break;
				}
			}
			if (matched)
				continue;
		}
		pre_tokens.push_back(new CToken(*t));
	}

	vector<CToken*> pre_tokens2;
	for (int n=0; n<pre_tokens.size(); n++) {
		CToken *t = pre_tokens[n];
		if (t->type == TT_PUNCTUATOR && t->info.punc == '##') {
			if (!pre_tokens2.size()) {
				BOOST_ASSERT(false);
			}
			CToken* pt = pre_tokens2.back();
			if (pt->type != TT_ID) {
				BOOST_ASSERT(false);
			}
			delete t;

			n++;
			if (n>=pre_tokens.size()) {
				BOOST_ASSERT(false);
			}
			t = pre_tokens[n];
			string s = get_oristr(cpp, t);


			delete t;
			*pt->info.id += s;
			continue;
		}
		pre_tokens2.push_back(t);

	}

	vector<CToken*> tokens;
	for (int n=0; n<pre_tokens2.size(); n++) {
		CToken *t = pre_tokens2[n];
		if (t->type == TT_ID) {
			CMacro* mm = macro_exists(cpp, *t->info.id);
			if (mm) {
				if (mm->type == MT_OBJ) {
					vector<CToken*> expanded_tokens = expand_macro_obj(cpp, mm);
					tokens.insert(tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				} else {
					BOOST_ASSERT(m->type == MT_FUNC);
					vector<vector<CToken*>> args = extruct_macro_func_args2(pre_tokens2, n);
					vector<CToken*> expanded_tokens = expand_macro_func(cpp, mm, args);
					tokens.insert(tokens.end(), expanded_tokens.begin(), expanded_tokens.end());
				}
				delete t;
				continue;
			}
		}
		tokens.push_back(t);
	}

	cpp.macro_stack.pop_back();

	return tokens;
}

string get_oristr(CPreprocessor& cpp, CToken *t)
{
	return cpp.lexers[t->lexer_no]->get_oristr(t->token0_no);
}

vector<vector<CToken*>> extruct_macro_func_args(CLexer &lexer, int &n, bool single_line)
{
	vector<vector<CToken*>> args;

	vector<CToken0> &token0s = lexer.tokens;
	is_not_eol(token0s[n]);

	n++;
	CToken* t = lexer.createIfMacroToken(n);
	if (t->type != TT_PUNCTUATOR || t->info.punc != '(') {
		BOOST_ASSERT(false);
	}
	
	int blace_level = 1;
	vector<CToken*> arg;
	bool succeeded = false;

	for(;;) {
		// Skip comment
		for(;;) {
			if(single_line && token0s[n].is_eol)
				goto ENDLOOP;
			n++;
			if (n >= token0s.size()) 
				goto ENDLOOP;
			CToken0 &t0 = token0s[n];
			if (t0.type != TT0_COMMENT)
				break;
			if (t0.is_eol && single_line)
				goto ENDLOOP;
		}

		CToken* t = lexer.createIfMacroToken(n);
		if (t->type == TT_PUNCTUATOR) {
			if (t->info.punc == '(') {
				blace_level++;
			} else if (t->info.punc == ')') {
				blace_level--;
				if (blace_level == 0) {
					delete t;
					succeeded = true;
					break;
				}
			} else if (t->info.punc == ',' && blace_level == 1) {
				delete t;
				if (!arg.size()) {
					BOOST_ASSERT(false);
				}
				args.push_back(arg);
				arg.clear();
				continue;
			}
		}
		arg.push_back(t);
	};
ENDLOOP:
	if (!succeeded) {
		BOOST_ASSERT(false);
	}

	if (arg.size())
		args.push_back(arg);

	return args;
}

vector<vector<CToken*>> extruct_macro_func_args2(vector<CToken*> &tokens, int &n)
{
	vector<vector<CToken*>> args;
	BOOST_ASSERT(n+2 < tokens.size());
	n++;
	BOOST_ASSERT(tokens[n]->type == TT_PUNCTUATOR);
	BOOST_ASSERT(tokens[n]->info.punc == '(');
	n++;

	int blace_level = 1;
	vector<CToken*> arg;

	for (; n < tokens.size(); n++) {
		CToken *t = tokens[n];
		if (t->type == TT_PUNCTUATOR) {
			if (t->info.punc == '(') {
				blace_level++;
			} else if (t->info.punc == ')') {
				blace_level--;
				if (blace_level == 0) {
					break;
				}
			} else if (t->info.punc == ',' && blace_level == 1) {
				args.push_back(arg);
				continue;
			}
		}
		arg.push_back(new CToken(*t));
	}
	args.push_back(arg);

	return args;
}

CMacro* macro_exists(CPreprocessor& cpp, const string& id)
{
	auto mi = find_if(cpp.macros.begin(), cpp.macros.end(),
			[id](CMacro* m) { return m->name == id; });

	if (mi != cpp.macros.end()) {
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
			x = x & land(tokens, n);
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
			x = x & bor(tokens, n);
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
			x = x & bxor(tokens, n);
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
			x = x & band(tokens, n);
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

	if (tt == TT_INT || tt == TT_LONG) { 
		x = t->info.intval;
	} else if (tt == TT_UINT || tt == TT_ULONG) {
		x = t->info.uintval;
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
