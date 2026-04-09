#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(build_mgr, helloworld) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "Hello World!\n");
}

TEST(build_mgr, basic_expr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/101_basic_expr.pa");
	ASSERT_EQ(output, "10\n30\n10\n100\n42\n100 100 50 50\n7\n1 1 0\n0 0 1\n");
}

TEST(build_mgr, func_def) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/102_func_def.pa");
	ASSERT_EQ(output, "7\n42\n15\n16\n99\n3 5\n5 10\n3 5\nhello\n3 5\n");
}

TEST(build_mgr, block) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/103_block.pa");
	ASSERT_EQ(output, "10 20\n10\n6\nhello from block\n14\n1 2 3\n1 2\n1\n");
}

TEST(build_mgr, import_basic) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/023_import_basic.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, import_mutual) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/024_import_mutual.pa");
	ASSERT_EQ(output, "10\n13\n");
}

TEST(build_mgr, fibonacci) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/006_fibonacci.pa");
	ASSERT_EQ(output, "55\n");
}

TEST(build_mgr, abs_gcd_lcm) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/007_abs_gcd_lcm.pa");
	ASSERT_EQ(output, "abs=42 gcd=4 lcm=12 abs=7 gcd=6 lcm=30\n");
}

TEST(build_mgr, register_spill) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/008_register_spill.pa");
	// Sub/Mul/Neg/Cmp dst spilled to stack when all callee-saved regs exhausted
	ASSERT_EQ(output, "12\n26\n2\n9\n");
}

TEST(build_mgr, seven_param_palan_func) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/009_seven_param_palan_func.pa");
	// 7th param passes on stack (RegAlloc lines 94, 155-158; X86CodeGen stack-arg path)
	ASSERT_EQ(output, "28\n");
}

TEST(build_mgr, divmod_rdx_conflict) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/010_divmod_rdx_conflict.pa");
	// vreg desired for %rdx whose live range spans a Div → conflict → callee-saved (RegAlloc lines 252-254)
	ASSERT_EQ(output, "17\n");
}

TEST(build_mgr, param_loop_call_arg) {
	cleanTestEnv();
	// Parameter n is used only as call arg inside loop (not Cmp operand).
	// Covers RegAlloc lines 172-173: call_uses loop-region check for params.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/014_param_loop_call_arg.pa");
	ASSERT_EQ(output, "42\n42\n42\n");
}

TEST(build_mgr, rdx_divmod_conflict) {
	cleanTestEnv();
	// r (CallPln result, desired %rdx) spans a Div before use as 3rd printf arg.
	// Covers RegAlloc lines 265-267: divmod conflict detection forces callee-saved.
	// Without the fix, idivq would clobber %rdx (remainder=1), giving "3 1" instead of "3 10".
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/015_rdx_divmod_conflict.pa");
	ASSERT_EQ(output, "3 10\n");
}

TEST(build_mgr, collatz) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/012_collatz.pa");
	ASSERT_EQ(output, "collatz(27) = 111\ncollatz(871) = 178\n");
}

TEST(build_mgr, fizzbuzz) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/013_fizzbuzz.pa");
	ASSERT_EQ(output, "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n11\nFizz\n13\n14\nFizzBuzz\n16\n17\nFizz\n19\nBuzz\n");
}

TEST(build_mgr, narrow_types) {
	cleanTestEnv();
	// Covers: var-decl without init, int8/int16 arithmetic (add/sub/mul/neg/cmp),
	// named single-return function with explicit bare return.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/017_narrow_types.pa");
	ASSERT_EQ(output, "0\n13 7 -3 0\n107 93 700 -7 0\n42\n");
}

TEST(build_mgr, print_primes) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/016_print_primes.pa");
	ASSERT_EQ(output, "2\n3\n5\n7\n11\n13\n17\n19\n---\n1\n2\n4\n5\n7\n8\n10\n");
}

TEST(build_mgr, float_basics) {
	cleanTestEnv();
	// flo64/flo32 var-decl/init (float and int literals), float-to-int cast for output
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/018_float_basics.pa");
	ASSERT_EQ(output, "3\n2\n5\n3\n");
}

TEST(build_mgr, float_printf) {
	cleanTestEnv();
	// flo64 variable and float literal as printf args
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/019_float_printf.pa");
	ASSERT_EQ(output, "3.140000\n2.0\n1.500000\n");
}

TEST(build_mgr, float_mixed_args) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/020_float_mixed_args.pa");
	// Case 1: interleaved int/float args (all registers)
	// Case 2: int overflow to stack, float in xmm
	// Case 3: float overflow to stack (9 floats)
	ASSERT_EQ(output,
		"10 3.500000 20\n"
		"1 2 3 4 5 6 7 0.500000\n"
		"1.000000 2.000000 3.000000 4.000000 5.000000 6.000000 7.000000 8.000000 9.000000\n");
}

TEST(build_mgr, c_float_return) {
	cleanTestEnv();
	// C function returning flo64/flo32: stored in var and used directly as arg
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/021_c_float_return.pa");
	ASSERT_EQ(output, "1.500000\n2.250000\n0.500000\n");
}

TEST(build_mgr, int_to_float_implicit) {
	cleanTestEnv();
	// int32/int64 variables assigned to flo64 without explicit cast
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/022_int_to_float_implicit.pa");
	ASSERT_EQ(output, "7.000000\n-100.000000\n");
}

TEST(build_mgr, int_convert_extra) {
	cleanTestEnv();
	// int8/16 widening and int32/16->int8 narrowing
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/023_int_convert_extra.pa");
	ASSERT_EQ(output, "100 100 300 -56 44\n");
}

TEST(build_mgr, float32_convert) {
	cleanTestEnv();
	// flo64<->flo32, flo32->int, int->flo32, int8/16->flo32/64
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/024_float32_convert.pa");
	ASSERT_EQ(output, "3 3 3\n10.000000 5.000000 12.000000\n5.000000\n42.000000\n");
}

TEST(build_mgr, uint_convert) {
	cleanTestEnv();
	// uint widening/narrowing and uint->float implicit
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/025_uint_convert.pa");
	ASSERT_EQ(output, "200 200 200\n200\n200.000000 200.000000\n300.000000 70000.000000\n");
}

TEST(build_mgr, float_arith) {
	cleanTestEnv();
	// flo64 and flo32 arithmetic: +, -, *, /
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/026_float_arith.pa");
	ASSERT_EQ(output, "5.000000 1.000000 6.000000 1.500000\n5.000000 1.000000 6.000000 1.500000\n");
}

TEST(build_mgr, float_cmp) {
	cleanTestEnv();
	// flo64 comparison: <, >, ==
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/027_float_cmp.pa");
	ASSERT_EQ(output, "1\n0\n0\n");
}

TEST(build_mgr, float_neg) {
	cleanTestEnv();
	// flo64 unary negation: literal neg and variable neg via ->
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/028_float_neg.pa");
	ASSERT_EQ(output, "-3.500000\n-2.000000\n");
}

TEST(build_mgr, float_newton) {
	cleanTestEnv();
	// Newton's method sqrt(2) using float arith, neg, if, while
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/029_float_newton.pa");
	ASSERT_EQ(output, "sqrt(2) = 1.414214\n");
}

TEST(build_mgr, float_int_mixed) {
	cleanTestEnv();
	// int/float mixed arithmetic: int is implicitly widened to float
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/030_float_int_mixed.pa");
	ASSERT_EQ(output, "5.000000\n30.000000\n4.000000\n");
}

TEST(build_mgr, float32_cmp) {
	cleanTestEnv();
	// flo32 comparison: !=, ==, <, > (exercises ucomiss and setne)
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/031_float32_cmp.pa");
	ASSERT_EQ(output, "1\n0\n1\n0\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
