/// semantic analyzer error case tests: structs and pointers
///
/// @file errorStructTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(sa_error, unknown_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_060_unknown_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, unknown_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_061_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, non_prim_struct_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_062_non_prim_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unsupported struct field type"), string::npos);
}

TEST(sa_error, recursive_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_069_recursive_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct 'A' recursively contains itself"), string::npos);
}

TEST(sa_error, field_assign_undef_var)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_063_field_assign_undef_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, field_assign_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_064_field_assign_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_unknown_field)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_065_field_assign_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, field_assign_void_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_066_field_assign_void_value.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, field_access_undef_var)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_067_field_access_undef_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, field_access_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_068_field_access_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, inline_as_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_070_inline_as_value.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":5:1: error: cannot convert 'Point' to 'int64'."), string::npos);
}

TEST(sa_error, field_on_prim)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_071_field_on_prim.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, write_readonly_ptr)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_072_write_readonly_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through"), string::npos);
}

TEST(sa_error, struct_field_unknown_type)
{
	// type Foo { BadType x; } — prim field with unknown type name
	// Covers: buildStructDef E_UnknownType for prim field (sz < 0 branch)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_073_struct_field_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, embed_unknown_struct)
{
	// type Foo { $Unknown a; } — embed field with undefined struct name
	// Covers: buildStructDef E_UnknownType for embed field
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_074_embed_unknown_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, field_access_on_arr_index_non_struct)
{
	// [4]int64 arr; printf("%ld\n", arr[0].x); — arr[i] is not a struct pointer
	// Covers: resolveObjectChain arr-index base case, non-struct value-type branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_078_field_access_on_arr_index_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_on_arr_index_non_struct)
{
	// [4]int64 arr; 10 -> arr[0].x; — arr[i] is not a struct pointer
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, non-struct value-type branch (E_FieldAccessOnNonStruct)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_079_field_assign_on_arr_index_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_access_on_tuple_base)
{
	// type Point{...}; type Line{$Point a; $Point b;}; int64 v = (1,2).a.x; --
	// a multi-expression tuple grouping normalizes to "not-impl" (see term's
	// '(' tapple_inner ')' rule), reached as the base of a field-access chain.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_116_field_access_on_tuple_base.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("this expression is not supported in this version."), string::npos);
}

TEST(sa_error, field_access_on_prim_arr_field_elem)
{
	// type Buf { [4]$int64 data; }; printf("%ld\n", buf.data[0].sub);
	// -- data[0] is a primitive embed-arr leaf, not a struct pointer
	// Covers: sa_expr_arr_index's primitive leaf branch feeding
	// into resolveObjectChain's arr-index base case, non-struct value-type branch
	// (E_FieldAccessOnNonStruct), exercised via an embedded struct field array
	// rather than a plain variable array (already covered by
	// field_access_on_arr_index_non_struct).
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_084_field_access_on_prim_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, field_assign_on_prim_arr_field_elem)
{
	// type Buf { [4]$int64 data; }; 10 -> buf.data[0].sub;
	// -- same as field_access_on_prim_arr_field_elem but through the write side
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, non-struct
	// value-type branch (E_FieldAccessOnNonStruct), via embedded struct field array.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_085_field_assign_on_prim_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, struct_arr_ptr_field_unknown_prim_type)
{
	// type T { [4]NoSuchType field; }; -- [n]T owned pointer array, prim leaf unknown
	// Covers: buildStructDef "arr" branch, non-embedded owned array field,
	// E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_086_arr_ptr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, struct_embed_arr_field_unknown_prim_type)
{
	// type T { [4]$NoSuchType field; }; -- [n]$T embed-arr, prim leaf unknown
	// Covers: buildStructDef "arr" branch, embed-arr prim-leaf case,
	// E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_087_embed_arr_field_unknown_prim_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unknown type"), string::npos);
}

TEST(sa_error, addr_of_on_param)
{
	// `@n;` where n is a function parameter, not a local variable
	// Covers: sa_expression addr-of E_AddrOfNotLocalVar branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_091_addr_of_on_param.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("address-of requires a local variable"), string::npos);
}

TEST(sa_error, addr_of_struct_ptr_var)
{
	// `@v;` where v is a `@!Point` local -- a struct pointer is passed by name,
	// and '@' on it would build a `struct T **`.
	// Covers: sa_expr_addr_of E_AddrOfBorrowed branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_092_addr_of_struct_ptr_var.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("'v' is already a borrow"), string::npos);
}

TEST(sa_error, addr_of_undefined)
{
	// `@undefined_name;` -- reuses the existing E_UndefinedVariable diagnostic
	// Covers: sa_expression addr-of not-found branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_093_addr_of_undefined.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, addr_of_field_write_through_readonly)
{
	// `@Point p = s; @!int64 q = @!p.x;` -- p is a read-only struct pointer
	// (@T); taking a mutable address through one of its fields must not
	// launder read-only into mutable.
	// Covers: sa_expr_addr_of field-access branch -> resolveObjectChain(forWrite=true)
	// "id" case -> E_WriteThroughReadOnlyPtr
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_105_addr_of_field_write_through_readonly.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer '@T'"), string::npos);
}

TEST(sa_error, addr_of_field_immutable_ptr_hop)
{
	// `@!int64 p = @!n.next.val;` where `next` is `@Node` (read-only raw-ptr
	// field) -- the intermediate hop, not just the final field, must be
	// write-permission checked.
	// Covers: resolveObjectChain(forWrite=true) field-hop branch -> E_WriteToImmutablePtrField
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_106_addr_of_field_immutable_ptr_hop.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer field '@T'"), string::npos);
}

TEST(sa_error, addr_of_ptr_field)
{
	// `@s.in;` where `in` is a `@Inner` pointer field -- it is passed by name,
	// like a pointer variable.
	// Covers: sa_expr_addr_of field-access branch -> raw-ptr leaf -> E_AddrOfNotPrimitive
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_107_addr_of_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of"), string::npos);
}

TEST(sa_error, addr_of_unknown_field)
{
	// `@s.z;` where Point has no field `z`.
	// Covers: sa_expr_addr_of field-access branch -> findFieldOrExit -> E_UnknownField
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_108_addr_of_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("has no field"), string::npos);
}

TEST(sa_error, addr_of_call_not_addressable)
{
	// `@f();` -- a call returning a non-struct, outside a call argument.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_110_addr_of_call.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this expression"), string::npos);
}

TEST(sa_error, addr_of_field_readonly_arr_elem)
{
	// `@!int64 p = @!rpts[0].x;` where `rpts` is `[4]@Point` (read-only
	// pointer-slot array) -- the arr-index hop itself is the read-only
	// element, not just a field along the way.
	// Covers: resolveObjectChain(forWrite=true) "arr-index" branch -> E_WriteToReadOnlyArrElem
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_111_addr_of_field_readonly_arr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer array element"), string::npos);
}

TEST(sa_error, write_through_readonly_ptr)
{
	// `@int64 p = @x; 99 -> p[0];` -- deref write through a read-only `@T`
	// Covers: sa_arr_assign_stmt isWritableThrough branch (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_094_write_through_readonly_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, write_readonly_ptr_var_field)
{
	// `@Point view = original; 20 -> view.x;` -- field write through a
	// read-only `@T`-typed plain local variable (not a struct field)
	// Covers: resolveObjectChain(forWrite=true) "id" isWritableThrough branch
	// (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_095_write_readonly_ptr_var_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade)
{
	// `@int64 p = @x; @!int64 q = p;` -- binding a read-only pointer to a
	// mutable-typed destination
	// Covers: sa_var_decl ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_096_ptr_mutability_upgrade.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_assign)
{
	// `p -> q;` where p is `@int64` and q is `@!int64` -- plain assign-stmt
	// upgrade
	// Covers: sa_assign_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_097_ptr_mutability_upgrade_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_return)
{
	// `func f() -> IntPtr { ...; return p; }` where IntPtr is an alias for
	// `@!int64` and p is `@int64` -- return-stmt upgrade
	// Covers: sa_return_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_098_ptr_mutability_upgrade_return.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_arr_assign)
{
	// `view -> wpts[0];` where view is `@Point` and wpts is `[4]@!Point` --
	// storing a read-only pointer value into a mutable pointer-slot array
	// element
	// Covers: sa_arr_assign_stmt ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_099_ptr_mutability_upgrade_arr_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_field_assign)
{
	// `ro -> n1.next;` where ro is `@Node` and `next` is a `@!Node` field --
	// storing a read-only pointer value into a mutable pointer field
	// Covers: sa_field_assign ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_100_ptr_mutability_upgrade_field_assign.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, ptr_mutability_upgrade_call_arg)
{
	// `take(ro);` where `take` takes `@!int64` and ro is `@int64` -- passing
	// a read-only pointer where a Palan function expects a mutable one
	// Covers: sa_expr_call ptrPermissionOk branch (E_PtrMutabilityUpgrade)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_101_ptr_mutability_upgrade_call_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, assign_whole_struct_elem)
{
	// `other ->> pt[0];` -- `pt[0]` is the struct's storage, not a pointer
	// slot, so it cannot take over another struct's ownership.
	// Covers: sa_arr_assign_stmt addr-only guard (E_AssignToWholeStructElem)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_102_assign_whole_struct_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot assign to a struct element as a whole"), string::npos);
}

TEST(sa_error, write_readonly_struct_ptr_field)
{
	// `42 -> ro[0].x;` where ro is `@Point` -- writing a field through a
	// read-only struct pointer via `p[i].field` is rejected, same as for a
	// struct stored in a read-only borrowed array.
	// Covers: resolveObjectChain(forWrite=true) arr-index branch (E_WriteThroughReadOnlyPtr)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_103_write_readonly_struct_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, deref_unknown_struct_ptr)
{
	// `func f(@!Foo p) { p[0].bar; }` where `Foo` is never declared. gen-ast has
	// no symbol table, so `Foo` parses as a prim base-type, not a struct one --
	// sa_expr_arr_index's generic (non-struct) branch must reject
	// this at the SA boundary instead of letting elemSizeBytes' -1
	// "unknown type" sentinel leak into elem-size and crash palan-codegen
	// downstream (layer violation).
	// A local `@!Foo p;` var decl is
	// now rejected at declaration time (sa_var_decl), so only a parameter's
	// pointee (unchecked at signature normalization) still reaches this guard.
	// Covers: sa_expr_arr_index generic branch, sz<0 guard (E_UnknownType)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_104_deref_unknown_struct_ptr.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, addr_of_ptr_elem_not_addressable)
{
	// `@!wpts[0]` where `wpts` is `[4]@!Point` (pointer-slot array) -- the
	// element itself is already a pointer, so taking its address would be
	// a double indirection; out of scope.
	// Covers: sa_expr_addr_of arr-index branch, elem value-type type-kind != "prim" -> E_AddrOfNotPrimitiveElem
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_118_addr_of_ptr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot take the address of this array element"), string::npos);
}

TEST(sa_error, addr_of_readonly_ptr_elem)
{
	// `@!p[1]` where `p` is `@int64` (read-only pointer) -- `@!arr[i]` is
	// "get write permission to arr[i]", so it must be rejected the same way
	// a plain deref-write through a read-only pointer is.
	// Covers: sa_expr_addr_of arr-index branch, isMutable && !isWritableThrough -> E_WriteThroughReadOnlyPtr
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_119_addr_of_readonly_ptr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, ptr_decl_unknown_type)
{
	// `@!@!NoSuchStruct p;` (pntr-of-pntr, to exercise the walk-through loop
	// too) -- sa_var_decl used to have no "pntr" branch in its dispatch guard,
	// so the pointee name was never validated at declaration time; without an
	// initializer, this used to compile silently and leave `p` referencing a
	// nonexistent type.
	// Covers: sa_var_decl pntr branch (incl. pntr-of-pntr walk), unknown
	// pointee prim name -> E_UnknownType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_120_ptr_decl_unknown_type.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("unknown type"), string::npos);
}

TEST(sa_error, call_arg_ptr_mismatch_native)
{
	// convertCallArg used to pass any non-Prim
	// (pointer/struct) argument through unchecked -- `@!int32` bound to a
	// `@!int64` parameter of a Palan function compiled with no diagnostic
	// and the wrong access width at runtime.
	// Covers: convertCallArg via sa_expr_call (Palan callee), E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_153_call_arg_ptr_mismatch_native.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, convert_for_binding_ptr_mismatch)
{
	// Same Incompatible gap as call_arg_ptr_mismatch_native, but through
	// convertForBinding's binding-site path (var-decl initializer here;
	// shared by assignment/array-assign/return/field-assign too).
	// Covers: convertForBinding, E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_156_convert_for_binding_ptr_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, deref_void_pointer)
{
	// `p[0]` where p is @!void -- void has no size, so
	// this must give a dedicated diagnostic rather than the misleading
	// "unknown type 'void'." that the shared sz<0 guard in
	// sa_expr_arr_index would otherwise produce.
	// Covers: sa_expr_arr_index void-pointee guard, E_DerefVoidPointer
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_166_deref_void_pointer.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot index or dereference a void pointer"), string::npos);
}

TEST(sa_error, deref_void_pointer_write)
{
	// Same guard as deref_void_pointer, reached via sa_arr_assign_stmt's
	// write path (`v -> p[0]`) instead of a read.
	// Covers: sa_expr_arr_index void-pointee guard, E_DerefVoidPointer
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_167_deref_void_pointer_write.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot index or dereference a void pointer"), string::npos);
}

TEST(sa_error, field_access_through_prim_ptr_field)
{
	// Prerequisite fix: `s.p.q` where `p` is a
	// raw-ptr field with a primitive pointee (`@!int64 p;`) has nothing to
	// chain into -- resolveObjectChain must reject it as a non-struct hop,
	// not build a bogus pntr(struct("int64")) and fail later with an
	// unrelated "unknown type 'int64'." from requireCompleteStruct.
	// Covers: resolveObjectChain raw-ptr elemKind=="prim" guard,
	// E_FieldAccessOnNonStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_168_field_access_through_prim_ptr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("field access on non-struct variable"), string::npos);
}

TEST(sa_error, struct_borrow)
{
	const pair<string, string> cases[] = {
		{"error_248_struct_borrow_no_addr_of.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_249_struct_borrow_c_no_addr_of.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_250_struct_borrow_init_no_addr_of.pa", ":3:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_251_struct_borrow_ro_to_mut.pa", ":4:1: error: cannot bind a read-only pointer"},
		{"error_252_struct_borrow_owned_field_ro.pa", ":3:27: error: cannot write through read-only pointer"},
		{"error_253_ro_ptr_embed_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_254_ro_ptr_embed_arr_field_addr_mut.pa", ":3:30: error: cannot write through read-only pointer '@T'"},
		{"error_255_ro_ptr_owned_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_256_ro_ptr_embed_struct_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_257_ro_ptr_owned_struct_arr_field_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_258_ro_ptr_ptr_slot_arr_field_store.pa", ":3:29: error: cannot write through read-only pointer '@T'"},
		{"error_259_ro_ptr_raw_ptr_field_index_write.pa", ":3:21: error: cannot write through read-only pointer '@T'"},
		{"error_260_mut_ptr_ro_elem_arr_field_write.pa", ":3:22: error: cannot write through read-only pointer array element"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_arr_2d)
{
	const pair<string, string> cases[] = {
		{"error_291_embed_struct_arr_2d_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer '@T'"},
		{"error_292_embed_struct_arr_2d_borrow_owned.pa", "array shape '[2][3]$Point' does not match the borrowed array type '@[2][3]Point'."},
		{"error_293_struct_arr_2d_borrow_embed.pa", "array shape '[2][3]Point' does not match the borrowed array type '@[2][3]$Point'."},
		{"error_294_embed_struct_arr_2d_borrow_size.pa", "array shape '[2][4]$Point' does not match the borrowed array type '@[2][3]$Point'."},
		{"error_295_embed_struct_arr_2d_owned_sub.pa", ":3:1: error: cannot embed 'Box' with '$': it owns storage of its own; use 'Box' without '$'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_arr_lit)
{
	const pair<string, string> cases[] = {
		{"error_263_struct_lit_count.pa", ":2:21: error: struct 'Point' has 2 fields but its literal has 1 values."},
		{"error_264_struct_lit_dup_field.pa", ":2:26: error: field 'x' is given more than once in a struct literal."},
		{"error_265_struct_lit_missing_field.pa", ":2:15: error: the literal of struct 'Point' is missing field 'y'."},
		{"error_266_struct_lit_unknown_field.pa", ":2:21: error: struct 'Point' has no field 'z'."},
		{"error_267_struct_lit_ptr_field.pa", ":2:15: error: field 'p' of struct 'P' cannot be initialized by a literal: only numeric, struct and array fields are supported."},
		{"error_268_struct_lit_narrowing.pa", ":3:17: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_693_dict_lit_prim_var.pa", ":1:11: error: a '{name: value}' literal can only be used as a struct value"},
		{"error_269_dict_lit_prim_elem.pa", ":1:15: error: a '{name: value}' literal can only be used as a struct value: a struct variable's initializer, the source of '->' to a struct, an element of an array literal, or an argument for a struct parameter."},
		{"error_270_dict_lit_context.pa", ":3:5: error: parameter 'p' of type 'int32' cannot take a '{name: value}' literal: only a struct parameter can."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ptr_slot_arr)
{
	const pair<string, string> cases[] = {
		{"error_282_prim_ptr_slot_arr_write_ro.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
		{"error_283_ptr_slot_arr_borrow_elem_upgrade.pa", "array shape '[2]@P' does not match the borrowed array type '@[2]@!P'."},
		{"error_284_ptr_slot_arr_borrow_narrow_mut_slots.pa", "array shape '[2]@!P' does not match the borrowed array type '@![2]@P'."},
		{"error_285_ptr_slot_arr_borrow_owned_arr.pa", "array shape '[2]P' does not match the borrowed array type '@![2]@!P'."},
		{"error_286_ptr_slot_arr_borrow_write_ro_slot.pa", ":3:2: error: cannot write through read-only pointer '@T'"},
		{"error_287_ptr_slot_arr_borrow_write_ro_elem.pa", ":3:7: error: cannot write through read-only pointer array element"},
		{"error_288_prim_ptr_slot_arr_borrow_as_prim.pa", "array shape '[2]@int32' does not match the borrowed array type '@[2]int32'."},
		{"error_290_arr_borrow_runtime_inner_size.pa", "array shape '[2][?]int32' does not match the borrowed array type '@[2][3]int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, embed_owning_struct)
{
	// Owned array fields count as owning just like owned struct fields,
	// and a plain $T field is rejected the same way as [n]$T.
	const pair<string, string> cases[] = {
		{"error_326_embed_arr_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns storage of its own"},
		{"error_327_embed_arr_2d_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns storage of its own"},
		{"error_328_embed_arr_field_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns storage of its own"},
		{"error_329_embed_field_owning_arr_field.pa", ":2:1: error: cannot embed 'W' with '$': it owns storage of its own"},
		{"error_330_embed_field_owning_struct_field.pa", ":3:1: error: cannot embed 'Rect' with '$': it owns storage of its own"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_field_size)
{
	const pair<string, string> cases[] = {
		{"error_431_struct_field_var_size.pa", ":4:2: error: struct field array size must be a compile-time constant integer; 'n' is not a const."},
		{"error_567_struct_field_var_size_top.pa", ":2:1: error: struct field array size must be a compile-time constant integer; 'n' is not a const."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_ptr_elem_copy)
{
	const pair<string, string> cases[] = {
		{"error_440_copy_to_readonly_ptr_elem.pa", ":2:24: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_lit_init_assign)
{
	const pair<string, string> cases[] = {
		{"error_441_struct_lit_to_borrow.pa", ":4:1: error: a struct literal cannot be given to a borrow '@T'; write to its '[0]' to copy into the struct it points to."},
		{"error_442_struct_lit_init_borrow.pa", ":3:1: error: a struct literal cannot be given to a borrow '@T'; write to its '[0]' to copy into the struct it points to."},
		{"error_443_struct_lit_to_readonly_elem.pa", ":2:16: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_444_struct_lit_assign_count.pa", ":3:1: error: struct 'V' has 2 fields but its literal has 3 values."},
		{"error_445_struct_lit_assign_ptr_field.pa", ":4:2: error: field 'p' of struct 'P' cannot be initialized by a literal: only numeric, struct and array fields are supported."},
		{"error_446_struct_lit_transfer.pa", ":3:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, or a call result."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, named_ret_struct_redecl)
{
	const pair<string, string> cases[] = {
		{"error_447_named_ret_struct_redecl.pa", ":3:2: error: Variable 'p' is already declared."},
		{"error_448_named_ret_struct_nested_decl.pa", ":4:3: error: Variable 'p' is already declared."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, struct_init_from_c_ptr_ret)
{
	// A C pointer return has no owner to take over, unlike a Palan struct return.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_450_struct_init_from_c_ptr_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: a 'tm' variable can only be initialized from a call that returns a 'tm' (a Palan function, or a C function returning it by value)."), string::npos) << sa;
}

TEST(sa_error, struct_init_other_struct_ret)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_451_struct_init_other_struct_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: cannot copy 'Q' into 'P'"), string::npos) << sa;
}

TEST(sa_error, return_arr_lit_non_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_452_return_arr_lit_non_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:28: error: an array literal can only be used as"), string::npos) << sa;
}

TEST(sa_error, return_struct_lit_field_count)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"bin/palan-gen-ast ../test/testdata/sa/error_453_return_struct_lit_field_count.pa -o " + ast_out), "");
	string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:24: error: struct 'P' has 2 fields but its literal has 1 values."), string::npos) << sa;
}

TEST(sa_error, ptr_ptr_field_chain)
{
	const pair<string, string> cases[] = {
		{"error_538_ptr_ptr_field_chain.pa", ":4:11: error: field access on non-struct variable."},
		{"error_539_c_ptr_ptr_field_chain.pa", ":3:11: error: field access on non-struct variable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ptr_inner_permission)
{
	const string upgrade = "error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'.";
	const string narrowing = "error: cannot bind '@!@!int32' to '@!@int32': "
		"a pointer reached through a mutable pointer '@!' must keep its permission.";
	const pair<string, string> cases[] = {
		{"error_540_ptr_inner_upgrade.pa", ":4:1: " + upgrade},
		{"error_541_ptr_inner_narrowing.pa", ":4:1: " + narrowing},
		{"error_542_ptr_ptr_field_inner_upgrade.pa", ":3:1: " + upgrade},
		{"error_543_c_ptr_inner_narrowing.pa", ":5:1: " + narrowing},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, func_ptr)
{
	const string sigType = " cannot be a parameter or return type of a function pointer;";
	const pair<string, string> cases[] = {
		{"error_599_mutable_func_ptr.pa",
		 ":1:1: error: a function pointer is read-only; write '@func(...)', not '@!func(...)'."},
		{"error_600_func_ptr_struct_param.pa", ":2:1: error: 'P'" + sigType},
		{"error_601_func_ptr_arr_param.pa", ":1:1: error: 'array'" + sigType},
		{"error_602_func_ptr_alias_struct_param.pa", ":3:1: error: 'P'" + sigType},
		{"error_603_variadic_func_value.pa",
		 ":2:27: error: function 'printf' cannot be used as a function pointer: it takes variable arguments."},
		{"error_604_syscall_func_value.pa",
		 ":2:27: error: function 'wr' cannot be used as a function pointer: it is a syscall."},
		{"error_605_owned_param_func_value.pa",
		 ":3:15: error: function 'g' cannot be used as a function pointer: its parameter type 'P' cannot be passed through one."},
		{"error_606_owned_ret_func_value.pa",
		 ":3:19: error: function 'g' cannot be used as a function pointer: its return type 'P' cannot be returned through one."},
		{"error_607_ambiguous_func_ptr_call.pa",
		 ":5:1: error: call to 'f3' is ambiguous: it names both a function pointer and a function."},
		{"error_608_ambiguous_func_ptr_field_call.pa",
		 ":4:1: error: call to 'run' is ambiguous: it names both a function pointer and a function."},
		{"error_609_func_ptr_permission.pa",
		 ":2:1: error: cannot convert '@func(@!int32) -> int32' to '@func(@int32) -> int32'."},
		{"error_610_func_ptr_arg_count.pa", ":3:1: error: function 'f' takes 1 argument(s), but 2 given."},
		{"error_685_func_ptr_c_func_call.pa",
		 ":3:1: error: call to 'abs' is ambiguous: it names both a function pointer and a function."},
		{"error_686_method_on_incomplete_struct.pa", ":3:1: error: Undefined function 'foo'."},
		{"error_687_method_on_non_func_ptr_field.pa", ":3:11: error: Undefined function 'n'."},
		{"error_694_func_ptr_param_count.pa",
		 ":3:1: error: cannot convert '@func(int32) -> int32' to '@func(int32, int32) -> int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, frame_var)
{
	const pair<string, string> cases[] = {
		{"error_620_frame_prim.pa", ":2:1: error: '$' on a variable applies only to a struct or an array, not 'int32'."},
		{"error_621_frame_ptr.pa", ":2:1: error: '$' on a variable applies only to a struct or an array, not '@P'."},
		{"error_622_frame_arr_size.pa", ":2:1: error: the size of stack array 'a' must be a compile-time constant."},
		{"error_623_return_frame_arr.pa", ":3:5: error: stack array 'a' cannot be returned; its storage ends with the function."},
		// A stack variable doesn't own its storage, so it has nothing to give away.
		{"error_624_transfer_frame_struct.pa", ":5:1: error: '->>' needs a source that owns its value"},
		{"error_625_transfer_frame_arr.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_626_frame_embed_owning.pa", ":3:1: error: cannot embed 'Q' with '$': it owns storage of its own; use 'Q' without '$'."},
		{"error_627_frame_param.pa", ":2:1: error: this parameter is not supported in this version."},
		{"error_628_frame_named_ret.pa", ":2:1: error: this return value is not supported in this version."},
		{"error_629_frame_ret_type.pa", ":2:13: error: this return value is not supported in this version."},
		{"error_663_tapple_frame_prim.pa", ":2:1: error: '$' on a variable applies only to a struct or an array, not 'int32'."},
		{"error_664_tapple_frame_arr.pa", ":3:1: error: cannot copy '[?]int32' into '[3]int32'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, expr_rooted_access)
{
	const pair<string, string> cases[] = {
		{"error_646_call_owned_struct_field.pa", ":3:11: error: a struct or array returned by a call has no owner to access it through"},
		{"error_647_call_owned_struct_field_write.pa", ":3:6: error: a struct or array returned by a call has no owner to access it through"},
		{"error_648_c_byval_struct_field.pa", ":2:17: error: a struct or array returned by a call has no owner to access it through"},
		{"error_649_call_temp_borrow_field.pa", ":4:11: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_650_readonly_cast_field_write.pa", ":4:7: error: cannot write through read-only pointer '@T'"},
		{"error_651_readonly_cast_index_write.pa", ":3:1: error: cannot write through read-only pointer '@T'"},
		{"error_652_call_prim_field.pa", ":2:11: error: field access on non-struct variable."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"bin/palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("bin/palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}
