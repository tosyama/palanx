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
	// Parameter n is used only as call arg inside loop (not Cmp operand), so
	// only the loop extension of its live range makes it cross the call.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/014_param_loop_call_arg.pa");
	ASSERT_EQ(output, "42\n42\n42\n");
}

TEST(build_mgr, loop_live_range) {
	// Values defined before a loop must stay live until its backward jump:
	// a spilled param's stack slot and a call-arg-only local were both
	// clobbered on the second iteration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/210_loop_live_range.pa");
	ASSERT_EQ(output, "f=60\nf=60\nf=60\nk=7\nk=7\nk=7\n");
}

TEST(build_mgr, rdx_divmod_conflict) {
	cleanTestEnv();
	// r (CallPln result, desired %rdx) spans a Div before use as 3rd printf arg;
	// without the conflict check idivq would clobber %rdx, giving "3 1" not "3 10".
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
	// Covers both operand orders: usualArithConv's float tie-break is not
	// commutative in the code path taken (left vs right operand).
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

	// Skip allocations from shared libraries (e.g. libc stdio buffers): not
	// freed within the mtrace window.
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
	// Bucket { [3]int64 vals; }, declaration-only: proves the shared
	// __pln_alloc_arr_prim_int64 allocator is generated and runs without crashing.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/069_owned_prim_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, owned_struct_arr_field) {
	// Cluster { [4]Point pts; }, declaration-only: proves __pln_alloc_Cluster's
	// forward reference to __pln_alloc_arr_Point (emitted later in the same file) resolves.
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
	// Regression: DerefLoad used to read the embedded struct's raw bytes as a
	// stored pointer instead of computing poly_ptr+offset, segfaulting at runtime.
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
	// Regression: DerefLoad on the freshly-calloc'd "nodes" field read back 0,
	// collapsing `p -> r.nodes[0];`'s store address to NULL.
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
	// 6 = Widget calloc(1) + owned_pts arr-alloc(1 ptr-array + 2 elements) +
	// owned_vals/owned_more (shared prim-int64 allocator, dedup'd = 2 more).
	// tris/slots are embedded in Widget's own calloc block: no extra allocs.
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
	// `view` is a non-owning pointer aliasing `original`'s storage; writing
	// through `view.x` must be visible via `original.x` (same memory).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/121_at_bang_plain_var_decl.pa");
	ASSERT_EQ(output, "20 10\n");
}

TEST(build_mgr, toplevel_call_named_return_struct) {
	cleanTestEnv();
	// Regression: a top-level call with a struct-typed @!T named return crashed
	// palan-sa ("unknown prim type-name: Point") before signature struct-normalization.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/129_toplevel_call_named_return_struct.pa");
	ASSERT_EQ(output, "20\n");
}

TEST(build_mgr, addr_of) {
	cleanTestEnv();
	// Second pair (z = a + b) exercises addr-of on a non-literal-initialized
	// local -- the gap PlnRegAlloc's isVar-unification design closes.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/123_addr_of.pa");
	ASSERT_EQ(output, "42\n5\n");
}

TEST(build_mgr, time_h_category_a) {
	cleanTestEnv();
	// timer_t is a pointer-bottomed typedef chain (flattened by a c2ast fix),
	// so NULL type-checks against it via the generic-pointer rule.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/124_time_h_category_a.pa");
	ASSERT_EQ(output, "366\n60.000000\n1\n-1\n-1\n");
}

TEST(build_mgr, time_h_struct_tm) {
	cleanTestEnv();
	// mktime normalizes tm_wday as a side effect, so asctime/asctime_r
	// (called after) correctly print "Thu".
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/125_time_h_struct_tm.pa");
	ASSERT_EQ(output, "0\n0\n0\n1970-01-01\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n");
}

TEST(build_mgr, time_h_struct_timespec) {
	cleanTestEnv();
	// itimerspec's nested timespec fields (its.it_value.tv_sec) resolve through
	// the same embed-field chain native $T structs use. timer_gettime is called
	// with an invalid handle (timer_create is out of scope) and expected to fail.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/126_time_h_struct_timespec.pa");
	ASSERT_EQ(output, "1\n1\n1\n1\n1\n1\n");
}

TEST(build_mgr, time_h_category_c) {
	cleanTestEnv();
	// time(NULL)'s live return is only boundary-checked (>= 0); ctime/ctime_r
	// are exercised against a separately fixed epoch value for determinism.
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/127_time_h_category_c.pa");
	ASSERT_EQ(output, "1\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n1\n");
}

TEST(build_mgr, time_h_category_d) {
	cleanTestEnv();
	// gmtime's glibc static-buffer aliasing (2nd call overwrites the 1st result)
	// proves @!T is a real non-owning alias, not a copy; gmtime_r's caller-owned
	// buffer is unaffected. localtime/localtime_r are timezone-dependent, so get
	// only a loose sanity check.
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
	// Regression: "char name[16]"/"long vals[4]" each used to collapse to a
	// single scalar field, so vals[3] read/wrote 24 bytes past the field's
	// true end -- past the whole struct's calloc'd block.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/130_cinclude_arr_field_access.pa");
	ASSERT_EQ(output, "5 65 90 100 400\n");
}

TEST(build_mgr, cinclude_ptr_slot_arr_field) {
	// A C "T *field[n];" is Palan's [n]@T/[n]@!T shape. Storing an address into
	// the slot is allowed despite the slot's read-only "mutable:false" default
	// (only write-through to the pointee is restricted).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/140_cinclude_ptr_slot_arr_field.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, sys_stat_h_s_ifdir_alias) {
	// S_IFDIR is `#define S_IFDIR __S_IFDIR`: the alias must resolve to the
	// same value as the macro it references, not be dropped from const-inlining.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/131_sys_stat_s_ifdir.pa");
	ASSERT_EQ(output, "dir-mode-ok\n");
}

TEST(build_mgr, deref_write_mutable_ptr) {
	// Regression guard for the new read-only enforcement: writes through
	// `@!T` must remain unaffected.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/132_deref_write_mutable_ptr.pa");
	ASSERT_EQ(output, "99\n99\n");
}

TEST(build_mgr, deref_c_outparam_readback) {
	// A C out-param write through `@!T` is readable via `p[0]` -- an earlier
	// version documented this as "C side only"; that limitation is lifted.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/133_deref_c_outparam_readback.pa");
	ASSERT_EQ(output, "1\n1\n");
}

TEST(build_mgr, deref_scalar_widths) {
	// DerefLoadIdx/DerefStoreIdx must pick the right mov instruction and
	// register class per width: int8/int16/int32/flo64.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/134_deref_scalar_widths.pa");
	ASSERT_EQ(output, "100\n30000\n2000000000\n3.500000\n");
}

TEST(build_mgr, deref_struct_ptr_field) {
	// `p[i]` on a struct pointer is an address computation (no register-sized
	// struct value), so `p[0].field` behaves exactly like `p.field`.
	cleanTestEnv();
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/135_deref_struct_ptr_field.pa");
	ASSERT_EQ(output, "1972\n1\n");
}

TEST(build_mgr, addr_of_struct_field) {
	// `@!s.y` / `@!s.in.v` (top-level and nested-embed field addresses) feed
	// a C out-param (memcpy); the write reads back via ordinary field access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/136_addr_of_struct_field.pa");
	ASSERT_EQ(output, "99\n7\n");
}

TEST(build_mgr, addr_of_arr_elem) {
	// `@!arr[2]` fed to a C out-param (memcpy): the write must land at that
	// element's offset, not the array start.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/137_addr_of_arr_elem.pa");
	ASSERT_EQ(output, "0 0 99 0\n");
}

TEST(build_mgr, addr_of_borrow_mtrace) {
	// @!s.x / @!arr[2] / @!v are borrows, not new owned allocations: taking
	// and writing through them adds no alloc, and the original owners
	// (Point s, [4]int64 arr) are still freed exactly once each.
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
	// Same borrow check through the cascade alloc/free path: `@!c.pts[0].x`
	// (a leaf field inside Cluster.pts's own alloc/free pair) adds no allocation.
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
	// Cluster c: calloc(1) + __pln_alloc_arr_Point(2) = 4 (same shape as
	// owned_struct_arr_field_mtrace). Taking @!c.pts[0].x adds no allocation.
	EXPECT_EQ(allocs, 4) << "expected 4 allocs for Cluster c { [2]Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, sign_cross_convert) {
	// Regression: emitConvert had no signed<->unsigned branches, so every case
	// below used to abort with rc=134. Covers the full cross-signedness matrix.
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
	// Regression: a uint32 row index into a [n]$[m]T array with a runtime
	// inner dimension used to abort before the Uint32->Int64 stride convert.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/142_uint_idx_var_stride.pa");
	ASSERT_EQ(output, "40 50 60\n");
}

TEST(build_mgr, ptr_alias_pointee) {
	// Regression: deepNormalizePrimToStruct didn't re-apply resolveTypeAlias at
	// each pntr-chain level, so a type alias/typedef used as a `@T`/`@!T`
	// pointee reached fromJson unresolved and aborted with rc=134.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/143_ptr_alias_pointee.pa");
	ASSERT_EQ(output, "42 7\n");
}

TEST(build_mgr, uint_narrow_arith) {
	// Regression: arithmetic mnemonic tables fell through to the 64-bit default
	// for Uint8/16/32 while sizedRegName already sized them narrower -- e.g.
	// `uint32 a + uint32 b` emitted `movl` into a 32-bit reg then `addq`, which
	// the assembler rejects.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/144_uint_narrow_arith.pa");
	ASSERT_EQ(output, "4 240 0\n4 65520 65476 0\n4 4294967280 4294967236 0\n");
}

TEST(build_mgr, uint_lit_narrow) {
	// Regression: a `u`-suffixed literal (lit-uint) deserialized with no type
	// field, so codegen always emitted a 64-bit MovImm regardless of the
	// declared width, and lowerVarDeclStmt never routed it through InitVar --
	// reassignment produced mismatched widths. Uint64 and unsuffixed literals
	// (lit-int, SA-retyped) never exposed this.
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
	// Regression: registerCStruct used to drop the whole tag when one field
	// (glibc's "_unused2", a size-expr c2ast can't evaluate) couldn't be laid
	// out. It now registers _IO_FILE as incomplete, so a non-owning pointer
	// declaration (no layout needed) builds and runs.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/147_incomplete_struct_handle.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, file_handle) {
	// FILE (`typedef struct _IO_FILE FILE;`) resolves as an alias for
	// _IO_FILE, so fopen/fclose signatures build instead of aborting at fromJson.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/148_file_handle.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, c_global_stderr) {
	// fprintf(stderr, ...) must actually write to fd 2. execTestCommand appends
	// stderr after a ":" only when non-empty, so the leading ":" here is itself
	// proof the bytes went to fd 2, not fd 1.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/149_c_global_stderr.pa");
	ASSERT_EQ(output, "out\n:err\n");
}

TEST(build_mgr, stdio_text_io) {
	// The expected string is the file's own content round-tripped through the
	// filesystem. `uint64 n = fread(...)`: fread returns size_t and a var-decl
	// initializer rejects cross-signedness narrowing.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/150_stdio_text_io.pa");
	ASSERT_EQ(output, "1:hello\n2:42 world\n3:raw n=3\nclose=0,0\n");
}

TEST(build_mgr, stdio_binary_seek) {
	// fwrite's void* parameter accepts any pntr(T). Also the repo's first use
	// of the SEEK_*/EOF constants c2ast exports from stdio.h.
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
	// stdin is read for real: execTestCommand runs via popen, so the "<"
	// redirect is honoured by the shell and inherited by the binary. The
	// trailing ":to-err\n" is execTestCommand's non-empty-stderr marker.
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/152_stdio_std_streams.pa"
		" < ../test/testdata/build-mgr/152_stdio_stdin_input.txt");
	ASSERT_EQ(output, "fd out=1 err=2 in=0\nin:piped-line\nto-out\n:to-err\n");
}

TEST(build_mgr, file_handle_no_autofree_mtrace) {
	// Scope exit must not free a `@!FILE` handle. The log's two allocations are
	// both libc.so.6 frames that parseMtraceLog skips; a control case (an owned
	// [4]int64) does produce a matching alloc/free, so frees==0 is a real signal.
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
	// Complements param_loop_call_arg's loop-region case with the straight-line
	// case: a genuine 2-cycle swap, and a 3-arg call whose bystander resolves
	// before the 2-cycle, exercising emitSafeRegMoves' cycle-break path when
	// the cycle isn't at index 0.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/154_call_arg_in_func_body.pa");
	ASSERT_EQ(output, "direct=7\nviacopy=7\ntwo=7 8\ng=22 11\ng4=1 3 2\n");
}

TEST(build_mgr, neg_lit_narrow_init) {
	// A negated literal now adopts the initializer's expected type instead of
	// always widening to int64/flo64 first and tripping narrowing diagnostics.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/155_neg_lit_narrow_init.pa");
	ASSERT_EQ(output, "-1 -1.500000 -200\n");
}

TEST(build_mgr, stat_file_types) {
	// The mkfifo'd path is checked with lstat only: open()'ing a FIFO with no
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
	// umask(0) makes mkdir's permission bits deterministic regardless of the
	// caller's inherited umask; chmod's bits are unaffected by umask either way.
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
	// Regression: a mixed signed/unsigned pair silently fell through
	// typeCompat's ExplicitCast with no convert node, in a binary operator
	// (m & big) and a call argument (uint32 -> int64 param).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/158_usual_arith_conv.pa");
	ASSERT_EQ(output, "255\n4294967295\n");
}

TEST(build_mgr, strtol_endptr) {
	// `@!` on a pointer-typed local (not just primitive) produces pntr-of-pntr,
	// letting strtol's `char **endptr` idiom be written as `@!end` where
	// `end` is `@!int8`.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/159_strtol_endptr.pa");
	ASSERT_EQ(output, "42 abc\n");
}

TEST(build_mgr, struct_ret_div) {
	// SysV struct-by-value return through real glibc functions: div_t
	// (1 eightbyte) and ldiv_t/lldiv_t (2 eightbytes, both INTEGER class).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/160_struct_ret_div.pa");
	ASSERT_EQ(output,
		"3 1\n"
		"3 1\n"
		"3 1\n");
}

TEST(build_mgr, struct_ret_div_mtrace) {
	// The calloc backing `div_t d` must be freed exactly once at scope exit
	// for a struct-ret'd C-function call.
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
	// Regression: sa_expr_member_call duplicated sa_expr_call's argument loop
	// without the variadic-promotion step, so an aliased call (`S.printf(...)`)
	// silently passed flo32 where va_arg reads flo64; the unaliased form
	// already promoted correctly.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/162_alias_call_variadic_promote.pa");
	ASSERT_EQ(output, "1.500000\n");
}

TEST(build_mgr, non_executable_stack) {
	// Palan bypasses the C driver, so palan-codegen must emit .note.GNU-stack
	// itself -- without it, the stack goes RWE as soon as any note-carrying
	// object (e.g. libc_nonshared.a's atexit) joins the link.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_gnu_stack_bin "
		"../test/testdata/build-mgr/001_helloworld.pa"), "");

	string segs = execTestCommand("readelf -lW /tmp/palan_gnu_stack_bin");
	ASSERT_NE(segs.find("GNU_STACK"), string::npos);
	ASSERT_EQ(segs.find("RWE"),       string::npos);
}

TEST(build_mgr, qsort_callback) {
	// glibc's qsort calls a Palan function through the func-ref/LeaLabel
	// address. Comparator uses a typed pointee (@int32); bsearch_callback
	// below covers the C-idiomatic @void spelling.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/163_qsort_callback.pa");
	ASSERT_EQ(output, "1 3 4 5\n");
}

TEST(build_mgr, bsearch_callback) {
	// Comparator uses C's `const void *` signature (@void, read-only rebinding
	// to @int32); bsearch's `void *` result binds to a typed pointer -- a miss
	// comparing equal to NULL proves glibc consumes the comparator's return
	// value, not just calls it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/164_bsearch_callback.pa");
	ASSERT_EQ(output,
		"4\n"
		"1\n");
}

TEST(build_mgr, atexit_callback) {
	// Two handlers prove LIFO dispatch order (C11 7.22.4.2) -- evidence they go
	// through glibc's __cxa_atexit registry, not invoked incidentally. Requires
	// the entry object's own __dso_handle. Both touch only .rodata strings:
	// _start frees its owned locals before `call exit`, so reading a Palan
	// local here would be a use-after-free.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/165_atexit_callback.pa");
	ASSERT_EQ(output,
		"hello\n"
		"last\n"
		"bye\n");
}

TEST(build_mgr, limits_constants) {
	// Regression: sa_cinclude()'s old "return if no functions" early-return
	// sat before constants-registration, so a function-less header's
	// (limits.h has zero C functions) constants were silently never registered.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/166_limits_constants.pa");
	ASSERT_EQ(output, "2147483647\n");
}

TEST(build_mgr, stdint) {
	// stdint.h declares zero C functions, only typedefs. Before the dedicated
	// typedefs section, int32_t was invisible: nothing carried its name-hint
	// through the old signature-piggyback path.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/167_stdint.pa");
	ASSERT_EQ(output, "5\n");
}

TEST(build_mgr, stdint_types) {
	// Complements stdint's minimal repro: multiple typedef'd types used
	// together plus one cross-type (int32_t -> int64_t) assignment.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/169_stdint_types.pa");
	ASSERT_EQ(output, "1000000 2000000 300000000 400000000 1000000\n");
}

TEST(build_mgr, typedef_chain) {
	// A multi-level chain (A->B->C->int32), a struct-bottomed typedef, and an
	// anonymous-struct typedef all register via the unconditional ast.typedefs
	// loop. The header also carries an unreferenced pointer-bottomed typedef
	// (P), proving the others still register when a header mixes one in.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/168_typedef_chain.pa");
	ASSERT_EQ(output, "1 1 1 3 7\n");
}

TEST(build_mgr, all_headers) {
	// cinclude all 13 supported headers simultaneously. A manual audit found
	// zero E_ConflictingTypedef diagnostics now that typedefs register
	// unconditionally rather than only when referenced by a function signature
	// -- this pins that result as a standing regression guard.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/170_all_headers.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, link_math) {
	// build-mgr unions "libs" from every module's sa.json into -l<name> for
	// ld, so a cinclude `link` clause actually makes sqrt (in libm) linkable.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/171_link_math.pa");
	ASSERT_EQ(output, "1.414214\n");
}

TEST(build_mgr, link_import) {
	// This file has no `link` clause -- imported lib_sqrt.pa is the one
	// requesting libm, proving the "libs" union is whole-program, not per-file.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/172_link_import.pa");
	ASSERT_EQ(output, "11\n");
}

TEST(build_mgr, math_functions) {
	// Broad libm coverage (sqrt/pow/sin/.../hypot) run through the `link`
	// clause, including a variable and a nested expression as arguments.
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
	// The -o argument used to be concatenated unquoted into ld's shell command
	// line. The marker file must NOT exist (injection closed); a file with the
	// exact literal name must exist (the argument reached ld verbatim).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string output = execTestCommand(
		"bin/palan -o 'out/b_$(touch PWNED)' ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");
	ASSERT_FALSE(fs::exists("PWNED"));
	ASSERT_TRUE(fs::exists("out/b_$(touch PWNED)"));
}

TEST(build_mgr, source_path_injection) {
	// The input .pa path used to flow unquoted through fs::weakly_canonical
	// into every pipeline stage's command line. Metacharacter filenames aren't
	// checked into git, so the hostile name is created at runtime.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand(
		"cp ../test/testdata/build-mgr/174_shell_metachar_source.pa 'out/inj_$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan 'out/inj_$(touch PWNED).pa'");
	ASSERT_EQ(output, "Hello World!\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, import_path_injection) {
	// An import path read back out of ast.json used to be concatenated
	// unquoted into the next palan-gen-ast shell command line. A nonexistent
	// metacharacter path is rejected earlier by fs::exists(), so the imported
	// file must actually exist under the hostile name to reach that code path.
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

TEST(build_mgr, syscall_write) {
	// Also exercises calling a return-declaring syscall as a bare statement.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/176_syscall_write.pa");
	ASSERT_EQ(output, "hello, syscall\nsecond line\nwrote 12\n");
}

TEST(build_mgr, syscall_read) {
	// Same buffer crosses both pointer permissions: @!void into read, @void into write.
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/177_syscall_read.pa"
		" < ../test/testdata/build-mgr/177_syscall_read_input.txt");
	ASSERT_EQ(output, "syscall-read-line\nn==w\n");
}

TEST(build_mgr, syscall_getpid_matches_libc) {
	// Declared sys_getpid, not getpid: a cinclude'd C function resolves first
	// with no collision diagnostic, which would compare libc against itself.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/178_syscall_getpid.pa");
	ASSERT_EQ(output, "same=1 positive=1\n");
}

TEST(build_mgr, syscall_exit_status) {
	// execTestCommand exposes no numeric exit code, so `echo $?` reports it;
	// braces keep palan's stderr inside execTestCommand's " 2>out/err" redirect.
	cleanTestEnv();
	string output = execTestCommand(
		"{ bin/palan ../test/testdata/build-mgr/179_syscall_exit.pa; echo $?; }");
	ASSERT_EQ(output, "before-exit\n7\n");
}

TEST(build_mgr, syscall_negative_errno) {
	// Raw %rax stays -9 (EBADF): Palan does not translate it into libc errno.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/180_syscall_errno.pa");
	ASSERT_EQ(output, "rc=-9\n");
}

TEST(build_mgr, import_syscall) {
	// The library module exports no code at all, only a prototype, so this also
	// covers assembling and linking an object with no callable symbol.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/181_import_syscall.pa");
	ASSERT_EQ(output, "import syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_syscall_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/182_import_syscall_alias.pa");
	ASSERT_EQ(output, "alias syscall\nn==14\n");
}

TEST(build_mgr, syscall_macro_number) {
	// A cinclude'd macro constant (SYS_write) used directly as a syscall
	// number, folded by gen-ast before it ever reaches SA.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/183_syscall_macro_number.pa");
	ASSERT_EQ(output, "macro syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_syscall_macro) {
	// The exported syscall's macro-derived number is already folded in the
	// library module's own ast.json (ast.export gets the same walk as the
	// library's own statements), so the importing module needs no cinclude
	// of its own to resolve it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/184_import_syscall_macro.pa");
	ASSERT_EQ(output, "import macro syscall\nsecond\nn==7\n");
}

TEST(build_mgr, macro_array_size) {
	// A macro constant used as an array size, both for a local array
	// declaration and a struct field array declaration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/185_macro_array_size.pa");
	ASSERT_EQ(output, "1 2 3 10 20 30\n");
}

TEST(build_mgr, macro_name_collision) {
	// A macro constant silently wins over a same-named variable, function
	// parameter, and Palan const -- every reference to the colliding name
	// resolves to the macro's value, ignoring the local declaration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/186_macro_name_collision.pa");
	ASSERT_EQ(output, "7 7 7\n");
}

TEST(build_mgr, c_union_rw)
{
	// Both members of a cinclude'd union alias the same bytes: a write through
	// one is visible through the other (little-endian 258 = 0x0102).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/187_c_union_rw.pa");
	ASSERT_EQ(output, "2 1\n259\n");
}

TEST(build_mgr, pthread_create_join)
{
	// The thread entry is a Palan function passed as C's void *(*)(void *).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/188_pthread_create_join.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, pthread_mutex)
{
	// A lost update between the two threads would show as a count below 200000.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/189_pthread_mutex.pa");
	ASSERT_EQ(output, "200000\n");
}

TEST(build_mgr, pthread_cond)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/190_pthread_cond.pa");
	ASSERT_EQ(output, "ready=1\n");
}

TEST(build_mgr, struct_field_type_alias)
{
	// Owned alias-typed fields reach the generated allocator module under
	// their resolved names.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/191_struct_field_type_alias.pa");
	ASSERT_EQ(output, "3 4 5 6 7\n");
}

TEST(build_mgr, int_literal_range)
{
	// A uint64 literal above INT64_MAX used to crash codegen's stoll.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/194_int_literal_range.pa");
	ASSERT_EQ(output, "18446744073709551600 -128 127\n");
}

TEST(build_mgr, int64_imm_range)
{
	// mov to memory only encodes a sign-extended imm32, so wider values
	// failed to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/195_int64_imm_range.pa");
	ASSERT_EQ(output, "3000000000 -9223372036854775807 9223372036854775808\n-3000000000\n");
}

TEST(build_mgr, unsigned_cmp)
{
	// Unsigned comparisons used signed setCC, so a value with its top bit set
	// compared as negative.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/196_unsigned_cmp.pa");
	ASSERT_EQ(output, "001101\n001101\n001101\n001101\n110001\nif ok\n6\n");
}

TEST(build_mgr, stack_arg_narrow_int)
{
	// Stack-passed arguments narrower than 64 bits were stored with movq,
	// which fails to assemble for a sized register source.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/197_stack_arg_narrow_int.pa");
	ASSERT_EQ(output, "1 2 3 4 5 -4 4000000001\n"
	                  "1 2 3 4 5 -299 -69999 -5\n"
	                  "1 2 3 4 5 -300 -70000 4000000000\n"
	                  "-699990393\n-700000493\n");
}

TEST(build_mgr, mul_8bit)
{
	// 8-bit multiply was emitted as imulq on byte registers, which fails to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/198_mul_8bit.pa");
	ASSERT_EQ(output, "-15 44 -128\n144 255\n44 144\n");
}

TEST(build_mgr, divmod_width_sign)
{
	// Div/Mod always used movq+cqto+idivq: narrower operands failed to assemble
	// and unsigned operands were divided as signed.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/199_divmod_width_sign.pa");
	ASSERT_EQ(output, "-3 -1 -128 0\n-4285 -5\n-666666666 -2\n-1285714285714285714 -2\n"
	                  "35 5 9285 5\n571428571 3\n1844674407370955160 0\n-128 500000000\n");
}

TEST(build_mgr, pln_float_call)
{
	// Palan calls passed every arg in intArgs[j] and returned in %rax regardless
	// of type, so float params/returns failed to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/200_pln_float_call.pa");
	ASSERT_EQ(output, "2.000000\n4.500000 2.500000\n21.750000\n"
	                  "9.000000 10.000000 600 700 800\n3655.000000\n"
	                  "x=1.250000\n2.500000\n3 1.500000 2 1.000000\n0.500000\n");
}

TEST(build_mgr, import_float)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/201_import_float.pa");
	ASSERT_EQ(output, "25.000000 1.500000\n");
}

TEST(build_mgr, multiret_recv_swap)
{
	// Results were copied to their dsts in reverse order, clobbering a return
	// register still holding a later-copied result when dsts sit in arg registers.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/202_multiret_recv_swap.pa");
	ASSERT_EQ(output, "2 1\n3 1 2\n20 2.500000 10\n");
}

TEST(build_mgr, owned_struct_arr_owned_field_mtrace) {
	// Moving each element into the array nulls the loop-local source, so the
	// generated __pln_free_L must accept NULL.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_owned_field_mtrace_bin "
		"../test/testdata/build-mgr/192_owned_struct_arr_owned_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_owned_field_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_owned_field_mtrace_bin");
	ASSERT_EQ(output, "5\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]L ls: 1 ptr array + 2 x (L + owned P) = 5 allocs
	EXPECT_EQ(allocs, 5) << "expected 5 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_arr_embed_field_mtrace) {
	// Element types whose embedded fields are not declarable in the generated
	// allocator module: a native embed, a C struct embed, and a C union.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_arr_embed_field_mtrace_bin "
		"../test/testdata/build-mgr/193_owned_arr_embed_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_arr_embed_field_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_arr_embed_field_mtrace_bin");
	ASSERT_EQ(output, "3 4 5 6\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]L: 1 + 2 x (L + owned P) = 5; [2]itimerspec: 3; [2]pthread_mutex_t: 3
	EXPECT_EQ(allocs, 11) << "expected 11 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, int_lit_float_ctx) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/203_int_lit_float_ctx.pa");
	ASSERT_EQ(output, "2.0 2.5 3.0\n3.0 2.0\n7.0\n");
}

TEST(build_mgr, arr_lit_1d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/204_arr_lit_1d.pa");
	ASSERT_EQ(output, "1 -2 3\n200 255\n1.5 2.0 10.0\n11 20 -2\n2 3 5\n9 7 8\n");
}

TEST(build_mgr, arr_lit_2d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/207_arr_lit_2d.pa");
	ASSERT_EQ(output, "1 3 5 -6\n8 10 12\n1.50 10.00 3.00 4.25\n3 4 6\n200 255 0 10\n20 -6 3\n10 11 12 13\n2 3 6\n");
}

TEST(build_mgr, mixed_type_var_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/206_mixed_type_var_decl.pa");
	ASSERT_EQ(output, "7 5000000000\n3\n1 4\n5 6\n");
}

TEST(build_mgr, arr_lit_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_lit_mtrace_bin "
		"../test/testdata/build-mgr/205_arr_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_lit_mtrace_bin");
	ASSERT_EQ(output, "8\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 2) << "expected 2 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, arr_lit_2d_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_lit_2d_mtrace_bin "
		"../test/testdata/build-mgr/208_arr_lit_2d_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_lit_2d_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_lit_2d_mtrace_bin");
	ASSERT_EQ(output, "10\n");

	// [2][3]int32: row table + 2 rows; []$[3]int32: one contiguous block
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 4) << "expected 4 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, uint64_float_convert) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/209_uint64_float_convert.pa");
	ASSERT_EQ(output,
		"0 9007199254740992 9223372036854775808 18446744073709551616 9223372036854777856\n"
		"0 9007199254740992 9223372036854775808 18446744073709551616\n"
		"18446744073709551616 9223372036854775808 9223372036854775808.0\n"
		"18000000000000000000 12345 9223372036854775808\n"
		"18446744073709549568\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan --clean");
	ASSERT_EQ(output, "");
}
