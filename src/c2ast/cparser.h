#include "../../lib/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

class CParser {
	const vector<CToken*> &top_tokens;
	const vector<CLexer*> &lexers;

	int parse(json &ast, const vector<CToken*>& tokens);
	
	bool primary_expression(json &ast, const vector<CToken*> &tokens, int &index);
	bool postfix_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool unary_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool cast_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool multiplicative_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool additive_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool shift_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool relational_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool equality_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool and_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool exclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool inclusive_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool logical_and_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool logical_or_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool conditional_expression(json &ast, const vector<CToken*> &tokens, int &result_index);

	bool constant_expression(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool declaration(json &ast, const vector<CToken*> &tokens, int &index);
	bool declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool declarator(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_typeonly);
	bool declarator_tail(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool parameter_list(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index);

	void debug_token(const CToken* token);

public:
	CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers);
	int parse(json &ast);
};
