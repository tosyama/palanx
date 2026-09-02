#include <gtest/gtest.h>
#include <set>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, basic_tests) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/000_temp_c_header.h");
    ASSERT_EQ(output, "int testproc(){H((A+H(1)));123;return xSz;}");
}

TEST(c2ast, va_args) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/001_va_args.h");
    ASSERT_EQ(output, "int testproc(){mylog(\"hello\",1,2);}");
}

TEST(c2ast, token_paste_nonid) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/002_token_paste.h");
    ASSERT_EQ(output, "int testproc(){return val1;return 1val;return 12;return int_t;}");
}

TEST(c2ast, std_headers) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -ds stdio.h");
	ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}

TEST(c2ast, pragma_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/003_pragma_once_main.h");
    ASSERT_EQ(output, "int result=1;");
}
TEST(c2ast, char_const_in_if) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/004_char_const.h");
    ASSERT_EQ(output, "int char_result=1;");
}

TEST(c2ast, token_paste_keyword) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/005_token_paste_kw.h");
    ASSERT_EQ(output, "int x;");
}

TEST(c2ast, stringizing) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/007_stringify.h");
    ASSERT_EQ(output, "int n=\"hello\";char *s=\"1 + 2\";");
}

TEST(c2ast, warning_directive) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/006_warning.h");
    ASSERT_TRUE(output.find("int warning_test=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: this is a test warning") != string::npos);
}

TEST(c2ast, stdio_functions_in_ast) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s stdio.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name && f["func-type"] == "c") return &f;
        return nullptr;
    };

    // printf: int printf(const char *format, ...)
    {
        json* pf = find_func("printf");
        ASSERT_NE(pf, nullptr);
        // ret-type: int
        ASSERT_EQ((*pf)["ret-type"]["type-kind"], "prim");
        ASSERT_EQ((*pf)["ret-type"]["type-name"], "int32");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 2u);
        ASSERT_EQ(params.back()["name"], "...");
        // first param: const char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        ASSERT_EQ(p0vt["base-type"]["const"], true);
    }

    // fgets: char *fgets(char *s, int n, FILE *stream)
    {
        json* pf = find_func("fgets");
        ASSERT_NE(pf, nullptr);
        // ret-type: char *
        auto& ret = (*pf)["ret-type"];
        ASSERT_EQ(ret["type-kind"], "pntr");
        ASSERT_EQ(ret["base-type"]["type-kind"], "prim");
        ASSERT_EQ(ret["base-type"]["type-name"], "int8");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 3u);
        // first param: char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        // second param: int
        auto& p1vt = params[1]["var-type"];
        ASSERT_EQ(p1vt["type-kind"], "prim");
        ASSERT_EQ(p1vt["type-name"], "int32");
    }
}

TEST(c2ast, include_macro) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/008_include_macro.h");
    ASSERT_EQ(output, "int macro_include_result=1;");
}

TEST(c2ast, include_next_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/010_include_next.h");
    ASSERT_TRUE(output.find("int include_next_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: '#include_next' is not supported, ignored") != string::npos);
}

TEST(c2ast, line_directive_warning_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/011_line_directive.h");
    ASSERT_TRUE(output.find("int line_directive_result=1;") != string::npos);
    // warning appears exactly once despite two #line directives
    size_t first = output.find("warning: '#line' is not supported");
    ASSERT_NE(first, string::npos);
    ASSERT_EQ(output.find("warning: '#line' is not supported", first + 1), string::npos);
}

TEST(c2ast, unknown_directive_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/012_unknown_directive.h");
    ASSERT_TRUE(output.find("int unknown_dir_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: unsupported directive '#ident'") != string::npos);
}

TEST(c2ast, include_macro_bracket) {
    cleanTestEnv();
    string output;
    output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/009_include_macro_bracket.h");
    ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}

TEST(c2ast, struct_enum_typedef) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/013_struct_enum.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // get_color() returns typedef enum Color
    {
        json* f = find_func("get_color");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ((*f)["ret-type"]["type-kind"], "user");
        ASSERT_EQ((*f)["ret-type"]["type-name"], "Color");
    }

    // make_point() returns typedef struct Point
    {
        json* f = find_func("make_point");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ((*f)["ret-type"]["type-kind"], "user");
        ASSERT_EQ((*f)["ret-type"]["type-name"], "Point");
    }

    // point_sum() takes Point param
    {
        json* f = find_func("point_sum");
        ASSERT_NE(f, nullptr);
        auto& p0vt = (*f)["parameters"][0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "user");
        ASSERT_EQ(p0vt["type-name"], "Point");
    }
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

    // struct Missing; (forward declaration only) is not registered
    ASSERT_EQ(find_struct("Missing"), nullptr);

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

    // take_missing()'s struct Missing* parameter stays without a type-name,
    // since Missing was never captured with a field list.
    json* take_missing = find_func("take_missing");
    ASSERT_NE(take_missing, nullptr);
    auto& m_vt = (*take_missing)["parameters"][0]["var-type"];
    ASSERT_EQ(m_vt["type-kind"], "pntr");
    ASSERT_EQ(m_vt["base-type"]["type-kind"], "strct");
    ASSERT_FALSE(m_vt["base-type"].contains("type-name"));
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

TEST(c2ast, typedef_struct_unresolved) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/017_typedef_struct_unresolved.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* h = find_func("h");
    ASSERT_NE(h, nullptr);
    ASSERT_EQ((*h)["ret-type"]["type-kind"], "user");
    ASSERT_EQ((*h)["ret-type"]["type-name"], "Point");
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
}

TEST(c2ast, relational_ops) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/014_relational_ops.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    bool found = false;
    for (auto& f : functions) if (f["name"] == "f") found = true;
    ASSERT_TRUE(found);
}

TEST(c2ast, ctype_h_parses) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s ctype.h");
    json ast = json::parse(output);
    auto& fns = ast["ast"]["functions"];
    std::set<string> names;
    for (auto& f : fns) names.insert(f["name"].get<string>());

    // isctype is excluded: guarded by #ifdef __USE_GNU in glibc's ctype.h,
    // which is not among this environment's predefined feature-test macros.
    static const char* targets[] = {
        "isalnum", "isalpha", "iscntrl", "isdigit", "islower", "isgraph",
        "isprint", "ispunct", "isspace", "isupper", "isxdigit", "isblank",
        "tolower", "toupper", "isascii", "toascii", "_toupper", "_tolower"
    };
    for (auto* name : targets)
        ASSERT_TRUE(names.count(name)) << "missing function: " << name;
}

TEST(c2ast, time_h_struct_pointer) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s time.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    // gmtime returns struct tm*, and struct tm is captured (defined earlier
    // in the header chain via bits/types/struct_tm.h) so the pointer's
    // base-type carries a type-name back to it.
    json* gmtime = nullptr;
    for (auto& f : functions)
        if (f["name"] == "gmtime") { gmtime = &f; break; }
    ASSERT_NE(gmtime, nullptr);
    auto& ret = (*gmtime)["ret-type"];
    ASSERT_EQ(ret["type-kind"], "pntr");
    ASSERT_EQ(ret["base-type"]["type-kind"], "strct");
    ASSERT_EQ(ret["base-type"]["type-name"], "tm");

    auto& structs = ast["ast"]["structs"];
    json* tm = nullptr;
    for (auto& s : structs)
        if (s["name"] == "tm") { tm = &s; break; }
    ASSERT_NE(tm, nullptr);
    ASSERT_GT((*tm)["fields"].size(), 0);

    // asctime(const struct tm *tp): pointee const on a struct-typed pointer
    // parameter must be captured (IT-2026-08-31-c2ast-const-capture).
    json* asctime = nullptr;
    for (auto& f : functions)
        if (f["name"] == "asctime") { asctime = &f; break; }
    ASSERT_NE(asctime, nullptr);
    auto& asctime_p0 = (*asctime)["parameters"][0]["var-type"];
    ASSERT_EQ(asctime_p0["type-kind"], "pntr");
    ASSERT_EQ(asctime_p0["base-type"]["type-kind"], "strct");
    ASSERT_EQ(asctime_p0["base-type"]["const"], true);
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

TEST(c2ast, macro_const_simple) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/018_macro_const_simple.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    json* magic = find_const("MAGIC");
    ASSERT_NE(magic, nullptr);
    ASSERT_EQ((*magic)["value"], "42");
    ASSERT_EQ((*magic)["value-type"]["type-kind"], "prim");
    ASSERT_EQ((*magic)["value-type"]["type-name"], "int32");

    // Not a recognized simple constant form: silently skipped, not an error.
    ASSERT_EQ(find_const("COMPLEX"), nullptr);
}

TEST(c2ast, macro_const_null) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/019_macro_const_null.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    json* null_const = nullptr;
    for (auto& c : constants)
        if (c["name"] == "NULL") { null_const = &c; break; }
    ASSERT_NE(null_const, nullptr);
    ASSERT_EQ((*null_const)["value"], "0");
    ASSERT_EQ((*null_const)["value-type"]["type-kind"], "pntr");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-kind"], "prim");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-name"], "void");
}

TEST(c2ast, macro_const_alias_chain) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/024_macro_const_alias.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    for (const string& name : {"A", "B", "C"}) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], "5");
        ASSERT_EQ((*c)["value-type"]["type-kind"], "prim");
        ASSERT_EQ((*c)["value-type"]["type-name"], "int32");
    }

    // Expands to an additive expression, not a recognized constant shape: still skipped.
    ASSERT_EQ(find_const("D"), nullptr);
}

TEST(c2ast, sys_stat_h_public_names) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s sys/stat.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    // S_IFDIR is defined as an alias of __S_IFDIR; it must be exported too, not just
    // the internal name.
    json* s_ifdir = nullptr;
    for (auto& c : constants)
        if (c["name"] == "S_IFDIR") { s_ifdir = &c; break; }
    ASSERT_NE(s_ifdir, nullptr);
    ASSERT_EQ((*s_ifdir)["value"], "16384");
    ASSERT_EQ((*s_ifdir)["value-type"]["type-kind"], "prim");
    ASSERT_EQ((*s_ifdir)["value-type"]["type-name"], "int32");
}

TEST(c2ast, string_h_null_constant) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s string.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    // NULL comes from stddef.h, pulled in transitively via string.h.
    json* null_const = nullptr;
    for (auto& c : constants)
        if (c["name"] == "NULL") { null_const = &c; break; }
    ASSERT_NE(null_const, nullptr);
    ASSERT_EQ((*null_const)["value"], "0");
    ASSERT_EQ((*null_const)["value-type"]["type-kind"], "pntr");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-kind"], "prim");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-name"], "void");
}

TEST(c2ast, array_decl) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/023_array_decl.h");
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

    // char name[16]; -- 1D array field, prim leaf
    json* rec = find_struct("Rec");
    ASSERT_NE(rec, nullptr);
    json* name_field = find_field((*rec)["fields"], "name");
    ASSERT_NE(name_field, nullptr);
    auto& name_vt = (*name_field)["var-type"];
    ASSERT_EQ(name_vt["type-kind"], "arr");
    ASSERT_EQ(name_vt["embedded"], true);
    ASSERT_EQ(name_vt["specifier"], "raw");
    ASSERT_EQ(name_vt["size-expr"]["expr-type"], "lit-int");
    ASSERT_EQ(name_vt["size-expr"]["value"], "16");
    ASSERT_EQ(name_vt["base-type"]["type-kind"], "prim");
    ASSERT_EQ(name_vt["base-type"]["type-name"], "int8");

    // struct Point pts[3]; -- 1D array field, struct leaf (kept as "strct" -- SA
    // normalizes this to "prim" at the registration boundary, not c2ast)
    json* pts_field = find_field((*rec)["fields"], "pts");
    ASSERT_NE(pts_field, nullptr);
    auto& pts_vt = (*pts_field)["var-type"];
    ASSERT_EQ(pts_vt["type-kind"], "arr");
    ASSERT_EQ(pts_vt["size-expr"]["value"], "3");
    ASSERT_EQ(pts_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(pts_vt["base-type"]["type-name"], "Point");

    // int cells[2][3]; -- 2D array field, nested arr (outer dim first)
    json* grid = find_struct("Grid2D");
    ASSERT_NE(grid, nullptr);
    json* cells_field = find_field((*grid)["fields"], "cells");
    ASSERT_NE(cells_field, nullptr);
    auto& cells_vt = (*cells_field)["var-type"];
    ASSERT_EQ(cells_vt["type-kind"], "arr");
    ASSERT_EQ(cells_vt["size-expr"]["value"], "2");
    auto& cells_inner = cells_vt["base-type"];
    ASSERT_EQ(cells_inner["type-kind"], "arr");
    ASSERT_EQ(cells_inner["size-expr"]["value"], "3");
    ASSERT_EQ(cells_inner["base-type"]["type-name"], "int32");

    // #define N 4; int arr[N]; -- macro-sized array resolves to a literal
    json* macro_sized = find_struct("MacroSized");
    ASSERT_NE(macro_sized, nullptr);
    json* arr_field = find_field((*macro_sized)["fields"], "arr");
    ASSERT_NE(arr_field, nullptr);
    ASSERT_EQ((*arr_field)["var-type"]["size-expr"]["value"], "4");

    // int f(char buf[32], struct Rec recs[2]); -- parameter array decay
    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    auto& buf_vt = (*f)["parameters"][0]["var-type"];
    ASSERT_EQ(buf_vt["type-kind"], "pntr");
    ASSERT_EQ(buf_vt["base-type"]["type-kind"], "prim");
    ASSERT_EQ(buf_vt["base-type"]["type-name"], "int8");
    auto& recs_vt = (*f)["parameters"][1]["var-type"];
    ASSERT_EQ(recs_vt["type-kind"], "pntr");
    ASSERT_EQ(recs_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(recs_vt["base-type"]["type-name"], "Rec");

    // int g(char buf[2][3]); -- only the outer dimension decays
    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    auto& g_buf_vt = (*g)["parameters"][0]["var-type"];
    ASSERT_EQ(g_buf_vt["type-kind"], "pntr");
    ASSERT_EQ(g_buf_vt["base-type"]["type-kind"], "arr");
    ASSERT_EQ(g_buf_vt["base-type"]["size-expr"]["value"], "3");
    ASSERT_EQ(g_buf_vt["base-type"]["base-type"]["type-name"], "int8");
}
