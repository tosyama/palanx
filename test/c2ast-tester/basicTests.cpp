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
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/001_va_args.h");
    ASSERT_EQ(output, "int testproc(){mylog(\"hello\",1,2);}");
}

TEST(c2ast, token_paste_nonid) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/002_token_paste.h");
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
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/003_pragma_once_main.h");
    ASSERT_EQ(output, "int result=1;");
}
TEST(c2ast, char_const_in_if) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/004_char_const.h");
    ASSERT_EQ(output, "int char_result=1;");
}

TEST(c2ast, token_paste_keyword) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/005_token_paste_kw.h");
    ASSERT_EQ(output, "int x;");
}

TEST(c2ast, stringizing) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/007_stringify.h");
    ASSERT_EQ(output, "int n=\"hello\";char *s=\"1 + 2\";");
}

TEST(c2ast, warning_directive) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/006_warning.h");
    ASSERT_TRUE(output.find("int warning_test=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: this is a test warning") != string::npos);
}

TEST(c2ast, stdio_functions_in_ast) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s stdio.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name && f["func-type"] == "c") return &f;
        return nullptr;
    };

    // printf: int printf(const char *format, ...)
    {
        json* pf = find_func("printf");
        ASSERT_NE(pf, nullptr);
        // ret-type: int
        ASSERT_EQ((*pf)["ret-type"]["type-kind"], "prim");
        ASSERT_EQ((*pf)["ret-type"]["type-name"], "int32");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 2u);
        ASSERT_EQ(params.back()["name"], "...");
        // first param: const char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        ASSERT_EQ(p0vt["base-type"]["const"], true);
    }

    // fgets: char *fgets(char *s, int n, FILE *stream)
    {
        json* pf = find_func("fgets");
        ASSERT_NE(pf, nullptr);
        // ret-type: char *
        auto& ret = (*pf)["ret-type"];
        ASSERT_EQ(ret["type-kind"], "pntr");
        ASSERT_EQ(ret["base-type"]["type-kind"], "prim");
        ASSERT_EQ(ret["base-type"]["type-name"], "int8");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 3u);
        // first param: char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        // second param: int
        auto& p1vt = params[1]["var-type"];
        ASSERT_EQ(p1vt["type-kind"], "prim");
        ASSERT_EQ(p1vt["type-name"], "int32");
    }
}

TEST(c2ast, include_macro) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/008_include_macro.h");
    ASSERT_EQ(output, "int macro_include_result=1;");
}

TEST(c2ast, include_next_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/010_include_next.h");
    ASSERT_TRUE(output.find("int include_next_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: '#include_next' is not supported, ignored") != string::npos);
}

TEST(c2ast, line_directive_warning_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/011_line_directive.h");
    ASSERT_TRUE(output.find("int line_directive_result=1;") != string::npos);
    // warning appears exactly once despite two #line directives
    size_t first = output.find("warning: '#line' is not supported");
    ASSERT_NE(first, string::npos);
    ASSERT_EQ(output.find("warning: '#line' is not supported", first + 1), string::npos);
}

TEST(c2ast, unknown_directive_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/012_unknown_directive.h");
    ASSERT_TRUE(output.find("int unknown_dir_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: unsupported directive '#ident'") != string::npos);
}

TEST(c2ast, include_macro_bracket) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/009_include_macro_bracket.h");
    ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}
