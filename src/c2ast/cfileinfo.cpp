#include <vector>
#include <string>
#include <fstream>
#include <iostream>
#include <boost/assert.hpp>
#include <sys/stat.h>
using namespace std;
#include "cfileinfo.h"

CFileInfo::CFileInfo(string fname)
	: fname(fname)
{
	ifstream infile(fname);
	BOOST_ASSERT(!infile.fail());

	// Read all lines.
	string str;
	while (getline(infile, str)) {
		lines.push_back(str);
	}

	// Join lines that end by '\'.
	for (int n=lines.size()-2; n>=0; n--) {
		if (lines[n].back() == '\\') {
			string& line = lines[n];
			line.resize(line.size()-1);
			line += lines[n+1];
			lines[n+1] = "";
		}
	}

	// Remove last backslash.
	if (lines.back().back() == '\\') {
		string& line = lines.back();
		line.resize(line.size()-1);
	}
}

CFileInfo::CFileInfo(CFileInfo&& finfo)
	: fname(move(finfo.fname)), lines(move(finfo.lines))
{
}

string& CFileInfo::getLine(int n)
{
	BOOST_ASSERT(n > 0 && n <= lines.size());
	return lines[n-1];
}

static bool file_exists(const string& filepath)
{
	struct stat st;
	return !stat(filepath.c_str(), &st);
}

string CFileInfo::getFilePath(string filepath, string parentfile, vector<string> &searchpaths)
{
	BOOST_ASSERT(parentfile.size() && parentfile != "");
	string fpath;
	bool search_curdir = false;

	if (filepath.size() >= 3) {
		if (filepath.front() == '"' && filepath.back() == '"') {
			search_curdir = true;
		} else if (filepath.front() == '<' && filepath.back() == '>') {
		} else {
			BOOST_ASSERT(false);
		}
		fpath = filepath.substr(1, filepath.size()-2);

	} else {
		return "";
	}

	if (search_curdir) {
		BOOST_ASSERT(false);
	}

	for (string& searchpath: searchpaths) {
		const char *delim = "/";
		if (searchpath.back() == *delim) {
			delim = "";
		}
		string checkpath = searchpath + delim + fpath;
		if (file_exists(checkpath)) {
			cout << "include file:" << checkpath << endl;
			return checkpath;
		}
	}

	return "";
}

