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
