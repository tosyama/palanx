/// c2ast tests: struct, union, enum and typedef capture
///
/// @file typeTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, struct_enum_typedef) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/013_struct_enum.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // get_color() returns typedef enum Color -- the anonymous body is named
    // after the typedef, as for a struct.
    {
        json* f = find_func("get_color");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ((*f)["ret-type"]["type-kind"], "enum");
        ASSERT_EQ((*f)["ret-type"]["type-name"], "Color");
    }

    // make_point() returns typedef struct Point -- the anonymous body's tag is
    // synthesized from the typedef name "Point".
    {
        json* f = find_func("make_point");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ((*f)["ret-type"]["type-kind"], "strct");
        ASSERT_EQ((*f)["ret-type"]["type-name"], "Point");
        ASSERT_EQ((*f)["ret-type"]["typedef-name"], "Point");
    }

    // point_sum() takes Point param
    {
        json* f = find_func("point_sum");
        ASSERT_NE(f, nullptr);
        auto& p0vt = (*f)["parameters"][0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "strct");
        ASSERT_EQ(p0vt["type-name"], "Point");
    }

    // The synthesized tag registers Point's field list in "structs", same as
    // a tagged struct would.
    json* point = nullptr;
    for (auto& s : structs)
        if (s["name"] == "Point") { point = &s; break; }
    ASSERT_NE(point, nullptr);
    ASSERT_TRUE(point->contains("fields"));
    ASSERT_EQ((*point)["fields"].size(), 2);
}

TEST(c2ast, struct_union_decl_backtrack) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/020_struct_decl_backtrack.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // "struct Point make_point(...)" (no extern) must not be swallowed by the
    // standalone struct-declaration fast path — it should parse as a function
    // returning struct Point.
    json* make_point = find_func("make_point");
    ASSERT_NE(make_point, nullptr);
    ASSERT_EQ((*make_point)["ret-type"]["type-kind"], "strct");

    json* make_pair = find_func("make_pair");
    ASSERT_NE(make_pair, nullptr);
    ASSERT_EQ((*make_pair)["ret-type"]["type-kind"], "union");
}

TEST(c2ast, struct_capture) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/021_struct_capture.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };

    // struct Point { int x; int y; }; is captured with its field list
    json* point = find_struct("Point");
    ASSERT_NE(point, nullptr);
    auto& fields = (*point)["fields"];
    ASSERT_EQ(fields.size(), 2);
    ASSERT_EQ(fields[0]["name"], "x");
    ASSERT_EQ(fields[0]["var-type"]["type-kind"], "prim");
    ASSERT_EQ(fields[0]["var-type"]["type-name"], "int32");
    ASSERT_EQ(fields[1]["name"], "y");

    // struct Missing; (forward declaration only) is registered without a
    // "fields" key, so a tag referenced only through a pointer still gets an
    // entry (needed for SA to register it as an incomplete struct type).
    json* missing = find_struct("Missing");
    ASSERT_NE(missing, nullptr);
    ASSERT_FALSE(missing->contains("fields"));

    // make_point()'s return type references the captured struct by name
    json* make_point = find_func("make_point");
    ASSERT_NE(make_point, nullptr);
    ASSERT_EQ((*make_point)["ret-type"]["type-kind"], "strct");
    ASSERT_EQ((*make_point)["ret-type"]["type-name"], "Point");

    // move_point()'s struct Point* parameter also gets the type-name
    json* move_point = find_func("move_point");
    ASSERT_NE(move_point, nullptr);
    auto& p_vt = (*move_point)["parameters"][0]["var-type"];
    ASSERT_EQ(p_vt["type-kind"], "pntr");
    ASSERT_EQ(p_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(p_vt["base-type"]["type-name"], "Point");

    // take_missing()'s struct Missing* parameter keeps the tag name even
    // though Missing was never captured with a field list -- a tag reference
    // always carries its name now, regardless of whether a definition exists.
    json* take_missing = find_func("take_missing");
    ASSERT_NE(take_missing, nullptr);
    auto& m_vt = (*take_missing)["parameters"][0]["var-type"];
    ASSERT_EQ(m_vt["type-kind"], "pntr");
    ASSERT_EQ(m_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(m_vt["base-type"]["type-name"], "Missing");
}

TEST(c2ast, typedef_scalar) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/015_typedef_scalar.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ((*f)["ret-type"]["type-kind"], "prim");
    ASSERT_EQ((*f)["ret-type"]["type-name"], "uint64");
    ASSERT_EQ((*f)["ret-type"]["typedef-name"], "my_size_t");
}

TEST(c2ast, typedef_chain) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/016_typedef_chain.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ((*g)["ret-type"]["type-kind"], "prim");
    ASSERT_EQ((*g)["ret-type"]["type-name"], "uint64");
    ASSERT_EQ((*g)["ret-type"]["typedef-name"], "level2_t");
}

TEST(c2ast, typedef_anon_union) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/017_typedef_anon_union.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    json* h = nullptr;
    for (auto& f : functions)
        if (f["name"] == "h") h = &f;
    ASSERT_NE(h, nullptr);
    ASSERT_EQ((*h)["ret-type"]["type-kind"], "union");
    ASSERT_EQ((*h)["ret-type"]["type-name"], "U");

    ASSERT_EQ(structs.size(), 1u);
    ASSERT_EQ(structs[0]["name"], "U");
    ASSERT_EQ(structs[0]["union"], true);
    ASSERT_EQ(structs[0]["fields"].size(), 1u);
}

TEST(c2ast, typedef_pointer_chain) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/022_typedef_pointer_chain.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ((*g)["ret-type"]["type-kind"], "pntr");
    ASSERT_EQ((*g)["ret-type"]["base-type"]["type-kind"], "prim");
    ASSERT_EQ((*g)["ret-type"]["base-type"]["type-name"], "void");
    ASSERT_EQ((*g)["ret-type"]["typedef-name"], "level2_t");

    ASSERT_EQ(ast["ast"]["typedefs"], json::parse(R"([
        {"name":"level1_t","var-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"void"}}},
        {"name":"level2_t","var-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"void"}}}])"));
}

TEST(c2ast, typedef_struct_tag) {
    // struct Fwd;              -- bare forward declaration, never defined
    // typedef struct Body X;   -- typedef of a tag not yet defined at this point
    // struct Body { int a; };  -- the definition arrives later
    // struct Never *use(X *x); -- X used as a parameter type; Never never declared at all
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/027_typedef_struct.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };

    // X (typedef of "struct Body") resolves to the tag, regardless of
    // definition order -- the typedef appears before Body's field-bearing
    // definition in the source.
    json* use = find_func("use");
    ASSERT_NE(use, nullptr);
    auto& x_vt = (*use)["parameters"][0]["var-type"];
    ASSERT_EQ(x_vt["type-kind"], "pntr");
    ASSERT_EQ(x_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(x_vt["base-type"]["type-name"], "Body");
    ASSERT_EQ(x_vt["base-type"]["typedef-name"], "X");

    // Fwd (forward-declared only) and Never (referenced only, never declared)
    // both get an entry without a "fields" key.
    json* fwd = find_struct("Fwd");
    ASSERT_NE(fwd, nullptr);
    ASSERT_FALSE(fwd->contains("fields"));
    json* never = find_struct("Never");
    ASSERT_NE(never, nullptr);
    ASSERT_FALSE(never->contains("fields"));

    // Body has exactly one entry, carrying its field list -- the typedef's
    // earlier tag-only reference doesn't create a duplicate.
    int bodyCount = 0;
    for (auto& s : structs)
        if (s["name"] == "Body") bodyCount++;
    ASSERT_EQ(bodyCount, 1);
    json* body = find_struct("Body");
    ASSERT_NE(body, nullptr);
    ASSERT_TRUE(body->contains("fields"));
    ASSERT_EQ((*body)["fields"].size(), 1);
    ASSERT_EQ((*body)["fields"][0]["name"], "a");
}

TEST(c2ast, const_capture) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/025_const_capture.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    auto find_field = [](json& fields, const string& name) -> json* {
        for (auto& f : fields)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // const struct/union/enum *: pointee const lives on base-type, same as
    // a const-qualified primitive pointee.
    json* f_struct = find_func("f_struct");
    ASSERT_NE(f_struct, nullptr);
    auto& s_vt = (*f_struct)["parameters"][0]["var-type"];
    ASSERT_EQ(s_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(s_vt["base-type"]["const"], true);

    json* f_union = find_func("f_union");
    ASSERT_NE(f_union, nullptr);
    auto& u_vt = (*f_union)["parameters"][0]["var-type"];
    ASSERT_EQ(u_vt["base-type"]["type-kind"], "union");
    ASSERT_EQ(u_vt["base-type"]["const"], true);

    json* f_enum = find_func("f_enum");
    ASSERT_NE(f_enum, nullptr);
    auto& e_vt = (*f_enum)["parameters"][0]["var-type"];
    ASSERT_EQ(e_vt["base-type"]["type-kind"], "enum");
    ASSERT_EQ(e_vt["base-type"]["const"], true);

    // struct fields: "const"/"volatile" must not be pre-consumed and
    // discarded -- declaration_specifiers() itself captures them, in any
    // order ("volatile const" as well as the usual "const volatile").
    json* rec = find_struct("Rec");
    ASSERT_NE(rec, nullptr);
    json* a = find_field((*rec)["fields"], "a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ((*a)["var-type"]["const"], true);
    json* b = find_field((*rec)["fields"], "b");
    ASSERT_NE(b, nullptr);
    ASSERT_EQ((*b)["var-type"]["const"], true);
    json* c = find_field((*rec)["fields"], "c");
    ASSERT_NE(c, nullptr);
    ASSERT_FALSE((*c)["var-type"].contains("const"));
}

TEST(c2ast, typedef_anon_struct) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/031_typedef_anon_struct.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };

    // typedef struct { int quot; int rem; } Div; -- single, non-derived
    // declarator: the anonymous body's tag is synthesized from "Div".
    {
        json* make_div = find_func("make_div");
        ASSERT_NE(make_div, nullptr);
        ASSERT_EQ((*make_div)["ret-type"]["type-kind"], "strct");
        ASSERT_EQ((*make_div)["ret-type"]["type-name"], "Div");
        ASSERT_EQ((*make_div)["ret-type"]["typedef-name"], "Div");

        json* div_sum = find_func("div_sum");
        ASSERT_NE(div_sum, nullptr);
        auto& p0vt = (*div_sum)["parameters"][0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "strct");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "Div");

        json* div = find_struct("Div");
        ASSERT_NE(div, nullptr);
        ASSERT_TRUE(div->contains("fields"));
        ASSERT_EQ((*div)["fields"].size(), 2);
    }

    // typedef struct { int a; } Multi, *MultiPtr; -- multi-declarator: non-goal,
    // stays unresolved "user", no "Multi" entry in structs.
    {
        json* take_multi = find_func("take_multi");
        ASSERT_NE(take_multi, nullptr);
        ASSERT_EQ((*take_multi)["ret-type"]["type-kind"], "user");
        ASSERT_EQ((*take_multi)["ret-type"]["type-name"], "Multi");
        ASSERT_EQ(find_struct("Multi"), nullptr);
    }

    // typedef struct { int b; } *DerivedPtr; -- derived (pointer) declarator:
    // non-goal, the pointee stays an untagged "strct".
    {
        json* take_derived = find_func("take_derived");
        ASSERT_NE(take_derived, nullptr);
        auto& p0vt = (*take_derived)["parameters"][0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "strct");
        ASSERT_FALSE(p0vt["base-type"].contains("type-name"));
    }

    // struct Clash { int k; }; then typedef struct { int m; } Clash; -- the
    // name is already a real C tag, so synthesis is skipped rather than
    // promoting Clash's entry with these unrelated fields.
    {
        json* clash = find_struct("Clash");
        ASSERT_NE(clash, nullptr);
        ASSERT_EQ((*clash)["fields"].size(), 1);
        ASSERT_EQ((*clash)["fields"][0]["name"], "k");

        json* use_clash = find_func("use_clash");
        ASSERT_NE(use_clash, nullptr);
        ASSERT_EQ((*use_clash)["ret-type"]["type-kind"], "user");
        ASSERT_EQ((*use_clash)["ret-type"]["type-name"], "Clash");
    }
}

TEST(c2ast, union_capture) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/036_union_capture.h");
    json ast = json::parse(output);
    auto& structs = ast["ast"]["structs"];
    auto& typedefs = ast["ast"]["typedefs"];

    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    auto find_typedef = [&](const string& name) -> json* {
        for (auto& t : typedefs)
            if (t["name"] == name) return &t;
        return nullptr;
    };

    // typedef union Attr { ... } attr_t; -- the pthread_attr_t shape.
    {
        json* attr = find_struct("Attr");
        ASSERT_NE(attr, nullptr);
        ASSERT_EQ((*attr)["union"], true);
        ASSERT_EQ((*attr)["fields"].size(), 2u);

        json* attr_t = find_typedef("attr_t");
        ASSERT_NE(attr_t, nullptr);
        ASSERT_EQ((*attr_t)["var-type"]["type-kind"], "union");
        ASSERT_EQ((*attr_t)["var-type"]["type-name"], "Attr");
    }

    // union Fwd; ... union Fwd { ... }; -- promoted in place, one entry.
    {
        int n = 0;
        for (auto& s : structs)
            if (s["name"] == "Fwd") n++;
        ASSERT_EQ(n, 1);
        json* fwd = find_struct("Fwd");
        ASSERT_EQ((*fwd)["union"], true);
        ASSERT_EQ((*fwd)["fields"].size(), 1u);
    }

    // A union-typed field of a struct carries the tag name; the struct
    // entry itself has no "union" key.
    {
        json* holder = find_struct("Holder");
        ASSERT_NE(holder, nullptr);
        ASSERT_FALSE(holder->contains("union"));
        auto& u_vt = (*holder)["fields"][1]["var-type"];
        ASSERT_EQ(u_vt["type-kind"], "union");
        ASSERT_EQ(u_vt["type-name"], "Attr");
    }
}

TEST(c2ast, anon_member_tag) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/037_anon_member_tag.h");
    json ast = json::parse(output);
    auto& structs = ast["ast"]["structs"];

    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    // Resolves a member var-type's synthesized tag to its captured body.
    auto body_of = [&](const json& vt) -> json* {
        if (!vt.contains("type-name")) return nullptr;
        return find_struct(vt["type-name"].get<string>());
    };

    {
        json* outer = find_struct("Outer");
        ASSERT_NE(outer, nullptr);
        json* in = body_of((*outer)["fields"][1]["var-type"]);
        ASSERT_NE(in, nullptr);
        ASSERT_FALSE(in->contains("union"));
        ASSERT_EQ((*in)["fields"].size(), 2u);
    }

    // The __atomic_wide_counter shape: anonymous struct member inside an
    // anonymous typedef'd union.
    {
        json* wide = find_struct("wide_t");
        ASSERT_NE(wide, nullptr);
        ASSERT_EQ((*wide)["union"], true);
        json* v32 = body_of((*wide)["fields"][1]["var-type"]);
        ASSERT_NE(v32, nullptr);
        ASSERT_EQ((*v32)["fields"][1]["name"], "hi");
    }

    // Pointer and array declarators share one tag for the one body.
    {
        json* derived = find_struct("Derived");
        ASSERT_NE(derived, nullptr);
        auto& p_vt = (*derived)["fields"][0]["var-type"];
        auto& arr_vt = (*derived)["fields"][1]["var-type"];
        ASSERT_EQ(p_vt["type-kind"], "pntr");
        ASSERT_EQ(arr_vt["type-kind"], "arr");
        ASSERT_NE(body_of(p_vt["base-type"]), nullptr);
        ASSERT_EQ(p_vt["base-type"]["type-name"], arr_vt["base-type"]["type-name"]);
    }

    // Two expansions of one macro body share a source location but must
    // still get distinct tags.
    {
        json* pairs = find_struct("Pairs");
        ASSERT_NE(pairs, nullptr);
        json* ip = body_of((*pairs)["fields"][0]["var-type"]);
        json* dp = body_of((*pairs)["fields"][1]["var-type"]);
        ASSERT_NE(ip, nullptr);
        ASSERT_NE(dp, nullptr);
        ASSERT_NE((*ip)["name"], (*dp)["name"]);
        ASSERT_EQ((*ip)["fields"][0]["var-type"]["type-name"], "int32");
        ASSERT_EQ((*dp)["fields"][0]["var-type"]["type-name"], "flo64");
    }
}

TEST(c2ast, anon_member_unnamed) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/044_anon_member_unnamed.h");
    json ast = json::parse(output);
    auto& structs = ast["ast"]["structs"];

    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    auto anon_body = [&](const json& field) -> json* {
        if (field.contains("name")) return nullptr;
        return find_struct(field["var-type"]["type-name"].get<string>());
    };

    json* s = find_struct("S");
    ASSERT_NE(s, nullptr);
    ASSERT_EQ((*s)["fields"].size(), 2u);
    json* u = anon_body((*s)["fields"][1]);
    ASSERT_NE(u, nullptr);
    ASSERT_EQ((*u)["union"], true);
    ASSERT_EQ((*u)["fields"].size(), 3u);
    json* lohi = anon_body((*u)["fields"][2]);
    ASSERT_NE(lohi, nullptr);
    ASSERT_EQ((*lohi)["fields"][1]["name"], "hi");

    json* pair = find_struct("pair_t");
    ASSERT_NE(pair, nullptr);
    json* xy = anon_body((*pair)["fields"][1]);
    ASSERT_NE(xy, nullptr);
    ASSERT_EQ((*xy)["fields"][0]["name"], "x");
}

// A header's typedefs are flushed into their own
// ast.typedefs section regardless of whether any C function references them
// -- this is what lets a header like stdint.h (zero functions, all typedefs)
// register its types at all. See stdint_typedefs below for that end-to-end case.
TEST(c2ast, typedef_section) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/035_typedef_section.h");
    json ast = json::parse(output);
    auto& typedefs = ast["ast"]["typedefs"];

    auto find_typedef = [&](const string& name) -> json* {
        for (auto& t : typedefs)
            if (t["name"] == name) return &t;
        return nullptr;
    };

    // typedef int A; typedef A B; typedef B C; -- a multi-level chain fully
    // resolves to the bottom primitive, and none of the intermediate links'
    // "typedef-name" reference-site hints leak into the stored entry (that
    // hint belongs only to the var-type node at the reference site that
    // looked the name up, e.g. a function parameter/return -- see
    // declaration_specifiers()).
    for (const char* name : {"A", "B", "C"}) {
        json* t = find_typedef(name);
        ASSERT_NE(t, nullptr) << "expected " << name << " to be registered";
        ASSERT_EQ((*t)["var-type"]["type-kind"], "prim");
        ASSERT_EQ((*t)["var-type"]["type-name"], "int32");
        ASSERT_FALSE((*t)["var-type"].contains("typedef-name"))
            << "stale typedef-name leaked into " << name;
    }

    // typedef struct Tag S; -- a tagged-struct-bottomed typedef registers as
    // an alias for the tag itself.
    {
        json* s = find_typedef("S");
        ASSERT_NE(s, nullptr);
        ASSERT_EQ((*s)["var-type"]["type-kind"], "strct");
        ASSERT_EQ((*s)["var-type"]["type-name"], "Tag");
    }

    // typedef struct Tag Tag; -- the common C self-alias idiom. Registers the
    // same as any other tagged-struct typedef; nothing about the name
    // matching the tag is special-cased.
    {
        json* tag = find_typedef("Tag");
        ASSERT_NE(tag, nullptr);
        ASSERT_EQ((*tag)["var-type"]["type-kind"], "strct");
        ASSERT_EQ((*tag)["var-type"]["type-name"], "Tag");
    }

    // typedef struct { int m; } Anon; -- single, non-derived declarator: the
    // anonymous body's tag is synthesized from "Anon" (see typedef_anon_struct
    // above), and that synthesized tag qualifies for this section the same as
    // an explicitly-tagged struct typedef.
    {
        json* anon = find_typedef("Anon");
        ASSERT_NE(anon, nullptr);
        ASSERT_EQ((*anon)["var-type"]["type-kind"], "strct");
        ASSERT_EQ((*anon)["var-type"]["type-name"], "Anon");
    }

    {
        json* p = find_typedef("P");
        ASSERT_NE(p, nullptr);
        ASSERT_EQ((*p)["var-type"]["type-kind"], "pntr");
    }

    ASSERT_EQ(typedefs.size(), 7u);
}

TEST(c2ast, enum_capture) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/040_enum.h");
    json ast = json::parse(output);
    auto& enums = ast["ast"]["enums"];
    auto find = [](json& list, const string& name) -> json* {
        for (auto& e : list)
            if (e["name"] == name) return &e;
        return nullptr;
    };
    auto enumRef = [](const char* name) { return json{{"type-kind", "enum"}, {"type-name", name}}; };

    json* color = find(enums, "Color");
    ASSERT_NE(color, nullptr);
    ASSERT_EQ((*color)["enumerators"], json::parse(R"([
        {"name":"RED","value":"0"},{"name":"GREEN","value":"5"},{"name":"BLUE","value":"6"},
        {"name":"NEG","value":"-3"},{"name":"AFTER","value":"7"},
        {"name":"NOTV","value":"-1"}])"));
    ASSERT_NE(find(enums, "CSpace"), nullptr);   // typedef name of an anonymous body
    ASSERT_NE(find(enums, "Mode"), nullptr);     // the tag, not the typedef name
    ASSERT_EQ(find(enums, "ModeAlias"), nullptr);
    ASSERT_EQ(find(enums, "Unknown"), nullptr);  // sizeof of an incomplete struct can't be computed
    ASSERT_EQ(find(enums, "Huge"), nullptr);     // H1 overflows long long
    ASSERT_EQ(enums.size(), 5u);                 // + two anonymous bodies

    auto& typedefs = ast["ast"]["typedefs"];
    ASSERT_EQ((*find(typedefs, "ModeAlias"))["var-type"], enumRef("Mode"));

    json* s = find(ast["ast"]["structs"], "S");
    ASSERT_NE(s, nullptr);
    ASSERT_EQ((*s)["fields"][0]["var-type"], enumRef("Color"));
    ASSERT_EQ((*s)["fields"][2]["var-type"]["size-expr"], (json{{"expr-type", "lit-int"}, {"value", "7"}}));
    ASSERT_EQ((*s)["fields"][3]["var-type"]["type-kind"], "enum");

    json* f = find(ast["ast"]["functions"], "get_color");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ((*f)["ret-type"], enumRef("Color"));
    ASSERT_EQ((*find(ast["ast"]["globals"], "g_cs"))["var-type"]["type-name"], "CSpace");

    // Anonymous enumerators are untyped constants; a macro that is just a
    // named enum's enumerator keeps the enum type, an expression does not.
    auto& constants = ast["ast"]["constants"];
    ASSERT_EQ(*find(constants, "ANON_B"), (json{{"name", "ANON_B"}, {"value", "11"}}));
    ASSERT_EQ(*find(constants, "IN_A"), (json{{"name", "IN_A"}, {"value", "0"}}));
    ASSERT_EQ(*find(constants, "DEF_COLOR"),
        (json{{"name", "DEF_COLOR"}, {"value", "5"}, {"value-type", enumRef("Color")}}));
    ASSERT_EQ(*find(constants, "NEXT_COLOR"), (json{{"name", "NEXT_COLOR"}, {"value", "7"}}));
    ASSERT_EQ(*find(constants, "MODE_X"),
        (json{{"name", "MODE_X"}, {"value", "4"}, {"value-type", enumRef("Mode")}}));
    int anonA = 0;
    for (auto& c : constants) anonA += c["name"] == "ANON_A";
    for (auto& m : ast["ast"]["unexported-macros"]) ASSERT_NE(m["name"], "ANON_A");
    ASSERT_EQ(anonA, 1);
    ASSERT_EQ(find(constants, "UNKNOWN_M"), nullptr);
}

TEST(c2ast, typedef_pointer) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/042_typedef_pointer.h");
    json ast = json::parse(output);
    ASSERT_EQ(ast["ast"]["typedefs"], json::parse(R"([
        {"name":"CP","var-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"int32","const":true}}},
        {"name":"FP","var-type":{"type-kind":"pntr","base-type":{"type-kind":"func",
            "ret-type":{"type-kind":"prim","type-name":"int32"},
            "parameters":[{"var-type":{"type-kind":"prim","type-name":"int32"}}]}}},
        {"name":"P","var-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"int32"}}},
        {"name":"PC","var-type":{"type-kind":"pntr","const":true,"base-type":{"type-kind":"prim","type-name":"int32"}}},
        {"name":"PP","var-type":{"type-kind":"pntr","base-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"int8"}}}},
        {"name":"SP","var-type":{"type-kind":"pntr","base-type":{"type-kind":"strct","type-name":"S"}}},
        {"name":"VP","var-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"void"}}}])"));
}

TEST(c2ast, undeclared_typedef) {
    // The undeclared name may be an earlier cinclude's typedef, which SA resolves.
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/041_undeclared_typedef.h");
    json ast = json::parse(output);
    ASSERT_EQ(ast["ast"]["typedefs"], json::parse(R"([
        {"name":"X","var-type":{"type-kind":"user","type-name":"undeclared_t"}}])"));
    ASSERT_EQ(ast["ast"]["functions"][0]["parameters"][0]["var-type"], json::parse(R"(
        {"type-kind":"user","type-name":"undeclared_t","typedef-name":"X"})"));
}
