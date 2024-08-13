typedef enum {
	TT0_ID,
	TT0_PUNCTUATOR,
	TT0_STR,
	TT0_NUMBER,
	TT0_CHAR,
	TT0_COMMENT,
	TT0_MACRO,
	TT0_MACRO_ARGS,
} CToken0Type;

class CToken0 {
public:
	CToken0Type type;
	int32_t line_no;
	int16_t pos;
	int16_t len;
	bool is_eol;

	CToken0(CToken0Type type, int line_no, int pos, int len);
};

typedef enum {
	TT_ID,
	TT_KEYWORD,
	TT_PUNCTUATOR,
	TT_STR,

	TT_INT,
	TT_UINT,
	TT_LONG,
	TT_ULONG,

	TT_FLOAT,
	TT_DOUBLE,
	TT_LDOUBLE,
	TT_INCLUDE,
} CTokenType;

typedef enum {
	TK_DEFINED,
} CTokenKeyword;

class CToken {
public:
	CTokenType type;
	int16_t lexer_no;
	int token0_no;
	union {
		string *id;
		CTokenKeyword keyword;
		int punc;
		string *str;
		vector<CToken*> *tokens;	// for TT_INCLUDE
		long intval;
		unsigned long uintval;
		float floval;
		double dblval;
		long double ldblval;
	} info;

	CToken(CTokenType type, int lexer_no, int token0_no);
	CToken(const CToken& token);
	CToken(const string& s, int lexer_no, int token0_no);
	~CToken();
};
