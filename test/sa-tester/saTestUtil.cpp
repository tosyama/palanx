/// semantic analyzer test helpers
///
/// @file saTestUtil.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <fstream>
#include <regex>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

json run_sa(const string& pa_file)
{
	string ast_out = "out/test.ast.json";
	string sa_out = "out/test.sa.json";

	string gen = execTestCommand("bin/palan-gen-ast " + pa_file + " -o " + ast_out);
	if (gen != "") return json{};

	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o " + sa_out);
	if (sa != "") return json{};

	ifstream f(sa_out);
	if (!f.is_open()) return json{};
	return json::parse(f);
}

// sa.json with each unexported type named as written: the program-wide name
// hashes the module's absolute path.
json written(const json& sa)
{
	// A name inside a symbol carries its length in front, which shrinks with it.
	static const regex internal("([0-9]*)__pln_L[0-9a-f]{16}_[0-9]+_");
	string in = sa.dump(), out;
	auto pos = in.cbegin();
	for (smatch m; regex_search(pos, in.cend(), m, internal); pos = m[0].second) {
		out.append(pos, m[0].first);
		if (m[1].length() > 0)
			out += to_string(stoul(m[1]) - (m[0].length() - m[1].length()));
	}
	out.append(pos, in.cend());
	return json::parse(out);
}

// The function name a Palan link name ("module..name.types") was made from;
// a C function's name is its link name.
string funcName(const json& linkName)
{
	string s = linkName;
	size_t pos = s.rfind("..");
	size_t start = pos == string::npos ? 0 : pos + 2;
	return s.substr(start, s.find('.', start) - start);
}

// A call argument held in a temp: the call that initializes it, and the
// function that releases it.
const json& tempInit(const json& arg)
{
	return arg.at("body").at(0).at("vars").at(0).at("init");
}

string tempRelease(const json& arg)
{
	return funcName(arg.at("release").at(0).at("body").at("name"));
}

void genLibSaImport()
{
	execTestCommand("bin/palan-gen-ast ../test/testdata/sa/lib_sa_import.pa -o out/lib_sa_import.pa.ast.json");
}

void genLibSaTypes()
{
	for (string lib : {"lib_sa_types", "lib_sa_types_mid", "lib_sa_types2"})
		execTestCommand("bin/palan-gen-ast ../test/testdata/sa/" + lib + ".pa -o out/" + lib + ".pa.ast.json");
}
