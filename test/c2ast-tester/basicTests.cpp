#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, basic_tests) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/000_temp_c_header.h");
    ASSERT_EQ(output, "int testproc(){H((A+H(1)));123;return xSz;}");
}

TEST(c2ast, va_args) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/001_va_args.h");
    ASSERT_EQ(output, "int testproc(){mylog(\"hello\",1,2);}");
}

TEST(c2ast, token_paste_nonid) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/002_token_paste.h");
    ASSERT_EQ(output, "int testproc(){return val1;return 1val;return 12;return int_t;}");
}

TEST(c2ast, std_headers) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -ds stdio.h");
	ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}

TEST(c2ast, pragma_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/003_pragma_once_main.h");
    ASSERT_EQ(output, "int result=1;");
}
TEST(c2ast, char_const_in_if) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/004_char_const.h");
    ASSERT_EQ(output, "int char_result=1;");
}

TEST(c2ast, token_paste_keyword) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/005_token_paste_kw.h");
    ASSERT_EQ(output, "int x;");
}

TEST(c2ast, stringizing) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/007_stringify.h");
    ASSERT_EQ(output, "int n=\"hello\";char *s=\"1 + 2\";");
}

TEST(c2ast, warning_directive) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/006_warning.h");
    ASSERT_TRUE(output.find("int warning_test=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: this is a test warning") != string::npos);
}

TEST(c2ast, printf_in_ast) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s stdio.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    bool found = false;
    for (auto& f : functions) {
        if (f["name"] == "printf" && f["func-type"] == "c") {
            found = true;
            break;
        }
    }
	cout << output;
    ASSERT_TRUE(found);
}

TEST(c2ast, include_macro) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/008_include_macro.h");
    ASSERT_EQ(output, "int macro_include_result=1;");
}

TEST(c2ast, include_next_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/010_include_next.h");
    ASSERT_TRUE(output.find("int include_next_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: '#include_next' is not supported, ignored") != string::npos);
}

TEST(c2ast, line_directive_warning_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/011_line_directive.h");
    ASSERT_TRUE(output.find("int line_directive_result=1;") != string::npos);
    // warning appears exactly once despite two #line directives
    size_t first = output.find("warning: '#line' is not supported");
    ASSERT_NE(first, string::npos);
    ASSERT_EQ(output.find("warning: '#line' is not supported", first + 1), string::npos);
}

TEST(c2ast, unknown_directive_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/012_unknown_directive.h");
    ASSERT_TRUE(output.find("int unknown_dir_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: unsupported directive '#ident'") != string::npos);
}

TEST(c2ast, include_macro_bracket) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/009_include_macro_bracket.h");
    ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}
