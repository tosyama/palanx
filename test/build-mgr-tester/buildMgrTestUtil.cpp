/// palan build manager test helpers
///
/// @file buildMgrTestUtil.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

pair<int,int> parseMtraceLog(const string& traceFile) {
	string log = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = log.find("@ ", pos)) != string::npos) {
		size_t eol = log.find('\n', pos);
		string line = log.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	return {allocs, frees};
}

// Copies <base>.pa and <base>.h into out/ and compiles <base>.c there with
// gcc, so the program's `link "<base>.o"` finds the object without a build
// artifact in the source tree. Returns the program's output.
string runWithCObj(const string& base)
{
	string src = "../test/testdata/build-mgr/" + base;
	string err = execTestCommand("cp " + src + ".pa " + src + ".h out/ && gcc -c -O1 "
		+ src + ".c -o out/" + base + ".o");
	if (err != "") return err;
	return execTestCommand("bin/palan out/" + base + ".pa");
}

void expectFrameMtrace(const string& base, const string& expected)
{
	cleanTestEnv();
	string bin = "/tmp/palan_" + base + "_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/" + base + ".pa"), "");

	string traceFile = "/tmp/palan_" + base + ".log";
	// Freeing a stack address would abort here instead of printing.
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin);
	ASSERT_EQ(output, expected);

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_GT(allocs, 0);
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

void expectBalancedMtrace(const string& traceFile, int expectedAllocs)
{
	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, expectedAllocs) << "expected " << expectedAllocs << " allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

void expectMtraceRun(const string& name, const string& expected, int expectedAllocs)
{
	cleanTestEnv();
	string bin = "/tmp/palan_" + name.substr(4) + "_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/" + name + ".pa"), "");
	string traceFile = "/tmp/palan_" + name.substr(4) + ".log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin);
	ASSERT_EQ(output, expected);
	expectBalancedMtrace(traceFile, expectedAllocs);
}
