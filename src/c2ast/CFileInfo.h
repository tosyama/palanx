class CFileInfo {
public:
	string fname;
	vector<string> lines;

	CFileInfo(string fname);
	CFileInfo(CFileInfo&& fingo);
	string& getLine(int n);

	static string getFilePath(string filepath, string parentfile, vector<string> &searchpaths);
	static string searchFilePath(const string& fpath, vector<string> &searchpaths);
};

