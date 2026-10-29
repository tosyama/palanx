/// Source file loading and include-path search for C headers.
///
/// @file CFileInfo.h
/// @copyright 2026 YAMAGUCHI Toshinobu

class CFileInfo {
public:
	string fname;
	vector<string> lines;

	CFileInfo(string fname);
	CFileInfo(string fname, istream&& in);
	CFileInfo(CFileInfo&& fingo);
	string& getLine(int n);

	static string getFilePath(string filepath, string parentfile, vector<string> &searchpaths);
	static string searchFilePath(const string& fpath, vector<string> &searchpaths);
	static bool fileExists(const string& filepath);
};

