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

// Verify that two sequential C calls each get correct argument registers.
// This exercises the multi-call path with the vector<VReg> CallC design.
// (Callee-saved register allocation is tested in the variable declaration tests.)
TEST(codegen, two_calls_arg_registers) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/003_two_calls.sa.json";
    string asmf = "out/003_two_calls.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // puts call: only %rdi
    ASSERT_NE(asm_text.find("call puts"),    string::npos);
    // printf call: %rdi (format) and %rsi (integer 99)
    ASSERT_NE(asm_text.find("movq $99,"),    string::npos);
    ASSERT_NE(asm_text.find("%rsi"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, var_decl_init) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/004_var_decl.sa.json";
    string asmf = "out/004_var_decl.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // x is initialized with 10 and passed to printf
    ASSERT_NE(asm_text.find("movq $10,"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"),     string::npos);
}

TEST(codegen, addition) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/005_addition.sa.json";
    string asmf = "out/005_addition.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq"),       string::npos);
    ASSERT_NE(asm_text.find("addq"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, convert_int32_to_int64) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/006_convert.sa.json";
    string asmf = "out/006_convert.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movslq"), string::npos);
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, var_decl_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/007_var_decl_int32.sa.json";
    string asmf = "out/007_var_decl_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int32 init must use movl, not movq
    ASSERT_NE(asm_text.find("movl $10,"),    string::npos);
    ASSERT_EQ(asm_text.find("movq $10,"),    string::npos);
    // int32 arg to printf must use 32-bit register (%edi), not 64-bit (%rdi)
    ASSERT_NE(asm_text.find("%edi"),         string::npos);
    ASSERT_EQ(asm_text.find("movq -"),       string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, addition_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/008_addition_int32.sa.json";
    string asmf = "out/008_addition_int32.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // int32 addition must use movl/addl, not movq/addq
    ASSERT_NE(asm_text.find("movl"),         string::npos);
    ASSERT_NE(asm_text.find("addl"),         string::npos);
    ASSERT_EQ(asm_text.find("addq"),         string::npos);
    ASSERT_NE(asm_text.find("call printf"),  string::npos);
}

TEST(codegen, cast_narrowing_int64_to_int32) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/009_cast_narrowing.sa.json";
    string asmf = "out/009_cast_narrowing.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movl "), string::npos);   // narrowing uses movl
    ASSERT_NE(asm_text.find("call printf"), string::npos);
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
