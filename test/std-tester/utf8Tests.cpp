/// std/utf8 tests
///
/// @file utf8Tests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <fstream>
#include "../test-base/testBase.h"

using namespace std;

TEST(std_utf8, top_level_tests) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../src/std/utf8.pa"), "");
}

TEST(std_utf8, imported_from_the_build_tree) {
	cleanTestEnv();
	ofstream("out/uses.pa") << "import <std/utf8>;\ncinclude <stdio.h>;\n"
		"utf8 s = toUtf8(\"h\\xC3\\xA9\", utf8mode.KEEP_INVALID);\ns.append(\"!\\xFF\");\n"
		"printf(\"%s %ld %ld %d\\n\", s -> cstr(), s -> byteLen(), s -> charCount(), s.hadInvalid);\n";
	ASSERT_EQ(execTestCommand("bin/palan out/uses.pa"), "h\xC3\xA9!\xFF 5 4 1\n");
}
