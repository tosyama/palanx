#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include "../test-base/testBase.h"

using namespace std;

static string run_codegen(const string& sa_file, const string& asm_out)
{
    return execTestCommand("bin/palan-codegen " + sa_file + " -o " + asm_out);
}

static string readFile(const string& path)
{
    ifstream f(path);
    ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

TEST(codegen, printf_int_literal) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/002_printf_int_literal.sa.json";
    string asmf = "out/002_printf_int_literal.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq $42,"),    string::npos);
    ASSERT_NE(asm_text.find("%rdi"),         string::npos);
    ASSERT_NE(asm_text.find("%rsi"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, helloworld_asm_output) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/001_helloworld.sa.json";
    string asmf = "out/001_helloworld.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find(".rodata"),        string::npos);
    ASSERT_NE(asm_text.find(".str0"),          string::npos);
    ASSERT_NE(asm_text.find("Hello World!\\n"), string::npos);
    ASSERT_NE(asm_text.find(".text"),          string::npos);
    ASSERT_NE(asm_text.find("_start"),         string::npos);
    ASSERT_NE(asm_text.find("%rdi"),           string::npos);
    ASSERT_NE(asm_text.find("call printf"),    string::npos);
    ASSERT_NE(asm_text.find("call exit"),      string::npos);
}
