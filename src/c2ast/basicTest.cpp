#include <vector>
#include <list>
#include <string>
#include <gtest/gtest.h>
using namespace std;
#include "../src/cfileinfo.h"
#include "../src/ctoken.h"
#include "../src/clexer.h"
#include "../src/cpreprocessor.h"

static void dumptokens(vector<CToken0>& tokens, vector<string>& lines)
{
	for (CToken0 &t: tokens) {
		string s =
			t.type == TT0_ID ? "id":
			t.type == TT0_NUMBER ? "number":
			t.type == TT0_STR ? "str":
			t.type == TT0_CHAR ? "char":
			t.type == TT0_PUNCTUATOR ? "punc":
			t.type == TT0_COMMENT ? "comment":
			t.type == TT0_MACRO? "macro":
			t.type == TT0_MACRO_ARGS? "macro arg":
			"unknown";

		cout << s << ":" << lines[t.line_no-1].substr(t.pos, t.len) << endl;
	}
}

TEST(Lexer, HelloWorld)
{
	CFileInfo finfo("cases/001_helloworld.c");
	ASSERT_EQ(finfo.lines.size(), 11);

	string& line = finfo.getLine(11);
	ASSERT_EQ(line, "}");

	CLexer lexer(finfo);
	vector<CToken0>& tokens = lexer.scan();
//	dumptokens(tokens, finfo.lines);

	ASSERT_EQ(tokens.size(), 21);
}

static void dumpmacros(vector<CMacro*> &macros)
{
	for (CMacro* m: macros) {
		cout << m->name;
		if (m->type == MT_FUNC) {
			cout << "(";
			for (auto i=m->params.begin(); i != m->params.end(); ++i) {
				cout << *i;
				if (i != m->params.end()-1) {
					cout << ",";
				}
			}
			cout << ")";
		}
		cout << ":";
		for (CToken* t: m->body) {
			if (!t) {
				cout << " null";
			} else if (t->type == TT_ID) {
				cout << " " << *t->info.id;

			} else if (t->type == TT_PUNCTUATOR) {
				cout << " ";
				char c0 = t->info.punc & 0xFF;
				char c1 = (t->info.punc >> 8) & 0xFF;
				if (c1) cout << c1;
				cout << c0;

			} else if (t->type == TT_INT || t->type == TT_UINT || t->type == TT_LONG || t->type == TT_ULONG) {
				cout << " " << t->info.intval;

			} else if (t->type == TT_KEYWORD)
				cout << " d";
			else 
				cout << " ?";
		}
		cout << endl;
	}
}

TEST(Preprocessor, TempolaryWork)
{
	vector<CMacro*> macros;
	vector<string> include_paths = {"/usr/include", "/usr/include/x86_64-linux-gnu"};
	CPreprocessor cpp(macros, include_paths);
	cpp.preprocess("cases/000_temp.c");
//	dumptokens(cpp.lexers[0]->tokens, cpp.lexers[0]->infile.lines);
//	dumpmacros(macros);
}

TEST(Preprocessor, HelloWorld)
{
	vector<CMacro*> macros;
	vector<string> include_paths = {
		"/usr/include",
		"/usr/include/x86_64-linux-gnu",
		"/usr/lib/gcc/x86_64-linux-gnu/7/include",
		"/usr/lib/gcc/x86_64-linux-gnu/8/include",
		"/usr/lib/gcc/x86_64-linux-gnu/9/include",
		"/usr/lib/gcc/x86_64-linux-gnu/10/include",
	};
	CPreprocessor cpp(macros, include_paths);
	cpp.loadPredefined("../src/predefined.h");
	cpp.preprocess("cases/001_helloworld.c");
//	dumpmacros(macros);
	cout << "token size: " << cpp.top_tokens.size() << endl;
}

