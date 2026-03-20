#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include "../test-base/testBase.h"
#include "../../src/codegen/PlnVProg.h"
#include "../../src/codegen/PlnRegAlloc.h"

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

TEST(codegen, uint_lit) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/010_uint_lit.sa.json";
    string asmf = "out/010_uint_lit.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("movq $42,"), string::npos);  // lit-uint emits Int64 movq
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, ccall_as_expr) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/011_ccall_as_expr.sa.json";
    string asmf = "out/011_ccall_as_expr.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("call abs"),    string::npos);  // inner CCCall emitted
    ASSERT_NE(asm_text.find("call printf"), string::npos);  // outer call emitted
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

TEST(codegen, callpln_retpln_regalloc) {
    static const PhysRegs testPhysRegs = {
        {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"},  // intArgs
        {},                                                // floatArgs
        {"%rbx", "%r12", "%r13", "%r14", "%r15"},         // calleeSaved
    };

    // Simulate: caller calls add(v0=1, v1=2) -> v2, then returns v2
    VFunc f;
    f.name    = "caller";
    f.isEntry = false;
    f.instrs.push_back(MovImm{0, VRegType::Int32, 1});
    f.instrs.push_back(MovImm{1, VRegType::Int32, 2});
    f.instrs.push_back(CallPln{"add", {0, 1}, {2}, {VRegType::Int32}});
    f.instrs.push_back(RetPln{{2}, {VRegType::Int32}});

    RegAllocResult ra = allocateRegisters(f, testPhysRegs);
    const RegMap& rm  = ra.regMap;

    // v0 and v1 should be assigned to arg registers (rdi, rsi)
    ASSERT_TRUE(rm.count(0));
    EXPECT_EQ(rm.at(0).base, "%rdi");
    ASSERT_TRUE(rm.count(1));
    EXPECT_EQ(rm.at(1).base, "%rsi");

    // v2 is defined by CallPln and used by RetPln — must be allocated
    ASSERT_TRUE(rm.count(2));

    // frame size for non-entry must be a multiple of 16
    EXPECT_EQ(ra.frameSize % 16, 0);
}

TEST(codegen, palan_func_simple) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/012_palan_func_simple.sa.json";
    string asmf = "out/012_palan_func_simple.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("add:"),        string::npos);  // function label emitted
    ASSERT_NE(asm_text.find("call add"),    string::npos);  // caller invokes add
    ASSERT_NE(asm_text.find("ret"),         string::npos);  // ret instruction present
    ASSERT_NE(asm_text.find("call printf"), string::npos);
}

TEST(codegen, assign_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/013_assign.sa.json";
    string asmf = "out/013_assign.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("passthrough:"),  string::npos);  // function label emitted
    ASSERT_NE(asm_text.find("call passthrough"), string::npos);  // caller invokes passthrough
    ASSERT_NE(asm_text.find("movl $0,"),      string::npos);  // dead InitVar for y still emitted
    ASSERT_NE(asm_text.find("ret"),           string::npos);
    ASSERT_NE(asm_text.find("call printf"),   string::npos);
}

// Regression: named return with narrowing (Int64 -> Int32) must insert a Convert in
// sa_assign_stmt, and callee-saved registers used inside the function must be saved and
// restored in the prologue/epilogue.
TEST(codegen, named_ret_narrowing) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/014_named_ret_narrowing.sa.json";
    string asmf = "out/014_named_ret_narrowing.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("test:"),          string::npos);  // function label
    ASSERT_NE(asm_text.find("call test"),       string::npos);  // caller invokes test
    // Callee-saved register %rbx must be saved in prologue and restored before ret
    ASSERT_NE(asm_text.find("movq %rbx, -8(%rbp)"),   string::npos);  // save
    ASSERT_NE(asm_text.find("movq -8(%rbp), %rbx"),   string::npos);  // restore
    ASSERT_NE(asm_text.find("ret"),            string::npos);
}

// Regression: when callee-saved registers are exhausted by temporaries, an Add result is
// spilled to a stack slot; X86CodeGen must emit a memory-destination Add instruction.
TEST(codegen, named_ret_double_assign) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/015_named_ret_double_assign.sa.json";
    string asmf = "out/015_named_ret_double_assign.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("test:"),        string::npos);
    ASSERT_NE(asm_text.find("call test"),    string::npos);
    // All 5 callee-saved registers must be saved/restored
    ASSERT_NE(asm_text.find("movq %rbx, -8(%rbp)"),   string::npos);
    ASSERT_NE(asm_text.find("movq -8(%rbp), %rbx"),   string::npos);
    ASSERT_NE(asm_text.find("movq %r15, -40(%rbp)"),  string::npos);
    ASSERT_NE(asm_text.find("movq -40(%rbp), %r15"),  string::npos);
    // Add with memory destination: the spilled Add result is stored directly to stack
    ASSERT_NE(asm_text.find("addq %r15, -56(%rbp)"),  string::npos);
    ASSERT_NE(asm_text.find("ret"),          string::npos);
}

TEST(codegen, multiret) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/016_multiret.sa.json";
    string asmf = "out/016_multiret.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    ASSERT_NE(asm_text.find("sumsOf:"),       string::npos);  // function label
    ASSERT_NE(asm_text.find("call sumsOf"),   string::npos);  // caller invokes sumsOf
    ASSERT_NE(asm_text.find("ret"),           string::npos);  // ret instruction
    ASSERT_NE(asm_text.find("call printf"),   string::npos);
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

TEST(codegen, block_stmt) {
    cleanTestEnv();
    string sa   = "../test/testdata/codegen/017_block.sa.json";
    string asmf = "out/017_block.s";

    string err = run_codegen(sa, asmf);
    ASSERT_EQ(err, "");

    string asm_text = readFile(asmf);
    // block generates valid asm: printf call and exit
    ASSERT_NE(asm_text.find("call printf"), string::npos);
    ASSERT_NE(asm_text.find("call exit"),   string::npos);
    // no extra block-related assembly instructions leaked
    ASSERT_EQ(asm_text.find("BlockEnter"),  string::npos);
    ASSERT_EQ(asm_text.find("BlockLeave"),  string::npos);
}
