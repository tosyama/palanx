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
	// compile, link, and run without crashing. Element access is deferred to IT-2506+.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/069_owned_prim_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, owned_struct_arr_field) {
	// type Point { int64 x; int64 y; }; type Cluster { [4]Point pts; }; Cluster c;
	// Declaration-only: proves __pln_alloc_Cluster/__pln_free_Cluster cascade into
	// the existing __pln_alloc_arr_Point/__pln_free_arr_Point (v0.1.24 IT-2407 asset),
	// including the forward reference from __pln_alloc_Cluster to __pln_alloc_arr_Point
	// (which is emitted later in the same generated file). Element access is
	// deferred to IT-2506+.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/070_owned_struct_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, embed_prim_arr_field_access) {
	// type Buf { [4]$int64 data; }; Buf buf; 10->buf.data[0]; 20->buf.data[1];
	// New 1D primitive embedded array field element access (IT-2507).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/071_embed_prim_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, embed_struct_arr_field_access) {
	// type Point{...}; type Polygon { [4]$Point pts; }; Polygon poly;
	// Regression test for the IT-2507 FieldAccessExpr addr-only fix: before the fix
	// this segfaulted at runtime (DerefLoad read the embedded struct's raw bytes as
	// if they were a stored pointer, instead of computing poly_ptr+offset).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/072_embed_struct_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, owned_prim_arr_field_access) {
	// type Bucket { [3]int64 vals; }; Bucket b; element read/write access (IT-2507).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/073_owned_prim_arr_field_access.pa");
	ASSERT_EQ(output, "1 2 3\n");
}

TEST(build_mgr, owned_struct_arr_field_access) {
	// type Point{...}; type Cluster { [4]Point pts; }; Cluster c; element access (IT-2507).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/074_owned_struct_arr_field_access.pa");
	ASSERT_EQ(output, "5 6\n");
}

TEST(build_mgr, embed_ptr_arr_field_access) {
	// type Point{...}; type Ring { [4]@!Point nodes; }; store then read through a
	// non-owning pointer-slot array field.
	// Regression test for the IT-2507 FieldAccessExpr addr-only fix: before the fix
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
	// struct; mktime(t) receives t as a borrowed pointer (IT-2702).
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
	// IT-2703: `@!Point view = original;` as a plain (non-field) local var decl.
	// `view` is a non-owning pointer aliasing `original`'s storage; writing
	// through `view.x` must be visible via `original.x` (same memory).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/121_at_bang_plain_var_decl.pa");
	ASSERT_EQ(output, "20 10\n");
}

TEST(build_mgr, addr_of) {
	cleanTestEnv();
	// IT-2704: `@ID`/`@!ID` address-of on a local primitive variable, passed as
	// an out-param pointer to a cincluded C function (memcpy). The second pair
	// (z = a + b) exercises addr-of on a non-literal-initialized local -- the
	// general-initializer gap that PlnRegAlloc's isVar-unification design closes.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/123_addr_of.pa");
	ASSERT_EQ(output, "42\n5\n");
}

TEST(build_mgr, time_h_category_a) {
	cleanTestEnv();
	// IT-2705: time.h Category A -- clock_t/time_t (already flattened by the
	// v0.1.26 typedef mechanism) and timer_t (a pointer-bottomed typedef chain,
	// newly flattened by this ticket's c2ast fix) both resolve cleanly, so NULL
	// type-checks against timer_t via the existing generic-pointer rule.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/124_time_h_category_a.pa");
	ASSERT_EQ(output, "366\n60.000000\n1\n-1\n-1\n");
}

TEST(build_mgr, time_h_struct_tm) {
	cleanTestEnv();
	// IT-2706: time.h Category B (struct tm) -- mktime/timegm/timelocal round
	// trip on a known epoch, strftime/asctime/asctime_r formatting. mktime
	// normalizes tm_wday as a side effect, so asctime/asctime_r (called after)
	// correctly print "Thu".
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/125_time_h_struct_tm.pa");
	ASSERT_EQ(output, "0\n0\n0\n1970-01-01\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n");
}

TEST(build_mgr, time_h_struct_timespec) {
	cleanTestEnv();
	// IT-2707: time.h Category B (struct timespec/itimerspec) -- clockid_t +
	// CLOCK_REALTIME/TIME_UTC const import used in real program logic, and
	// itimerspec's nested timespec embed fields (its.it_value.tv_sec) resolved
	// through the same embed-field chain native $T structs use. timer_gettime
	// is called with an invalid handle (timer_create is out of scope) and
	// expected to fail.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/126_time_h_struct_timespec.pa");
	ASSERT_EQ(output, "1\n1\n1\n1\n1\n1\n");
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

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
