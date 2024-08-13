class CLexer {
public:
	int no;
	CFileInfo infile;
	vector<CToken0> tokens;

	CLexer(CFileInfo &infile);
	vector<CToken0>& scan();

	CToken* createToken(int n);
	CToken* createIfMacroToken(int n);

	string get_str(CToken0* t0);
	int get_ch(CToken0* t0);
	string get_oristr(int token0_no);
	string get_oristr(const CToken *start, const CToken *end);
};
