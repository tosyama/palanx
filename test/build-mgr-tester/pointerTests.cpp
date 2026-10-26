/// palan build manager tests: pointers
///
/// @file pointerTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

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

TEST(build_mgr, at_bang_plain_var_decl) {
	cleanTestEnv();
	// `view` is a non-owning pointer aliasing `original`'s storage; writing
	// through `view.x` must be visible via `original.x` (same memory).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/121_at_bang_plain_var_decl.pa");
	ASSERT_EQ(output, "20 10\n");
}

TEST(build_mgr, addr_of) {
	cleanTestEnv();
	// Second pair (z = a + b) exercises addr-of on a non-literal-initialized
	// local -- the gap PlnRegAlloc's isVar-unification design closes.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/123_addr_of.pa");
	ASSERT_EQ(output, "42\n5\n");
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

TEST(build_mgr, deref_write_mutable_ptr) {
	// Regression guard for the new read-only enforcement: writes through
	// `@!T` must remain unaffected.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/132_deref_write_mutable_ptr.pa");
	ASSERT_EQ(output, "99\n99\n");
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
	ASSERT_EQ(output, "1 2 99 4\n");
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
	// Cluster c: calloc(1) + the field's row of 2 = 4 (same shape as
	// owned_struct_arr_field_mtrace). Taking @!c.pts[0].x adds no allocation.
	EXPECT_EQ(allocs, 4) << "expected 4 allocs for Cluster c { [2]Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, ptr_alias_pointee) {
	// Regression: a type alias/typedef used as a `@T`/`@!T` pointee reached
	// fromJson unresolved and aborted with rc=134.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/143_ptr_alias_pointee.pa");
	ASSERT_EQ(output, "42 7\n");
}

TEST(build_mgr, embed_ptr_slot) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/252_embed_ptr_slot.pa");
	ASSERT_EQ(output, "7 11 11\n");
}

TEST(build_mgr, ptr_ptr_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/284_ptr_ptr_field.pa");
	ASSERT_EQ(output, "7 6\n7 6\n");
}

// An array's element may be any pointer: here a pointer to a pointer.
TEST(build_mgr, ptr_ptr_slot_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/352_ptr_ptr_slot_arr.pa");
	ASSERT_EQ(output, "8 5 8\n");
}

TEST(build_mgr, func_ptr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/294_func_ptr.pa");
	ASSERT_EQ(output, "10 6 42 8\nhello\n8 4\n36 4.5 -7\n1 0\n1 2 3\nvia puts\n30\n");
}
