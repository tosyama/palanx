/// Palan Type System — PlnType class hierarchy, PlnTypeRegistry, and typeCompat()
///
/// @file PlnType.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <map>
#include <memory>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

// Result of type compatibility check
enum class TypeCompat {
    Identical,      // same type
    ImplicitWiden,  // implicit widening (SA inserts convert node)
    ExplicitCast,   // explicit cast required (narrowing or signed<->unsigned)
    Incompatible,   // no conversion possible
};

// Type base class
struct PlnType {
    enum class Kind { Prim, Ptr } kind;
    virtual ~PlnType() = default;
protected:
    PlnType(Kind k) : kind(k) {}
};

// Primitive type
struct PrimType : PlnType {
    enum class Name {
        Int8, Int16, Int32, Int64,
        Uint8, Uint16, Uint32, Uint64,
        Float32, Float64
    } name;
    PrimType(Name n) : PlnType(Kind::Prim), name(n) {}
};

// Pointer type
struct PtrType : PlnType {
    const PlnType* base;
    PtrType(const PlnType* b) : PlnType(Kind::Ptr), base(b) {}
};

// Type registry — owns all type instances and interns them
class PlnTypeRegistry {
    std::map<PrimType::Name, std::unique_ptr<PrimType>> primCache_;
    std::map<const PlnType*, std::unique_ptr<PtrType>>  ptrCache_;
public:
    const PrimType* prim(PrimType::Name name);
    const PtrType*  ptr(const PlnType* base);
    const PlnType*  fromJson(const json& j);
    json            toJson(const PlnType* t);
};

// Type compatibility check
TypeCompat typeCompat(const PlnType* from, const PlnType* to,
                      const PlnTypeRegistry& registry);
