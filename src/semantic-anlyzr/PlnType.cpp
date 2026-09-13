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
        toEnum["void"]   = PrimType::Name::Void;
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
} // LCOV_EXCL_EXCEPTION_BR_LINE

const PtrType* PlnTypeRegistry::ptr(const PlnType* base)
{
    auto it = ptrCache_.find(base);
    if (it != ptrCache_.end()) return it->second.get();
    auto [ins, ok] = ptrCache_.emplace(base, std::make_unique<PtrType>(base));
    return ins->second.get();
} // LCOV_EXCL_EXCEPTION_BR_LINE

const StructType* PlnTypeRegistry::structType(const std::string& name)
{
    auto it = structCache_.find(name);
    if (it != structCache_.end()) return it->second.get();
    auto [ins, ok] = structCache_.emplace(name, std::make_unique<StructType>(name));
    return ins->second.get();
} // LCOV_EXCL_EXCEPTION_BR_LINE

// Kept in sync with fromJson's three accepted shapes below by construction:
// fromJson calls this first and refuses to proceed unless it returns "", so
// the two cannot silently drift apart the way a second, independently
// maintained predicate could.
std::string unrepresentableTypeName(const json& j)
{
    if (!j.is_object() || !j.contains("type-kind")) // LCOV_EXCL_BR_LINE -- no real producer emits this
        return "malformed type"; // LCOV_EXCL_LINE
    std::string kind = j["type-kind"].get<std::string>();
    if (kind == "prim") {
        std::string tname = j.value("type-name", "");
        if (tname.empty()) return "malformed type"; // LCOV_EXCL_BR_LINE -- no real producer emits this
        return PrimTypeNames::instance().toEnum.count(tname) ? "" : tname; // LCOV_EXCL_EXCEPTION_BR_LINE
    }
    if (kind == "pntr")
        return j.contains("base-type") ? unrepresentableTypeName(j["base-type"]) : "malformed type"; // LCOV_EXCL_EXCEPTION_BR_LINE
    if (kind == "struct")
        return j.contains("type-name") ? "" : "anonymous struct";
    if (kind == "arr")   return "array"; // LCOV_EXCL_BR_LINE -- arrays always decay to pntr before reaching here
    if (kind == "func")  return "function pointer";
    if (kind == "union") return "union";
    if (kind == "enum")  return "enum";
    // "strct": c2ast's pre-normalization struct tag (normalizeCType folds it
    // to "struct" before a cinclude'd signature reaches fromJson, but this
    // predicate is also usable ahead of that fold).
    if (kind == "strct") return j.value("type-name", "anonymous struct"); // LCOV_EXCL_BR_LINE -- normalizeCType always folds this away first
    // "user": a typedef name c2ast could not resolve to a known underlying
    // type -- report the name itself, it is more useful than "user".
    if (kind == "user")  return j.value("type-name", "user"); // LCOV_EXCL_EXCEPTION_BR_LINE
    return kind;
} // LCOV_EXCL_EXCEPTION_BR_LINE

std::string typeDisplayName(const json& j)
{
    if (!j.is_object() || !j.contains("type-kind")) // LCOV_EXCL_BR_LINE -- no real producer emits this
        return "malformed type"; // LCOV_EXCL_LINE
    std::string kind = j["type-kind"].get<std::string>();
    if (kind == "prim")
        return j.value("type-name", "malformed type"); // LCOV_EXCL_BR_LINE -- no real producer emits this
    if (kind == "pntr") {
        if (!j.contains("base-type")) return "malformed type"; // LCOV_EXCL_LINE -- no real producer emits this
        bool mut = j.value("mutable", true);
        return (mut ? "@!" : "@") + typeDisplayName(j["base-type"]);
    }
    if (kind == "struct")
        return j.value("type-name", "anonymous struct");
    // Every other kind is already unrepresentable; its display name there is
    // exactly what a diagnostic needs here too.
    return unrepresentableTypeName(j);
} // LCOV_EXCL_EXCEPTION_BR_LINE

const PlnType* PlnTypeRegistry::fromJson(const json& j)
{
    std::string bad = unrepresentableTypeName(j);
    if (!bad.empty()) // LCOV_EXCL_BR_LINE -- callers only reach here with an already-validated type
        throw std::runtime_error("unrepresentable type: " + bad); // LCOV_EXCL_LINE
    std::string kind = j["type-kind"].get<std::string>();
    if (kind == "prim") {
        auto& toEnum = PrimTypeNames::instance().toEnum;
        return prim(toEnum.at(j["type-name"].get<std::string>()));
    }
    if (kind == "pntr")
        return ptr(fromJson(j["base-type"]));
    // kind == "struct" (the only remaining possibility once unrepresentableTypeName
    // returns "")
    return structType(j["type-name"].get<std::string>());
} // LCOV_EXCL_EXCEPTION_BR_LINE

json PlnTypeRegistry::toJson(const PlnType* t)
{
    if (t->kind == PlnType::Kind::Prim) {
        const auto* p = static_cast<const PrimType*>(t);
        auto& fromEnum = PrimTypeNames::instance().fromEnum;
        return {{"type-kind", "prim"}, {"type-name", fromEnum.at(p->name)}};
    }
    // LCOV_EXCL_START — Ptr/Struct toJson not reachable from current SA flow
    if (t->kind == PlnType::Kind::Ptr) {
        const auto* p = static_cast<const PtrType*>(t);
        return {{"type-kind", "pntr"}, {"base-type", toJson(p->base)}};
    }
    if (t->kind == PlnType::Kind::Struct) {
        const auto* s = static_cast<const StructType*>(t);
        return {{"type-kind", "struct"}, {"type-name", s->name}};
    }
    throw std::runtime_error("unknown PlnType::Kind");
    // LCOV_EXCL_STOP
} // LCOV_EXCL_EXCEPTION_BR_LINE

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
    return -1; // LCOV_EXCL_LINE
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
    return -1; // LCOV_EXCL_LINE
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
        if (pf->base == pt->base) return TypeCompat::Identical;

        bool fromIsVoid = pf->base->kind == PlnType::Kind::Prim
            && static_cast<const PrimType*>(pf->base)->name == PrimType::Name::Void;
        bool toIsVoid = pt->base->kind == PlnType::Kind::Prim
            && static_cast<const PrimType*>(pt->base)->name == PrimType::Name::Void;
        if (fromIsVoid || toIsVoid) return TypeCompat::Identical;  // pntr(void) is bidirectionally compatible with any pntr(T)

        // pntr(int8) <-> pntr(uint8): the documented "C uint8* / char*
        // convention" (PalanReference.md "Passing to C Functions") for
        // string/byte-buffer interop -- a cinclude'd `char *`/`const char *`
        // normalizes its pointee to int8 (see normalizeCType), while a
        // Palan string literal and an array-decayed `[n]uint8` both use
        // uint8. This is a pointer-level exception only: as scalar value
        // types int8/uint8 remain fully distinct and still require an
        // explicit cast (see the Prim/Prim branch above).
        if (pf->base->kind == PlnType::Kind::Prim && pt->base->kind == PlnType::Kind::Prim) {
            auto bf = static_cast<const PrimType*>(pf->base)->name;
            auto bt = static_cast<const PrimType*>(pt->base)->name;
            bool fromIsByte = bf == PrimType::Name::Int8 || bf == PrimType::Name::Uint8;
            bool toIsByte   = bt == PrimType::Name::Int8 || bt == PrimType::Name::Uint8;
            if (fromIsByte && toIsByte) return TypeCompat::Identical;
        }

        return TypeCompat::Incompatible;
    }

    return TypeCompat::Incompatible;
}

const PlnType* usualArithConv(const PlnType* a, const PlnType* b)
{
    if (a->kind != PlnType::Kind::Prim || b->kind != PlnType::Kind::Prim) return nullptr;
    const auto* pa = static_cast<const PrimType*>(a);
    const auto* pb = static_cast<const PrimType*>(b);

    int ga = primGroup(pa->name), gb = primGroup(pb->name);
    if (ga < 0 || gb < 0) return nullptr; // Void is not a valid operand type; LCOV_EXCL_BR_LINE -- already rejected upstream (E_VoidCallUsedAsValue)

    // 1. Either side float -> the wider float wins (both sides float: wider; one
    //    side integer: the float side, per the existing int-to-float ImplicitWiden rule).
    if (ga == 2 || gb == 2) {
        if (ga == 2 && gb == 2) return primRank(pa->name) >= primRank(pb->name) ? a : b;
        return ga == 2 ? a : b;
    }
    // 2. Same signedness -> higher rank wins.
    if (ga == gb) return primRank(pa->name) >= primRank(pb->name) ? a : b;
    // 3/4. Mixed signedness: signed wins only if its rank is strictly greater
    //      than the unsigned side's rank; otherwise the unsigned type wins
    //      (matches C's usual arithmetic conversions on same-size ranks).
    const PlnType* signedT   = (ga == 0) ? a : b;
    const PlnType* unsignedT = (ga == 0) ? b : a;
    auto signedName   = static_cast<const PrimType*>(signedT)->name;
    auto unsignedName = static_cast<const PrimType*>(unsignedT)->name;
    return primRank(signedName) > primRank(unsignedName) ? signedT : unsignedT;
}

bool argConvOk(const PlnType* from, const PlnType* to)
{
    if (usualArithConv(from, to) == to) return true;
    if (from->kind != PlnType::Kind::Prim || to->kind != PlnType::Kind::Prim) return false;
    const auto* pf = static_cast<const PrimType*>(from);
    const auto* pt = static_cast<const PrimType*>(to);
    int gf = primGroup(pf->name), gt = primGroup(pt->name);
    if (gf < 0 || gt < 0 || gf == 2 || gt == 2) return false;  // float pairs handled above
    return primRank(pf->name) == primRank(pt->name);           // same-width sign reinterpretation
}
