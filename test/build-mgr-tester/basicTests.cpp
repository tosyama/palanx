#include <gtest/gtest.h>
#include <filesystem>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;

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

TEST(build_mgr, const_decl_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/104_const_decl_basic.pa");
	ASSERT_EQ(output, "100\n101\nhello\n");
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
	// int/float mixed arithmetic: int is implicitly widened to float. Covers
	// both operand orders (float+int and int+float) and flo32/flo64 mixing --
	// usualArithConv's float tie-break is not commutative in the source code
	// path taken (left vs right operand), so both directions need a real
	// program to exercise.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/030_float_int_mixed.pa");
	ASSERT_EQ(output, "5.000000\n30.000000\n4.000000\n5.000000\n5.000000\n");
}

TEST(build_mgr, float32_cmp) {
	cleanTestEnv();
	// flo32 comparison: !=, ==, <, > (exercises ucomiss and setne)
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/031_float32_cmp.pa");
	ASSERT_EQ(output, "1\n0\n1\n0\n");
}

TEST(build_mgr, array_sprintf) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/032_array_sprintf.pa");
	ASSERT_EQ(output, "Hello, array! 2025\n");
}

TEST(build_mgr, array_mtrace) {
	cleanTestEnv();
	// Step 1: compile without LD_PRELOAD
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_array_mtrace_bin "
		"../test/testdata/build-mgr/033_array_mtrace.pa"), "");

	// Step 2: run the compiled binary with mtrace instrumentation
	string traceFile = "/tmp/palan_array_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_array_mtrace_bin");

	// Step 3: count alloc/free events and verify balance.
	// Skip allocations from shared libraries (e.g. libc stdio internal buffers)
	// since those are not freed within the mtrace window.
	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [64]uint8 buf, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, array_multi_var) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/034_array_multi_var.pa");
	ASSERT_EQ(output, "hello world\n");
}

TEST(build_mgr, array_while_break) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/035_array_while_break.pa");
	ASSERT_EQ(output, "0\n1\n");
}

TEST(build_mgr, array_while_continue) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/036_array_while_continue.pa");
	ASSERT_EQ(output, "1\n3\n5\n");
}

TEST(build_mgr, arr_index_fib) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/037_arr_index_fib.pa");
	ASSERT_EQ(output, "1\n1\n2\n3\n5\n8\n13\n21\n34\n55\n");
}

TEST(build_mgr, ptr_arr_transfer) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/038_ptr_arr_transfer.pa");
	ASSERT_EQ(output, "6\n");
}

TEST(build_mgr, 2d_array) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/039_2d_array.pa");
	ASSERT_EQ(output, "1 2 3\n4 5 6\n");
}

TEST(build_mgr, int32_index) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/041_int32_index.pa");
	ASSERT_EQ(output, "30\n10\n20\n");
}

TEST(build_mgr, logical_ops) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/040_logical_ops.pa");
	ASSERT_EQ(output, "1\n0\n1\n1\n");
}

TEST(build_mgr, embed_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/042_embed_arr.pa");
	ASSERT_EQ(output, "1\n10\n26\n");
}

TEST(build_mgr, embed_arr_int32_idx) {
	cleanTestEnv();
	// int32 expression as outer row index on the scale_expr path (variable inner dim)
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/043_embed_arr_int32_idx.pa");
	ASSERT_EQ(output, "20\n30\n");
}

TEST(build_mgr, uint_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/044_uint_literal.pa");
	ASSERT_EQ(output, "18446744073709551615\n");
}

TEST(build_mgr, block_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/045_block_import.pa");
	ASSERT_EQ(output, "4\n");
}

TEST(build_mgr, selective_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/046_selective_import.pa");
	ASSERT_EQ(output, "16\n");
}

TEST(build_mgr, alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/047_alias_import.pa");
	ASSERT_EQ(output, "9\n8\n");
}

TEST(build_mgr, selective_alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/048_selective_alias_import.pa");
	ASSERT_EQ(output, "25\n");
}

TEST(build_mgr, cinclude_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/049_cinclude_alias.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, struct_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/050_struct_basic.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, struct_c_abi) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/051_struct_c_abi.pa");
	ASSERT_EQ(output, "42 100\n");
}

TEST(build_mgr, struct_float) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/052_struct_float.pa");
	ASSERT_EQ(output, "1.5 2.5\n");
}

TEST(build_mgr, struct_scope) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/053_struct_scope.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, struct_multi) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/054_struct_multi.pa");
	ASSERT_EQ(output, "1 2 3 4\n");
}

TEST(build_mgr, owned_struct_alloc) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/055_owned_struct_alloc.pa");
	ASSERT_EQ(output, "10 20 30 40\n");
}

TEST(build_mgr, nested_owned_struct) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/056_nested_owned_struct.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, embed_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/057_embed_struct_field.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, ptr_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/058_ptr_field.pa");
	ASSERT_EQ(output, "42 100\n");
}

TEST(build_mgr, mutable_ptr_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/059_mutable_ptr_field.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, struct_func_param) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/060_struct_func_param.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, owned_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/061_owned_struct_arr.pa");
	ASSERT_EQ(output, "5 6 7 8\n");
}

TEST(build_mgr, embed_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/062_embed_struct_arr.pa");
	ASSERT_EQ(output, "30 40\n");
}

TEST(build_mgr, at_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/063_at_struct_arr.pa");
	ASSERT_EQ(output, "99 77\n");
}

TEST(build_mgr, at_bang_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/064_at_bang_struct_arr.pa");
	ASSERT_EQ(output, "42 20\n");
}

TEST(build_mgr, owned_prim_arr_field) {
	// type Bucket { [3]int64 vals; }; Bucket b;
	// Declaration-only: proves __pln_alloc_Bucket/__pln_free_Bucket and the shared
	// __pln_alloc_arr_prim_int64/__pln_free_arr_prim_int64 allocators are generated,
	// compile, link, and run without crashing. Element access is deferred to a
	// later test.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/069_owned_prim_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, owned_struct_arr_field) {
	// type Point { int64 x; int64 y; }; type Cluster { [4]Point pts; }; Cluster c;
	// Declaration-only: proves __pln_alloc_Cluster/__pln_free_Cluster cascade into
	// the existing __pln_alloc_arr_Point/__pln_free_arr_Point allocator helpers,
	// including the forward reference from __pln_alloc_Cluster to __pln_alloc_arr_Point
	// (which is emitted later in the same generated file). Element access is
	// deferred to a later test.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/070_owned_struct_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, embed_prim_arr_field_access) {
	// type Buf { [4]$int64 data; }; Buf buf; 10->buf.data[0]; 20->buf.data[1];
	// 1D primitive embedded array field element access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/071_embed_prim_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, embed_struct_arr_field_access) {
	// type Point{...}; type Polygon { [4]$Point pts; }; Polygon poly;
	// Regression test for a FieldAccessExpr addr-only fix: before the fix
	// this segfaulted at runtime (DerefLoad read the embedded struct's raw bytes as
	// if they were a stored pointer, instead of computing poly_ptr+offset).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/072_embed_struct_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, owned_prim_arr_field_access) {
	// type Bucket { [3]int64 vals; }; Bucket b; element read/write access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/073_owned_prim_arr_field_access.pa");
	ASSERT_EQ(output, "1 2 3\n");
}

TEST(build_mgr, owned_struct_arr_field_access) {
	// type Point{...}; type Cluster { [4]Point pts; }; Cluster c; element access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/074_owned_struct_arr_field_access.pa");
	ASSERT_EQ(output, "5 6\n");
}

TEST(build_mgr, embed_ptr_arr_field_access) {
	// type Point{...}; type Ring { [4]@!Point nodes; }; store then read through a
	// non-owning pointer-slot array field.
	// Regression test for a FieldAccessExpr addr-only fix: before the fix
	// the write `p -> r.nodes[0];` segfaulted (DerefLoad on the freshly-calloc'd
	// "nodes" field read back 0, collapsing the store address to NULL).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/075_embed_ptr_arr_field_access.pa");
	ASSERT_EQ(output, "99\n");
}

static pair<int,int> parseMtraceLog(const string& traceFile) {
	string log = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = log.find("@ ", pos)) != string::npos) {
		size_t eol = log.find('\n', pos);
		string line = log.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	return {allocs, frees};
}

TEST(build_mgr, owned_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/065_owned_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]Point pts: 1 malloc (ptr array) + 2 calloc (Point elements) = 3 allocs
	EXPECT_EQ(allocs, 3) << "expected 3 allocs for [2]Point pts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/066_embed_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [3]$Point pts: 1 malloc (contiguous block n*stride), 1 free
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [3]$Point pts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, at_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_at_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/067_at_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_at_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_at_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [4]@Point rpts: 1 malloc (ptr array), 1 free; Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [4]@Point rpts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, at_bang_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_at_bang_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/068_at_bang_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_at_bang_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_at_bang_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [4]@!Point wpts: 1 malloc (ptr array), 1 free; Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [4]@!Point wpts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_prim_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_prim_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/076_embed_prim_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_prim_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_prim_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Buf { [4]$int64 data; }: 1 calloc (Buf itself; data embedded in same block)
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Buf { [4]$int64 data; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_struct_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_struct_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/077_embed_struct_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_struct_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_struct_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Polygon { [4]$Point pts; }: 1 calloc (Polygon itself; pts embedded in same block)
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Polygon { [4]$Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_ptr_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_ptr_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/078_embed_ptr_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_ptr_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_ptr_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Ring { [4]@!Point nodes; }: 1 calloc (Ring itself; nodes are embedded ptr slots);
	// Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Ring { [4]@!Point nodes; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_prim_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_prim_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/079_owned_prim_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_prim_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_prim_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Bucket { [3]int64 vals; }: Bucket calloc(1) + __pln_alloc_arr_prim_int64 malloc(1) = 2
	EXPECT_EQ(allocs, 2) << "expected 2 allocs for Bucket { [3]int64 vals; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_struct_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/080_owned_struct_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Cluster { [2]Point pts; }: Cluster calloc(1) + __pln_alloc_arr_Point(2):
	// ptr-array malloc(1) + 2 element callocs = 4
	EXPECT_EQ(allocs, 4) << "expected 4 allocs for Cluster { [2]Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, mixed_owned_and_owned_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_mixed_owned_and_owned_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/081_mixed_owned_and_owned_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_mixed_owned_and_owned_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_mixed_owned_and_owned_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Mixed { Point single; [2]Point arr; }: Mixed calloc(1) + single-field
	// __pln_alloc_Point calloc(1) + arr-field __pln_alloc_arr_Point:
	// ptr-array malloc(1) + 2 element callocs = 5
	EXPECT_EQ(allocs, 5) << "expected 5 allocs for Mixed { Point single; [2]Point arr; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_and_embed_arr_mixed_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_and_embed_arr_mixed_mtrace_bin "
		"../test/testdata/build-mgr/082_owned_and_embed_arr_mixed_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_and_embed_arr_mixed_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_and_embed_arr_mixed_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Widget { [2]Point owned_pts; [3]int64 owned_vals; [2]int64 owned_more;
	//          [3]$Point tris; [4]@!Point slots; }:
	// Widget calloc(1) + owned_pts __pln_alloc_arr_Point(2): ptr-array malloc(1) +
	// 2 element callocs(2) + owned_vals __pln_alloc_arr_prim_int64 malloc(1) +
	// owned_more __pln_alloc_arr_prim_int64 malloc(1) (shared allocator, dedup'd) = 6.
	// tris (embed-arr) and slots (embed-ptr-arr) are embedded in Widget's own
	// calloc block, so they add no separate allocations.
	EXPECT_EQ(allocs, 6) << "expected 6 allocs for Widget with mixed owned/embed arr fields, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, type_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/083_type_alias.pa");
	ASSERT_EQ(output, "5\n42\n");
}

TEST(build_mgr, cinclude_typedef_size_t) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/084_cinclude_typedef_size_t.pa");
	ASSERT_EQ(output, "5\n");
}

TEST(build_mgr, null_strchr_notfound) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/105_null_strchr_notfound.pa");
	ASSERT_EQ(output, "not found\n");
}

TEST(build_mgr, null_strtok_loop) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/106_null_strtok_loop.pa");
	ASSERT_EQ(output, "a\nbb\nccc\n");
}

TEST(build_mgr, string_h_cmp) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/107_string_h_cmp.pa");
	ASSERT_EQ(output, "0\n-1\n0\n-1\n0\n0\n");
}

TEST(build_mgr, string_h_search) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/108_string_h_search.pa");
	ASSERT_EQ(output, "llo\nlo\nello\nlo\nllo\no world\nworld\nworld\n");
}

TEST(build_mgr, string_h_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/109_string_h_copy.pa");
	ASSERT_EQ(output, "foobar\n\nhi\ndup-test\n");
}

TEST(build_mgr, string_h_misc) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/110_string_h_misc.pa");
	ASSERT_EQ(output, "Success\nNo such file or directory\nInterrupt\n0\n1\n3\n4\n5\n");
}

TEST(build_mgr, string_h_len) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/111_string_h_len.pa");
	ASSERT_EQ(output, "11\n5\n2\n2\n3\n");
}

TEST(build_mgr, string_h_ncopy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/112_string_h_ncopy.pa");
	ASSERT_EQ(output, "hi\nfoobar\nhi\ndup\n");
}

TEST(build_mgr, string_h_lcmp) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/113_string_h_lcmp.pa");
	ASSERT_EQ(output, "hello 5\nfoobar 6\n5\n0\n-4\n");
}

TEST(build_mgr, string_h_mem_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/114_string_h_mem_copy.pa");
	ASSERT_EQ(output, "abc\nhello\n0\n-1\n");
}

TEST(build_mgr, string_h_mem_search) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/115_string_h_mem_search.pa");
	ASSERT_EQ(output, "llo\nhel\nbarbaz\nab\n");
}

TEST(build_mgr, string_h_b_null) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/116_string_h_b_null.pa");
	ASSERT_EQ(output, "0\n-1\nhello\n0\n0\nmemchr not found\nmemmem not found\n");
}

TEST(build_mgr, ctype_h_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/117_ctype_h_basic.pa");
	ASSERT_EQ(output, "8 0\n1024 0\n2 0\n2048 0\n512 0\n256 0\n32768 0\n16384 0\n4 0\n8192 0\n4096 0\n1 0\n1 0\na A\ni\nA a\n");
}

TEST(build_mgr, null_notfound_sweep) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/118_null_notfound_sweep.pa");
	ASSERT_EQ(output, "strstr: not found\nstrpbrk: not found\nstrchrnul: not null\nstrchrnul: []\n");
}

TEST(build_mgr, cinclude_struct_arg) {
	cleanTestEnv();
	// cinclude'd "tm" resolves through the same structDefs_ path as a native
	// struct; mktime(t) receives t as a borrowed pointer.
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/119_cinclude_struct_arg.pa");
	ASSERT_EQ(output, "946684800\n");
}

TEST(build_mgr, cinclude_struct_arg_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_cinclude_struct_arg_mtrace_bin "
		"../test/testdata/build-mgr/120_cinclude_struct_arg_mtrace.pa"), "");

	string traceFile = "/tmp/palan_cinclude_struct_arg_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_cinclude_struct_arg_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// tm t: 1 calloc; mktime(t) passes t as a borrowed pointer -- no extra
	// alloc/free from the C call itself.
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for tm t, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, at_bang_plain_var_decl) {
	cleanTestEnv();
	// `@!Point view = original;` as a plain (non-field) local var decl.
	// `view` is a non-owning pointer aliasing `original`'s storage; writing
	// through `view.x` must be visible via `original.x` (same memory).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/121_at_bang_plain_var_decl.pa");
	ASSERT_EQ(output, "20 10\n");
}

TEST(build_mgr, toplevel_call_named_return_struct) {
	cleanTestEnv();
	// A top-level statement calling a Palan function with a struct-typed
	// @!T named return used to crash palan-sa ("unknown prim type-name: Point")
	// because the pre-registered signature wasn't struct-normalized yet when the
	// top-level call resolved. Also confirms write-through via the returned
	// alias still works when the call itself is at top level.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/129_toplevel_call_named_return_struct.pa");
	ASSERT_EQ(output, "20\n");
}

TEST(build_mgr, addr_of) {
	cleanTestEnv();
	// `@ID`/`@!ID` address-of on a local primitive variable, passed as
	// an out-param pointer to a cincluded C function (memcpy). The second pair
	// (z = a + b) exercises addr-of on a non-literal-initialized local -- the
	// general-initializer gap that PlnRegAlloc's isVar-unification design closes.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/123_addr_of.pa");
	ASSERT_EQ(output, "42\n5\n");
}

TEST(build_mgr, time_h_category_a) {
	cleanTestEnv();
	// time.h Category A -- clock_t/time_t (flattened by the general typedef
	// mechanism) and timer_t (a pointer-bottomed typedef chain, flattened by
	// a c2ast fix) both resolve cleanly, so NULL type-checks against timer_t
	// via the existing generic-pointer rule.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/124_time_h_category_a.pa");
	ASSERT_EQ(output, "366\n60.000000\n1\n-1\n-1\n");
}

TEST(build_mgr, time_h_struct_tm) {
	cleanTestEnv();
	// time.h Category B (struct tm) -- mktime/timegm/timelocal round
	// trip on a known epoch, strftime/asctime/asctime_r formatting. mktime
	// normalizes tm_wday as a side effect, so asctime/asctime_r (called after)
	// correctly print "Thu".
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/125_time_h_struct_tm.pa");
	ASSERT_EQ(output, "0\n0\n0\n1970-01-01\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n");
}

TEST(build_mgr, time_h_struct_timespec) {
	cleanTestEnv();
	// time.h Category B (struct timespec/itimerspec) -- clockid_t +
	// CLOCK_REALTIME/TIME_UTC const import used in real program logic, and
	// itimerspec's nested timespec embed fields (its.it_value.tv_sec) resolved
	// through the same embed-field chain native $T structs use. timer_gettime
	// is called with an invalid handle (timer_create is out of scope) and
	// expected to fail.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/126_time_h_struct_timespec.pa");
	ASSERT_EQ(output, "1\n1\n1\n1\n1\n1\n");
}

TEST(build_mgr, time_h_category_c) {
	cleanTestEnv();
	// time.h Category C -- time/ctime/ctime_r/clock_getcpuclockid, the
	// only category depending solely on @ID/@!ID (see addr_of above) with no struct
	// interop. time(NULL)'s live return is only boundary-checked (>= 0); ctime/
	// ctime_r are exercised against a separately fixed epoch value for a
	// deterministic assertion.
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/127_time_h_category_c.pa");
	ASSERT_EQ(output, "1\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n1\n");
}

TEST(build_mgr, time_h_category_d) {
	cleanTestEnv();
	// time.h Category D -- gmtime/localtime/gmtime_r/localtime_r, the
	// convergence point of every struct/pointer mechanism above: @ID/@!ID
	// for the const time_t* input, and binding a cinclude'd struct tm* return
	// into a non-owning @!tm local. gmtime is UTC and
	// environment-independent, so also verifies its glibc static-buffer aliasing
	// (a second call overwrites the first result) to prove @!T is a real
	// non-owning alias, not a copy; gmtime_r's caller-owned buffer is unaffected.
	// localtime/localtime_r are timezone-dependent, so get a loose sanity check only.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/128_time_h_category_d.pa");
	ASSERT_EQ(output, "1970\n2 2\n1970\n1\n1\n");
}

TEST(build_mgr, at_bang_plain_var_decl_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_at_bang_plain_var_decl_mtrace_bin "
		"../test/testdata/build-mgr/122_at_bang_plain_var_decl_mtrace.pa"), "");

	string traceFile = "/tmp/palan_at_bang_plain_var_decl_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_at_bang_plain_var_decl_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// `@!Point view = original;` declares a plain non-owning pointer var:
	// no calloc/__pln_alloc_ call, and no auto-free registration at scope end.
	EXPECT_EQ(allocs, 0) << "expected no alloc for plain @!Point view, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, cinclude_arr_field_access) {
	// struct Rec { int id; char name[16]; long vals[4]; }; (cinclude'd) --
	// read/write through both the first and last element of a
	// prim-leaf array field. Before a fix, "char name[16]"/"long vals[4]" each
	// collapsed to a single scalar field, so vals[3] (the last of 4 int64 slots)
	// would have read/written 24 bytes past the field's true end -- i.e. past the
	// end of the whole struct's calloc'd block.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/130_cinclude_arr_field_access.pa");
	ASSERT_EQ(output, "5 65 90 100 400\n");
}

TEST(build_mgr, cinclude_ptr_slot_arr_field) {
	// struct Point { int x; int y; }; struct Slots { struct Point *pts[4]; long *vals[3]; };
	// (cinclude'd) -- a C struct field
	// that is an inline array of pointer slots ("T *field[n];") is Palan's
	// [n]@T / [n]@!T shape (same embed-ptr-arr layout as the native
	// embed_ptr_arr_field_access test, 075). Storing an address into the slot is
	// allowed regardless of the slot's read-only "mutable:false" default (only
	// write-through to the pointee is restricted -- see sa.field_arr_readonly_ptr_slot).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/140_cinclude_ptr_slot_arr_field.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, sys_stat_h_s_ifdir_alias) {
	// S_IFDIR is defined as `#define S_IFDIR __S_IFDIR` in sys/stat.h -- the
	// public alias name must resolve to the same value as the
	// internal macro it references, not be silently dropped from const-inlining.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/131_sys_stat_s_ifdir.pa");
	ASSERT_EQ(output, "dir-mode-ok\n");
}

TEST(build_mgr, deref_write_mutable_ptr) {
	// `p[0]` deref write through a mutable `@!T` still works end to
	// end (regression guard for the new read-only enforcement -- writes
	// through `@!T` must remain unaffected).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/132_deref_write_mutable_ptr.pa");
	ASSERT_EQ(output, "99\n99\n");
}

TEST(build_mgr, deref_c_outparam_readback) {
	// A value a cincluded C function writes through a `@!T`
	// out-param is readable from Palan itself via `p[0]` -- an earlier
	// version documented this as "C side only"; that limitation is lifted.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/133_deref_c_outparam_readback.pa");
	ASSERT_EQ(output, "1\n1\n");
}

TEST(build_mgr, deref_scalar_widths) {
	// `p[0]` read/write round-trips correctly for every scalar
	// width and float -- DerefLoadIdx/DerefStoreIdx pick the right
	// mov instruction and register class for int8/int16/int32/flo64.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/134_deref_scalar_widths.pa");
	ASSERT_EQ(output, "100\n30000\n2000000000\n3.500000\n");
}

TEST(build_mgr, deref_struct_ptr_field) {
	// `p[i]` on a pointer to a struct is an address computation
	// (Palan has no register-sized struct value), so `p[0].field` reads and
	// writes through it exactly like `p.field` on the same pointer.
	cleanTestEnv();
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/135_deref_struct_ptr_field.pa");
	ASSERT_EQ(output, "1972\n1\n");
}

TEST(build_mgr, addr_of_struct_field) {
	// `@!s.y` / `@!s.in.v` take the address of a struct field
	// (top-level and nested-embed) and hand it to a cincluded C function
	// (memcpy) as an out-param; the C-side write is read back via the
	// ordinary field-access path.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/136_addr_of_struct_field.pa");
	ASSERT_EQ(output, "99\n7\n");
}

TEST(build_mgr, addr_of_arr_elem) {
	// `@!arr[2]` takes the address of a scalar array element and
	// hands it to a cincluded C function (memcpy) as an out-param; the
	// write lands only at that element's offset, not the array start.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/137_addr_of_arr_elem.pa");
	ASSERT_EQ(output, "0 0 99 0\n");
}

TEST(build_mgr, addr_of_borrow_mtrace) {
	// Taking the address of a struct field (`@!s.x`), an array
	// element (`@!arr[2]`) and a plain local (`@!v`), then writing through
	// each via `p[0]` deref, causes no alloc/free of its own -- the address
	// is a borrow, not a new owned allocation, and the original owners
	// (Point s, [4]int64 arr) are still freed exactly once each at scope
	// exit.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_addr_of_borrow_mtrace_bin "
		"../test/testdata/build-mgr/138_addr_of_borrow_mtrace.pa"), "");

	string traceFile = "/tmp/palan_addr_of_borrow_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_addr_of_borrow_mtrace_bin");
	EXPECT_EQ(output, "11 22 33\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Point s: 1 calloc; [4]int64 arr: 1 malloc. Taking @!s.x / @!arr[2] /
	// @!v and writing through each via p[0] adds no allocation of its own.
	EXPECT_EQ(allocs, 2) << "expected 2 allocs for Point s + [4]int64 arr, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, addr_of_owned_field_borrow_mtrace) {
	// Same borrow check, but through the cascade alloc/free path --
	// `@!c.pts[0].x` takes the address of a leaf field inside an owned
	// struct-array field (Cluster.pts is [2]Point, its own
	// __pln_alloc_arr_Point/__pln_free_arr_Point pair), and the deref write
	// through it neither allocates nor disturbs that cascade's free count.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_addr_of_owned_field_borrow_mtrace_bin "
		"../test/testdata/build-mgr/139_addr_of_owned_field_borrow_mtrace.pa"), "");

	string traceFile = "/tmp/palan_addr_of_owned_field_borrow_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_addr_of_owned_field_borrow_mtrace_bin");
	EXPECT_EQ(output, "99\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Cluster c: calloc(1) + __pln_alloc_arr_Point(2): ptr-array malloc(1) +
	// 2 element callocs = 4 (same shape as owned_struct_arr_field_mtrace).
	// Taking @!c.pts[0].x adds no allocation of its own.
	EXPECT_EQ(allocs, 4) << "expected 4 allocs for Cluster c { [2]Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, sign_cross_convert) {
	// PlnX86CodeGen::emitConvert had no signed<->unsigned
	// branches at all; every case below used to abort with rc=134 instead of
	// printing. Covers the original repro plus the full cross-signedness
	// widen/narrow/reinterpret matrix (int8/16/32/64 <-> uint8/16/32/64).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/141_sign_cross_convert.pa");
	ASSERT_EQ(output,
		"7 7 7 7\n"
		"255 255 255 65535 65535 4294967295\n"
		"65535 4294967295 18446744073709551615 4294967295 18446744073709551615 18446744073709551615\n"
		"-1 -1 -1 -1\n"
		"255 65535 4294967295 18446744073709551615\n"
		"-1 -1 -1 -1 -1 -1\n"
		"255 255 255 65535 65535 4294967295\n");
}

TEST(build_mgr, uint_idx_var_stride) {
	// A uint32 row index into a [n]$[m]T array with a
	// runtime inner dimension used to abort in PlnVCodeGen's variable-stride
	// path (Uint32 -> Int64 convert before the stride multiply).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/142_uint_idx_var_stride.pa");
	ASSERT_EQ(output, "40 50 60\n");
}

TEST(build_mgr, ptr_alias_pointee) {
	// deepNormalizePrimToStruct only resolved
	// prim(Name) -> struct(Name) via structDefs_, without re-applying
	// resolveTypeAlias at each level of a pntr chain, so a Palan type alias
	// or a C typedef used as a `@T`/`@!T` pointee reached
	// PlnTypeRegistry::fromJson unresolved and aborted with rc=134 instead
	// of resolving to the underlying primitive type.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/143_ptr_alias_pointee.pa");
	ASSERT_EQ(output, "42 7\n");
}

TEST(build_mgr, uint_narrow_arith) {
	// PlnX86CodeGen's add/sub/mul/neg/cmp mnemonic tables enumerated
	// signed widths explicitly but fell through to the 64-bit default for
	// Uint8/Uint16/Uint32, while movInstrForType/sizedRegName already sized those
	// types at 8/16/32 bits — e.g. `uint32 a + uint32 b` emitted `movl` into a
	// 32-bit register followed by `addq`, which the assembler rejects. No test
	// exercised unsigned sub-64-bit arithmetic before this fix.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/144_uint_narrow_arith.pa");
	ASSERT_EQ(output, "4 240 0\n4 65520 65476 0\n4 4294967280 4294967236 0\n");
}

TEST(build_mgr, uint_lit_narrow) {
	// A uint8/16/32 variable declared directly from a `u`-suffixed
	// literal (lit-uint expr-type) deserialized to a codegen node with no type
	// field, so codegen always emitted a 64-bit MovImm regardless of the declared
	// width, and lowerVarDeclStmt never routed lit-uint through InitVar (unlike
	// lit-int/lit-flo), so the variable wasn't tracked as a stable stack-resident
	// variable by RegAlloc either -- reassignment produced mismatched instruction
	// widths. Uint64 never exposed this (Int64/Uint64 alias to the same 64-bit
	// register form); `uint8 a = 200;` (no `u` suffix, lit-int) never exposed it
	// either, since SA retypes the literal itself rather than going through
	// lit-uint's codegen path.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/145_uint_lit_narrow.pa");
	ASSERT_EQ(output, "44\n50\n4464\n12345\n14745824\n100\n");
}

TEST(build_mgr, bitwise_ops) {
	// `&` `|` `^` `~` were entirely unimplemented -- `&` parsed
	// but returned "not-impl" in SA, `|`/`^` weren't even lexed, `~` didn't exist.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/146_bitwise_ops.pa");
	ASSERT_EQ(output, "8 14 6 -13\n1\n61440 61503 63\n");
}

TEST(build_mgr, incomplete_struct_handle) {
	// `@!_IO_FILE p;` used to abort at declaration time
	// ("unknown struct type '_IO_FILE'.", from pointee validation)
	// because registerCStruct dropped the whole tag when one field
	// (glibc's "_unused2", a size-expr c2ast can't evaluate) couldn't be laid
	// out. It now registers _IO_FILE as an incomplete struct instead, so a
	// non-owning pointer declaration (no layout needed) builds and runs.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/147_incomplete_struct_handle.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, file_handle) {
	// `FILE` (typedef struct _IO_FILE FILE;) resolves
	// as a type alias for `_IO_FILE`, so fopen/fclose signatures that mention
	// it by name build and run instead of aborting at fromJson.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/148_file_handle.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, c_global_stderr) {
	// `stderr` resolves through cGlobalScopes and
	// lowers to LeaLabel+DerefLoad, so fprintf(stderr, ...) actually writes
	// to fd 2. execTestCommand appends stderr after a ":" only when stderr
	// is non-empty (test-base/testBase.cpp), so the leading ":" here is
	// itself proof the bytes went to fd 2, not fd 1.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/149_c_global_stderr.pa");
	ASSERT_EQ(output, "out\n:err\n");
}

TEST(build_mgr, stdio_text_io) {
	// End-to-end proof that stdio.h text I/O works. fputs/fprintf/fwrite write the file, then fgets/fread read
	// every byte back -- the expected string below is the file's own content
	// round-tripped through the filesystem, so no separate content check is
	// needed. `uint64 n = fread(...)` (not int64) because fread returns size_t
	// and a var-decl initializer rejects the cross-signedness narrowing.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/150_stdio_text_io.pa");
	ASSERT_EQ(output, "1:hello\n2:42 world\n3:raw n=3\nclose=0,0\n");
}

TEST(build_mgr, stdio_binary_seek) {
	// fwrite/fread on a raw [4]int64 buffer (fwrite's void*
	// parameter accepts any pntr(T)), random access via fseek/ftell, and the
	// feof/ferror indicators after a read at end-of-file. Also the repo's first
	// use of the SEEK_*/EOF constants c2ast exports from stdio.h.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/151_stdio_binary_seek.pa");
	ASSERT_EQ(output,
		"wrote=4\n"
		"size=32\n"
		"tell=16\n"
		"rec=33 n=1\n"
		"past=0 eof=1 err=0\n"
		"EOF=-1 SEEK_SET=0 SEEK_CUR=1 SEEK_END=2\n");
}

TEST(build_mgr, stdio_std_streams) {
	// stdout/stderr/stdin as cinclude'd C globals.
	// c_global_stderr above only proved stderr reaches fd 2; this adds stdout
	// and stdin. fileno gives a structural check of all three, and stdin is
	// additionally read for real -- execTestCommand runs the command through
	// popen, so the "<" redirect is honoured by the shell and inherited by the
	// binary palan runs. execTestCommand appends stderr after a ":" only when
	// it is non-empty (test-base/testBase.cpp), hence the trailing ":to-err\n".
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/152_stdio_std_streams.pa"
		" < ../test/testdata/build-mgr/152_stdio_stdin_input.txt");
	ASSERT_EQ(output, "fd out=1 err=2 in=0\nin:piped-line\nto-out\n:to-err\n");
}

TEST(build_mgr, file_handle_no_autofree_mtrace) {
	// Proves scope exit does not free a `@!FILE` handle.
	// Measured log for this program contains exactly two allocations, both
	// attributed to libc.so.6 frames (fopen64 and _IO_file_doallocate) which
	// parseMtraceLog skips, and zero deallocations. A control case -- an owned
	// `[4]int64` in the same block shape -- does produce a non-.so. alloc and a
	// matching free, so frees==0 here is a real signal and not a blind spot.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_file_handle_no_autofree_mtrace_bin "
		"../test/testdata/build-mgr/153_file_handle_no_autofree_mtrace.pa"), "");

	string traceFile = "/tmp/palan_file_handle_no_autofree_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_file_handle_no_autofree_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 0) << "fopen's allocation belongs to libc, not to Palan; got " << allocs;
	EXPECT_EQ(frees, 0) << "@!FILE must not be freed at scope exit; got " << frees << " free(s)";
}

TEST(build_mgr, call_arg_in_func_body) {
	// A Palan function parameter
	// passed as a call argument from inside the function body, outside any loop.
	// 014_param_loop_call_arg.pa pins the loop-region case; this covers the
	// straight-line case, a genuine 2-cycle swap between two parameters, and a
	// 3-arg call where a non-conflicting bystander resolves before the 2-cycle
	// among the other two -- exercising emitSafeRegMoves' cycle-break path when
	// the cycle isn't at index 0.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/154_call_arg_in_func_body.pa");
	ASSERT_EQ(output, "direct=7\nviacopy=7\ntwo=7 8\ng=22 11\ng4=1 3 2\n");
}

TEST(build_mgr, neg_lit_narrow_init) {
	// A negated literal now adopts the
	// initializer's expected type (matching the adjacent bitnot handling) instead
	// of always widening to int64/flo64 first and tripping the narrowing-
	// initializer diagnostic.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/155_neg_lit_narrow_init.pa");
	ASSERT_EQ(output, "-1 -1.500000 -200\n");
}

TEST(build_mgr, stat_file_types) {
	// stat/lstat/fstat all report the right S_IFMT bits
	// through three acquisition paths (path lookup, symlink-aware path lookup,
	// an open file descriptor), plus a nested-struct field read (st_mtim.tv_sec).
	// The mkfifo'd path is checked with lstat only -- open()'ing a FIFO with no
	// peer would hang until execTestCommand's 5-second SIGKILL timeout.
	cleanTestEnv();
	execTestCommand("rm -f /tmp/pln_156_reg.txt /tmp/pln_156_link /tmp/pln_156_fifo");
	execTestCommand("ln -s /tmp/pln_156_reg.txt /tmp/pln_156_link");
	execTestCommand("mkfifo /tmp/pln_156_fifo");
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/156_stat_file_types.pa");
	ASSERT_EQ(output,
		"dir=1\n"
		"chr=1\n"
		"reg=1 size=10 mtime_nonzero=1\n"
		"lnk=1 target_reg=1\n"
		"fifo=1\n");
}

TEST(build_mgr, stat_mode_bits) {
	// umask(0) makes mkdir's permission bits deterministic
	// regardless of the caller's inherited umask (verified under both the
	// harness's default umask and `umask 077`); chmod's bits are unaffected by
	// umask either way. Also pins constant folding on sys/stat.h's
	// expression macros (S_IRWXU, ACCESSPERMS, ...).
	cleanTestEnv();
	execTestCommand("rm -rf /tmp/pln_157_dir");
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/157_stat_mode_bits.pa");
	ASSERT_EQ(output,
		"mkdir_perm=504\n"
		"chmod_perm=488\n"
		"umask_roundtrip=0\n"
		"consts=448 56 7 511 4095\n");
}

TEST(build_mgr, usual_arith_conv) {
	// End-to-end pin for the two repro shapes
	// that used to produce bad assembly (register/operand-width mismatch)
	// because a mixed signed/unsigned operand pair silently fell through
	// typeCompat's ExplicitCast with no convert node inserted -- in a binary
	// operator (m & big) and in a call argument (uint32 -> int64 param).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/158_usual_arith_conv.pa");
	ASSERT_EQ(output, "255\n4294967295\n");
}

TEST(build_mgr, strtol_endptr) {
	// `@`/`@!` on a pointer-typed local (not just a
	// primitive one) now produces pntr-of-pntr, letting `strtol`'s C
	// out-param idiom (`char **endptr`) be written in Palan: `@!end` where
	// `end` is `@!int8` gives strtol its `int8**`, and the callee writes
	// the "abc" tail's address back through it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/159_strtol_endptr.pa");
	ASSERT_EQ(output, "42 abc\n");
}

TEST(build_mgr, struct_ret_div) {
	// End-to-end proof of SysV struct-by-value
	// return through real glibc functions -- div_t (1 eightbyte, INTEGER)
	// and ldiv_t/lldiv_t (2 eightbytes, INTEGER+INTEGER).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/160_struct_ret_div.pa");
	ASSERT_EQ(output,
		"3 1\n"
		"3 1\n"
		"3 1\n");
}

TEST(build_mgr, struct_ret_div_mtrace) {
	// The calloc backing `div_t d` must be freed exactly
	// once at scope exit -- no double-free, no leak, for a struct-ret'd
	// C-function call.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_ret_div_mtrace_bin "
		"../test/testdata/build-mgr/161_struct_ret_div_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_ret_div_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_ret_div_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for div_t d, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, alias_call_variadic_promote) {
	// Prerequisite fix: sa_expr_member_call used to
	// duplicate sa_expr_call's argument loop without the variadic-promotion
	// step, so an aliased C call silently passed a flo32 where the callee's
	// va_arg reads a flo64, producing a garbage value instead of a diagnostic
	// or the correct promotion. `printf("%f\n", f)` (no alias) already
	// promoted correctly; only the `S.printf(...)` alias form was affected.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/162_alias_call_variadic_promote.pa");
	ASSERT_EQ(output, "1.500000\n");
}

TEST(build_mgr, non_executable_stack) {
	// Prerequisite: Palan bypasses the C driver, so
	// palan-codegen must emit .note.GNU-stack itself. Without it the linked
	// program gets no PT_GNU_STACK at all (kernel default: READ_IMPLIES_EXEC),
	// and RWE as soon as any note-carrying object joins the link (e.g.
	// libc_nonshared.a's atexit) -- ld takes the union of its inputs' notes.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_gnu_stack_bin "
		"../test/testdata/build-mgr/001_helloworld.pa"), "");

	string segs = execTestCommand("readelf -lW /tmp/palan_gnu_stack_bin");
	ASSERT_NE(segs.find("GNU_STACK"), string::npos);
	ASSERT_EQ(segs.find("RWE"),       string::npos);
}

TEST(build_mgr, qsort_callback) {
	// End-to-end proof of the C-callback mechanism -- glibc's qsort calls a
	// Palan function directly through the address the
	// func-ref/LeaLabel lowering hands it. The comparator is spelled with a
	// typed pointee (@int32); bsearch_callback below covers the C-idiomatic
	// @void spelling.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/163_qsort_callback.pa");
	ASSERT_EQ(output, "1 3 4 5\n");
}

TEST(build_mgr, bsearch_callback) {
	// Same mechanism through bsearch, plus two things qsort cannot show: the
	// comparator written with C's own `const void *` signature (@void, with a
	// read-only @void -> @int32 rebinding in the body), and bsearch's `void *`
	// result bound to a typed Palan pointer -- a hit is dereferenced, a miss
	// compares equal to NULL, which proves glibc is actually consuming the
	// comparator's return value rather than merely calling it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/164_bsearch_callback.pa");
	ASSERT_EQ(output,
		"4\n"
		"1\n");
}

TEST(build_mgr, atexit_callback) {
	// A Palan function registered as a process exit handler. _start's epilogue
	// is `call exit`, so glibc's __run_exit_handlers dispatches these on the
	// way out -- two handlers prove LIFO dispatch order (C11 7.22.4.2), the
	// strongest available evidence that the Palan functions are genuinely
	// going through glibc's __cxa_atexit registry rather than being invoked
	// incidentally. Requires the entry object's own __dso_handle definition
	// (see the prereq commit). Both handlers touch only .rodata string
	// literals: _start frees its owned locals before `call exit`, so a
	// handler reading a Palan local here would be a use-after-free.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/165_atexit_callback.pa");
	ASSERT_EQ(output,
		"hello\n"
		"last\n"
		"bye\n");
}

TEST(build_mgr, limits_constants) {
	// limits.h declares zero C functions -- only object-like macro constants
	// (INT_MAX etc). Prereq bug: sa_cinclude()'s old "return if no functions"
	// early-return sat before the constants-registration block, so a
	// function-less header's constants were silently never registered.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/166_limits_constants.pa");
	ASSERT_EQ(output, "2147483647\n");
}

TEST(build_mgr, stdint) {
	// End-to-end: stdint.h declares zero C functions, only
	// typedefs (int32_t etc). Before the dedicated typedefs section, int32_t was invisible to
	// Palan entirely -- no C function/global in the header referenced it to
	// carry the typedef-name hint through the old signature-piggyback path.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/167_stdint.pa");
	ASSERT_EQ(output, "5\n");
}

TEST(build_mgr, stdint_types) {
	// A realistic multi-type scenario for stdint.h,
	// complementing 167_stdint's minimal repro -- int32_t/uint32_t/int64_t/
	// uint64_t used together, plus one cross-type (int32_t -> int64_t)
	// assignment, all from a header that declares zero C functions.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/169_stdint_types.pa");
	ASSERT_EQ(output, "1000000 2000000 300000000 400000000 1000000\n");
}

TEST(build_mgr, typedef_chain) {
	// End-to-end: a multi-level typedef chain (A -> B -> C,
	// all the way to int32), a tagged-struct-bottomed typedef (S -> struct Tag),
	// and an anonymous-struct typedef (Anon), all registered via the
	// unconditional ast.typedefs loop rather than by
	// piggybacking on a C function signature. The header also carries a
	// pointer-bottomed typedef (P) that is never referenced here -- its
	// presence proves the other typedefs still register normally even when a
	// header mixes in an excluded (pntr-bottomed) entry.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/168_typedef_chain.pa");
	ASSERT_EQ(output, "1 1 1 3 7\n");
}

TEST(build_mgr, all_headers) {
	// cinclude all 13 supported headers
	// (stdio.h, string.h, stdlib.h, time.h, math.h, ctype.h, sys/stat.h,
	// stdint.h, inttypes.h, sys/types.h, errno.h, locale.h, dirent.h)
	// simultaneously. A manual audit found zero
	// E_ConflictingTypedef diagnostics across this same set now that every
	// header's typedefs register unconditionally rather than only the ones
	// referenced by some function signature -- this pins that result down as
	// a standing regression guard instead of a one-off measurement.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/170_all_headers.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, link_math) {
	// build-mgr unions "libs" from every module's sa.json
	// and passes -l<name> to ld, so a cinclude `link` clause actually makes
	// the program linkable (sqrt lives in libm, not libc).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/171_link_math.pa");
	ASSERT_EQ(output, "1.414214\n");
}

TEST(build_mgr, link_import) {
	// This file itself has no `link` clause -- lib_sqrt.pa
	// (imported) is the one requesting libm, proving build-mgr's "libs"
	// aggregation is a whole-program union across modules, not per-file.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/172_link_import.pa");
	ASSERT_EQ(output, "11\n");
}

TEST(build_mgr, math_functions) {
	// Broad end-to-end proof that libm functions found
	// by a pre-audit (sqrt/pow/sin/cos/tan/exp/log/floor/ceil/fabs/fmod/
	// atan2/hypot) actually run through the `link` clause, including
	// passing a variable and a nested call/expression as arguments.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/173_math_functions.pa");
	ASSERT_EQ(output,
		"1.414214\n"
		"1024.000000\n"
		"0.644218\n"
		"0.764842\n"
		"0.842288\n"
		"4.481689\n"
		"2.014903\n"
		"3.000000\n"
		"4.000000\n"
		"5.500000\n"
		"1.500000\n"
		"0.643501\n"
		"5.000000\n"
		"6.250000\n"
		"5.000000\n");
}

TEST(build_mgr, output_name_injection) {
	// The -o argument used to be
	// concatenated unquoted into the ld shell command line. A shell
	// metacharacter payload proves both halves: the marker file the payload
	// would create must NOT exist (injection is closed), and a file with the
	// exact literal name must exist (the argument reached ld verbatim, not
	// truncated or mangled).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string output = execTestCommand(
		"bin/palan -o 'out/b_$(touch PWNED)' ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");
	ASSERT_FALSE(fs::exists("PWNED"));
	ASSERT_TRUE(fs::exists("out/b_$(touch PWNED)"));
}

TEST(build_mgr, source_path_injection) {
	// The input .pa path used to flow unquoted
	// through fs::weakly_canonical into every pipeline stage's command line
	// (gen-ast/sa/codegen/as/ld) plus the mirrored work directory path. Copy a
	// fixture to a hostile name at runtime (metacharacter filenames are not
	// checked into git) and confirm the whole pipeline still runs correctly.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand(
		"cp ../test/testdata/build-mgr/174_shell_metachar_source.pa 'out/inj_$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan 'out/inj_$(touch PWNED).pa'");
	ASSERT_EQ(output, "Hello World!\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, import_path_injection) {
	// An import path read back out of
	// ast.json used to be concatenated unquoted into the next palan-gen-ast
	// shell command line. A plain nonexistent metacharacter path is rejected
	// by the pre-existing fs::exists() check before ever reaching that
	// command line, so the imported file is created here with the exact
	// literal metacharacter name -- this is what actually reaches the
	// vulnerable code path pre-fix.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand("cp ../test/testdata/build-mgr/175_import_path_injection.pa out/");
	execTestCommand(
		"printf 'export func add(int32 a, int32 b) -> int32 { return a + b; }\\n' "
		"> 'out/$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan out/175_import_path_injection.pa");
	ASSERT_EQ(output, "7\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
