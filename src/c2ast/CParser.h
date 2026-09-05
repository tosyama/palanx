#include <string>
#include <map>
#include <set>
#include "../../lib/json/single_include/nlohmann/json.hpp"
using json = nlohmann::json;

class CPreprocessor;

class CParser {
	const vector<CToken*> &top_tokens;
	const vector<CLexer*> &lexers;
	map<string, json> typedefs_;  // typedef name -> resolved var-type (prim/pntr only)
	set<string> definedStructs_;  // struct tag names captured with a full field-bearing definition

	int parse(json &ast, const vector<CToken*>& tokens);
	
	bool declaration(json &ast, const vector<CToken*> &tokens, int &index, bool is_top_level);
	bool declaration_specifiers(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool declarator(json &ast, const vector<CToken*> &tokens, int &result_index, bool is_typeonly);
	bool declarator_tail(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool parameter_list(vector<json> &params, const vector<CToken*> &tokens, int &result_index);
	bool struct_union_definition(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool enum_definition(json &ast, const vector<CToken*> &tokens, int &result_index);

	bool statement(json &ast, const vector<CToken*> &tokens, int &result_index);
	bool jump_statement(json &ast, const vector<CToken*> &tokens, int &result_index);

	// This subtree (primary_expression .. expression) builds a value-AST node into
	// `value` for the small set of forms we can compute (literal, +/-, cast); otherwise
	// `value` is left null. See CParser.cpp for the "expr-type" node shapes used.
	bool primary_expression(json &value, const vector<CToken*> &tokens, int &index);
	bool postfix_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool unary_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool cast_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool multiplicative_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool additive_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool shift_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool relational_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool equality_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool and_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool exclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool inclusive_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool logical_and_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool logical_or_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool conditional_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool constant_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool assignment_expression(json &value, const vector<CToken*> &tokens, int &result_index);
	bool expression(json &value, const vector<CToken*> &tokens, int &result_index);

	bool resolveConstValue(const json &node, json &value, json &type);

	// void debug_token(const CToken* token);

public:
	CParser(const vector<CToken*> &top_tokens, const vector<CLexer*> &lexers);
	int parse(json &ast);
	void exportMacroConstants(json &ast, const vector<CMacro*> &macros, CPreprocessor &cpp);
};
