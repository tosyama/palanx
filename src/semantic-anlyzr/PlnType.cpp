/// Palan Type System — PlnTypeRegistry and typeCompat() implementation
///
/// @file PlnType.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnType.h"
#include <stdexcept>

// Singleton that holds bidirectional maps between type-name strings and PrimType::Name.
// Using a struct with an explicit constructor body so gcov tracks each entry individually.
namespace {
struct PrimTypeNames {
    std::map<std::string, PrimType::Name> toEnum;
    std::map<PrimType::Name, std::string> fromEnum;

    PrimTypeNames() {
        toEnum["int8"]   = PrimType::Name::Int8;
        toEnum["int16"]  = PrimType::Name::Int16;
        toEnum["int32"]  = PrimType::Name::Int32;
        toEnum["int64"]  = PrimType::Name::Int64;
        toEnum["uint8"]  = PrimType::Name::Uint8;
        toEnum["uint16"] = PrimType::Name::Uint16;
        toEnum["uint32"] = PrimType::Name::Uint32;
        toEnum["uint64"] = PrimType::Name::Uint64;
        toEnum["flo32"]  = PrimType::Name::Float32;
        toEnum["flo64"]  = PrimType::Name::Float64;
        for (auto& [k, v] : toEnum) fromEnum[v] = k;
    }

    static const PrimTypeNames& instance() {
        static PrimTypeNames inst;
        return inst;
    }
};
} // namespace

// PlnTypeRegistry implementation

const PrimType* PlnTypeRegistry::prim(PrimType::Name name)
{
    auto it = primCache_.find(name);
    if (it != primCache_.end()) return it->second.get();
    auto [ins, ok] = primCache_.emplace(name, std::make_unique<PrimType>(name));
    return ins->second.get();
}

const PtrType* PlnTypeRegistry::ptr(const PlnType* base)
{
    auto it = ptrCache_.find(base);
    if (it != ptrCache_.end()) return it->second.get();
    auto [ins, ok] = ptrCache_.emplace(base, std::make_unique<PtrType>(base));
    return ins->second.get();
}

const PlnType* PlnTypeRegistry::fromJson(const json& j)
{
    std::string kind = j.at("type-kind").get<std::string>();
    if (kind == "prim") {
        std::string tname = j.at("type-name").get<std::string>();
        auto& toEnum = PrimTypeNames::instance().toEnum;
        auto it = toEnum.find(tname);
        if (it == toEnum.end())
            throw std::runtime_error("unknown prim type-name: " + tname);
        return prim(it->second);
    }
    if (kind == "pntr") {
        const PlnType* base = fromJson(j.at("base-type"));
        return ptr(base);
    }
    throw std::runtime_error("unknown type-kind: " + kind);
}

json PlnTypeRegistry::toJson(const PlnType* t)
{
    if (t->kind == PlnType::Kind::Prim) {
        const auto* p = static_cast<const PrimType*>(t);
        auto& fromEnum = PrimTypeNames::instance().fromEnum;
        return {{"type-kind", "prim"}, {"type-name", fromEnum.at(p->name)}};
    }
    if (t->kind == PlnType::Kind::Ptr) {
        const auto* p = static_cast<const PtrType*>(t);
        return {{"type-kind", "pntr"}, {"base-type", toJson(p->base)}};
    }
    throw std::runtime_error("unknown PlnType::Kind");
}

// typeCompat implementation

static int primGroup(PrimType::Name n)
{
    // 0 = signed, 1 = unsigned, 2 = float
    using N = PrimType::Name;
    switch (n) {
        case N::Int8: case N::Int16: case N::Int32: case N::Int64:   return 0;
        case N::Uint8: case N::Uint16: case N::Uint32: case N::Uint64: return 1;
        case N::Float32: case N::Float64:                              return 2;
    }
    return -1;
}

static int primRank(PrimType::Name n)
{
    using N = PrimType::Name;
    switch (n) {
        case N::Int8:   case N::Uint8:   return 1;
        case N::Int16:  case N::Uint16:  return 2;
        case N::Int32:  case N::Uint32:  return 3;
        case N::Int64:  case N::Uint64:  return 4;
        case N::Float32:                 return 1;
        case N::Float64:                 return 2;
    }
    return -1;
}

TypeCompat typeCompat(const PlnType* from, const PlnType* to,
                      const PlnTypeRegistry& /*registry*/)
{
    if (from == to) return TypeCompat::Identical;

    if (from->kind == PlnType::Kind::Prim && to->kind == PlnType::Kind::Prim) {
        const auto* pf = static_cast<const PrimType*>(from);
        const auto* pt = static_cast<const PrimType*>(to);

        int gf = primGroup(pf->name);
        int gt = primGroup(pt->name);
        // Integer (signed or unsigned) → float: implicit widening allowed.
        // Float → integer and cross-signedness require explicit cast.
        if ((gf == 0 || gf == 1) && gt == 2) return TypeCompat::ImplicitWiden;
        if (gf != gt) return TypeCompat::ExplicitCast;

        int rf = primRank(pf->name);
        int rt = primRank(pt->name);
        if (rt > rf) return TypeCompat::ImplicitWiden;
        return TypeCompat::ExplicitCast;  // narrowing
    }

    if (from->kind == PlnType::Kind::Ptr && to->kind == PlnType::Kind::Ptr) {
        // base pointers are interned so pointer equality is sufficient
        const auto* pf = static_cast<const PtrType*>(from);
        const auto* pt = static_cast<const PtrType*>(to);
        return (pf->base == pt->base) ? TypeCompat::Identical : TypeCompat::Incompatible;
    }

    return TypeCompat::Incompatible;
}
