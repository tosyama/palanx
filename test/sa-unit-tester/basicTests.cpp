#include <gtest/gtest.h>
#include "PlnType.h"

using N = PrimType::Name;

// -------- typeCompat: identical --------

TEST(typecompat, identical_prim) {
    PlnTypeRegistry reg;
    const PlnType* t = reg.prim(N::Int32);
    EXPECT_EQ(typeCompat(t, t, reg), TypeCompat::Identical);
}

TEST(typecompat, identical_ptr) {
    PlnTypeRegistry reg;
    const PlnType* base = reg.prim(N::Int32);
    const PlnType* p1 = reg.ptr(base);
    const PlnType* p2 = reg.ptr(base);  // interned — same pointer
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(typeCompat(p1, p2, reg), TypeCompat::Identical);
}

// -------- typeCompat: ImplicitWiden --------

TEST(typecompat, implicit_widen_signed) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Int64), reg),
              TypeCompat::ImplicitWiden);
}

TEST(typecompat, implicit_widen_unsigned) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Uint16), reg.prim(N::Uint32), reg),
              TypeCompat::ImplicitWiden);
}

// -------- typeCompat: ExplicitCast --------

TEST(typecompat, explicit_cast_narrowing) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int64), reg.prim(N::Int32), reg),
              TypeCompat::ExplicitCast);
}

TEST(typecompat, explicit_cast_sign_change_same_size) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Uint32), reg),
              TypeCompat::ExplicitCast);
}

TEST(typecompat, explicit_cast_sign_change_widen) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Uint64), reg),
              TypeCompat::ExplicitCast);
}

// -------- typeCompat: Incompatible --------

TEST(typecompat, incompatible_prim_ptr) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* pi32 = reg.ptr(i32);
    EXPECT_EQ(typeCompat(i32, pi32, reg), TypeCompat::Incompatible);
}

TEST(typecompat, incompatible_ptr_diff_base) {
    PlnTypeRegistry reg;
    const PlnType* pi32 = reg.ptr(reg.prim(N::Int32));
    const PlnType* pi64 = reg.ptr(reg.prim(N::Int64));
    EXPECT_EQ(typeCompat(pi32, pi64, reg), TypeCompat::Incompatible);
}

// -------- typeCompat: pntr(void) (IT-2605) --------

TEST(typecompat, void_ptr_compat_with_typed_ptr) {
    PlnTypeRegistry reg;
    const PlnType* pvoid = reg.ptr(reg.prim(N::Void));
    const PlnType* pi32  = reg.ptr(reg.prim(N::Int32));
    EXPECT_EQ(typeCompat(pvoid, pi32, reg), TypeCompat::Identical);
    EXPECT_EQ(typeCompat(pi32, pvoid, reg), TypeCompat::Identical);
}

TEST(typecompat, void_ptr_compat_with_void_ptr) {
    PlnTypeRegistry reg;
    const PlnType* pvoid = reg.ptr(reg.prim(N::Void));
    EXPECT_EQ(typeCompat(pvoid, pvoid, reg), TypeCompat::Identical);
}

// -------- PlnTypeRegistry: interning --------

TEST(typecompat, registry_intern_prim) {
    PlnTypeRegistry reg;
    const PlnType* a = reg.prim(N::Int32);
    const PlnType* b = reg.prim(N::Int32);
    EXPECT_EQ(a, b);
}

TEST(typecompat, registry_intern_ptr) {
    PlnTypeRegistry reg;
    const PlnType* base = reg.prim(N::Int32);
    const PlnType* p1 = reg.ptr(base);
    const PlnType* p2 = reg.ptr(base);
    EXPECT_EQ(p1, p2);
}

// -------- PlnTypeRegistry: fromJson --------

TEST(typecompat, registry_from_json_prim) {
    PlnTypeRegistry reg;
    json j = {{"type-kind", "prim"}, {"type-name", "int32"}};
    const PlnType* t = reg.fromJson(j);
    EXPECT_EQ(t, reg.prim(N::Int32));
}

TEST(typecompat, registry_from_json_ptr) {
    PlnTypeRegistry reg;
    json j = {
        {"type-kind", "pntr"},
        {"base-type", {{"type-kind", "prim"}, {"type-name", "int32"}}}
    };
    const PlnType* t = reg.fromJson(j);
    ASSERT_EQ(t->kind, PlnType::Kind::Ptr);
    const auto* pt = static_cast<const PtrType*>(t);
    EXPECT_EQ(pt->base, reg.prim(N::Int32));
}

TEST(typecompat, registry_from_json_void_ptr) {
    PlnTypeRegistry reg;
    json j = {
        {"type-kind", "pntr"},
        {"base-type", {{"type-kind", "prim"}, {"type-name", "void"}}}
    };
    const PlnType* t = reg.fromJson(j);
    ASSERT_EQ(t->kind, PlnType::Kind::Ptr);
    const auto* pt = static_cast<const PtrType*>(t);
    EXPECT_EQ(pt->base, reg.prim(N::Void));
}

TEST(typecompat, registry_from_json_unrepresentable_throws) {
    // IT-2026-09-06-2906: fromJson delegates its domain check to
    // unrepresentableTypeName; this backstop throw only fires for a
    // value-type that reached it despite the upstream gates
    // (validateNativeSig / requireSupportedCFuncSig) -- exercised directly
    // here since no SA-level test path reaches it anymore.
    PlnTypeRegistry reg;
    json j = {{"type-kind", "union"}};
    EXPECT_THROW(reg.fromJson(j), std::runtime_error);
}

// -------- unrepresentableTypeName --------

TEST(typecompat, unrepresentable_prim_known) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"},{"type-name","int32"}}), "");
}

TEST(typecompat, unrepresentable_prim_unknown) {
    // e.g. c2ast's "flt128" for `long double`, not in PrimTypeNames
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"},{"type-name","flt128"}}), "flt128");
}

TEST(typecompat, unrepresentable_prim_missing_name) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"}}), "malformed type");
}

TEST(typecompat, unrepresentable_missing_type_kind) {
    EXPECT_EQ(unrepresentableTypeName(json::object()), "malformed type");
}

TEST(typecompat, unrepresentable_non_object) {
    EXPECT_EQ(unrepresentableTypeName(json(nullptr)), "malformed type");
}

TEST(typecompat, unrepresentable_pntr_known_base) {
    json j = {{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","int64"}}}};
    EXPECT_EQ(unrepresentableTypeName(j), "");
}

TEST(typecompat, unrepresentable_pntr_missing_base) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","pntr"}}), "malformed type");
}

TEST(typecompat, unrepresentable_pntr_recurses_to_func) {
    // int (*cb)(int) -- pntr(func(...))
    json j = {{"type-kind","pntr"},{"base-type",{{"type-kind","func"}}}};
    EXPECT_EQ(unrepresentableTypeName(j), "function pointer");
}

TEST(typecompat, unrepresentable_struct_named) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","struct"},{"type-name","Foo"}}), "");
}

TEST(typecompat, unrepresentable_struct_anonymous) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","struct"}}), "anonymous struct");
}

TEST(typecompat, unrepresentable_arr) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","arr"}}), "array");
}

TEST(typecompat, unrepresentable_union) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","union"}}), "union");
}

TEST(typecompat, unrepresentable_enum) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","enum"}}), "enum");
}

TEST(typecompat, unrepresentable_strct_named) {
    // c2ast's pre-normalizeCType shape (checked ahead of the strct->struct fold)
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","strct"},{"type-name","Tag"}}), "Tag");
}

TEST(typecompat, unrepresentable_strct_anonymous) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","strct"}}), "anonymous struct");
}

TEST(typecompat, unrepresentable_user_named) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","user"},{"type-name","mystery_t"}}), "mystery_t");
}

TEST(typecompat, unrepresentable_user_unnamed) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","user"}}), "user");
}

TEST(typecompat, unrepresentable_unknown_kind_fallback) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","embed"}}), "embed");
}
