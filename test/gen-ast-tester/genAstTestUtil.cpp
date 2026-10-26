/// gen-ast test helpers
///
/// @file genAstTestUtil.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include "genAstTestUtil.h"

using namespace std;

bool checkerr(string &output) {
	if (output[0] != '{') {
		cout << output;
		return false;
	}
	return true;
}
