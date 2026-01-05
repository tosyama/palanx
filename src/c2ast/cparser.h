#include "../../lib/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

class CParser {
	const vector<CToken*> &top_tokens;
	const vector<CLexer*> &lexers;

	int parse(json &ast, const vector<CToken*>& tokens);
	bool declaration(json &ast, const vector<CToken*> &tokens, int &index);
	bool declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool declarator(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_typeonly);
	bool declarator_tail(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index);

	void debug_token(const CToken* token);

public:
	CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers);
	int parse(json &ast);
};
