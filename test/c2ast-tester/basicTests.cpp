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

    // stdin/stdout/stderr: extern FILE *NAME -- captured into ast.globals as
    // pntr(strct _IO_FILE), FILE's typedef resolved to the underlying struct.
    {
        auto& globals = ast["ast"]["globals"];
        auto find_global = [&](const string& name) -> json* {
            for (auto& g : globals)
                if (g["name"] == name) return &g;
            return nullptr;
        };
        for (const char* name : {"stdin", "stdout", "stderr"}) {
            json* g = find_global(name);
            ASSERT_NE(g, nullptr);
            auto& vt = (*g)["var-type"];
            ASSERT_EQ(vt["type-kind"], "pntr");
            ASSERT_EQ(vt["base-type"]["type-kind"], "strct");
            ASSERT_EQ(vt["base-type"]["type-name"], "_IO_FILE");
        }
    }
}

TEST(c2ast, extern_globals) {
    // struct Handle;                    -- forward-declared only, never defined
    // extern int g_count;                extern double g_ratio;
    // extern struct Handle *g_handle;   -- prim/prim/pntr(strct): all captured
    // extern int g_a, g_b;              -- comma-separated: both captured
    // static int s_hidden;              -- no external linkage: not captured
    // extern char *g_names[2];          -- array type: not captured (non-goal)
    // typedef int A, B; A take_a(B b);  -- comma-separated typedef: B also
    //                                      registered, so take_a's signature
    //                                      resolves to int32/int32, not "user"
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/028_extern_globals.h");
    json ast = json::parse(output);
    auto& globals = ast["ast"]["globals"];

    auto find_global = [&](const string& name) -> json* {
        for (auto& g : globals)
            if (g["name"] == name) return &g;
        return nullptr;
    };

    json* count = find_global("g_count");
    ASSERT_NE(count, nullptr);
    ASSERT_EQ((*count)["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*count)["var-type"]["type-name"], "int32");

    json* ratio = find_global("g_ratio");
    ASSERT_NE(ratio, nullptr);
    ASSERT_EQ((*ratio)["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*ratio)["var-type"]["type-name"], "flo64");

    json* handle = find_global("g_handle");
    ASSERT_NE(handle, nullptr);
    auto& hvt = (*handle)["var-type"];
    ASSERT_EQ(hvt["type-kind"], "pntr");
    ASSERT_EQ(hvt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(hvt["base-type"]["type-name"], "Handle");

    ASSERT_NE(find_global("g_a"), nullptr);
    ASSERT_NE(find_global("g_b"), nullptr);

    ASSERT_EQ(find_global("s_hidden"), nullptr);
    ASSERT_EQ(find_global("g_names"), nullptr);

    ASSERT_EQ(globals.size(), 5u);

    // take_a's signature proves the comma-separated typedef "B" was
    // registered too (both A and B resolve to int32, not left as "user").
    auto& functions = ast["ast"]["functions"];
    json* take_a = nullptr;
    for (auto& f : functions)
        if (f["name"] == "take_a") take_a = &f;
    ASSERT_NE(take_a, nullptr);
    ASSERT_EQ((*take_a)["ret-type"]["type-kind"], "prim");
    ASSERT_EQ((*take_a)["ret-type"]["type-name"], "int32");
    ASSERT_EQ((*take_a)["parameters"][0]["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*take_a)["parameters"][0]["var-type"]["type-name"], "int32");
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
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // get_color() returns typedef enum Color -- enum bodies are never
    // tag-synthesized (tag synthesis is struct-only), so this stays "user".
    {
        json* f = find_func("get_color");
        ASSERT_NE(f, nullptr);
        ASSERT_EQ((*f)["ret-type"]["type-kind"], "user");
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
    // parameter must be captured.
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
    // Unsuffixed and uncast: untyped, like a Palan source literal.
    ASSERT_FALSE(magic->contains("value-type"));

    // An additive expression: folded now that binary operators are evaluated.
    json* complex_ = find_const("COMPLEX");
    ASSERT_NE(complex_, nullptr);
    ASSERT_EQ((*complex_)["value"], "3");
    ASSERT_FALSE(complex_->contains("value-type"));
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
        ASSERT_FALSE(c->contains("value-type"));
    }

    // Expands to an additive expression: folded now that binary operators are evaluated.
    json* d = find_const("D");
    ASSERT_NE(d, nullptr);
    ASSERT_EQ((*d)["value"], "6");
    ASSERT_FALSE(d->contains("value-type"));
}

TEST(c2ast, macro_const_fold_expr) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/029_macro_const_expr.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    auto expect_value = [&](const string& name, const string& value) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
    };

    expect_value("BITS", "448");
    expect_value("SUB", "87");     // left-associative: (100 - 10) - 3, not 100 - (10 - 3)
    expect_value("SHIFTS", "8");   // left-associative: (64 >> 2) >> 1, not 64 >> (2 >> 1)
    expect_value("DIVS", "10");    // left-associative: (100 / 5) / 2, not 100 / (5 / 2)
    expect_value("MIXED", "13");   // precedence: 2 + (3 * 4) - 1
    expect_value("MASK", "63");
    expect_value("NEG", "-2");

    // Relational operators are recognized but intentionally not folded.
    ASSERT_EQ(find_const("CMP"), nullptr);
    // Would be undefined behavior to evaluate ourselves: folds to null, not a wrong value.
    ASSERT_EQ(find_const("DIVZERO"), nullptr);
    ASSERT_EQ(find_const("BIGSHIFT"), nullptr);
    ASSERT_EQ(find_const("OVERFLOWED"), nullptr);
    // sizeof is never evaluated, so any expression containing it stays null.
    ASSERT_EQ(find_const("SIZED"), nullptr);
    // An unresolved identifier keeps the whole expression null.
    ASSERT_EQ(find_const("IDENT"), nullptr);
    json* suffixed = find_const("SUFFIXED");
    ASSERT_NE(suffixed, nullptr);
    ASSERT_EQ((*suffixed)["value"], "2");
    ASSERT_EQ((*suffixed)["value-type"]["type-name"], "int64");
}

TEST(c2ast, macro_const_suffix) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/038_macro_const_suffix.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    auto expect_const = [&](const string& name, const string& value, const string& type_name) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
        ASSERT_EQ((*c)["value-type"]["type-kind"], "prim") << "for " << name;
        ASSERT_EQ((*c)["value-type"]["type-name"], type_name) << "for " << name;
    };

    expect_const("U1", "1", "uint32");
    expect_const("UL5", "5", "uint64");
    expect_const("ULL5", "5", "uint64");
    expect_const("L5", "5", "int64");
    expect_const("H80", "2147483648", "uint32");
    expect_const("ULMAX", "18446744073709551615", "uint64");
    expect_const("SH", "262144", "uint32");
    expect_const("AREV", "262144", "uint32");
    expect_const("MASK", "255", "uint32");
    expect_const("WRAP", "4294967295", "uint32");
    expect_const("NEGU", "4294967295", "uint32");
    expect_const("MIXNEG", "0", "uint32");
    expect_const("UCH", "44", "uint8");
    expect_const("LADD", "2", "int64");

    ASSERT_EQ(find_const("BIGSH"), nullptr);
    ASSERT_EQ(find_const("SOVF"), nullptr);
    ASSERT_EQ(find_const("BIGDEC"), nullptr);
    ASSERT_EQ(find_const("FLO"), nullptr);
}

TEST(c2ast, int_constant_width) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s stdlib.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    // Out of int32 range: the value is kept exactly rather than truncated.
    json* wclone = find_const("__WCLONE");
    ASSERT_NE(wclone, nullptr);
    ASSERT_EQ((*wclone)["value"], "2147483648");
    ASSERT_FALSE(wclone->contains("value-type"));

    json* exit_failure = find_const("EXIT_FAILURE");
    ASSERT_NE(exit_failure, nullptr);
    ASSERT_EQ((*exit_failure)["value"], "1");
    ASSERT_FALSE(exit_failure->contains("value-type"));
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
    ASSERT_FALSE(s_ifdir->contains("value-type"));
}

TEST(c2ast, sys_stat_h_perm_masks) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s sys/stat.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    // These are all expression-bodied macros (OR/shift of other macros), previously
    // silently skipped because binary operators weren't folded.
    auto expect_value = [&](const string& name, const string& value) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
    };

    expect_value("S_IRWXU", "448");
    expect_value("S_IRGRP", "32");
    expect_value("S_IRWXG", "56");
    expect_value("S_IROTH", "4");
    expect_value("S_IRWXO", "7");
    expect_value("ACCESSPERMS", "511");
    expect_value("ALLPERMS", "4095");
    expect_value("DEFFILEMODE", "438");
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

TEST(c2ast, ptr_array_decl) {
    // Regression guard: postfix suffixes ('[n]', '(params)') must bind
    // tighter than the prefix '*', so
    // "int *a[3]" (array of pointers) and "int (*a)[3]" (pointer to array) must
    // NOT come out swapped.
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/026_ptr_array_decl.h");
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

    json* ptr_arr = find_struct("PtrArr");
    ASSERT_NE(ptr_arr, nullptr);

    // int *ptrs[3]; -- array of 3 pointers to int
    json* ptrs = find_field((*ptr_arr)["fields"], "ptrs");
    ASSERT_NE(ptrs, nullptr);
    auto& ptrs_vt = (*ptrs)["var-type"];
    ASSERT_EQ(ptrs_vt["type-kind"], "arr");
    ASSERT_EQ(ptrs_vt["size-expr"]["value"], "3");
    ASSERT_EQ(ptrs_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(ptrs_vt["base-type"]["base-type"]["type-name"], "int32");

    // int (*grouped)[3]; -- pointer to an array of 3 ints
    json* grouped = find_field((*ptr_arr)["fields"], "grouped");
    ASSERT_NE(grouped, nullptr);
    auto& grouped_vt = (*grouped)["var-type"];
    ASSERT_EQ(grouped_vt["type-kind"], "pntr");
    ASSERT_EQ(grouped_vt["base-type"]["type-kind"], "arr");
    ASSERT_EQ(grouped_vt["base-type"]["size-expr"]["value"], "3");
    ASSERT_EQ(grouped_vt["base-type"]["base-type"]["type-name"], "int32");

    // void (*fp)(int); -- pointer to function, no longer needing the removed
    // is_grouped special case to avoid mis-parsing as a plain function.
    json* fp = find_field((*ptr_arr)["fields"], "fp");
    ASSERT_NE(fp, nullptr);
    auto& fp_vt = (*fp)["var-type"];
    ASSERT_EQ(fp_vt["type-kind"], "pntr");
    ASSERT_EQ(fp_vt["base-type"]["type-kind"], "func");
    ASSERT_EQ(fp_vt["base-type"]["ret-type"]["type-name"], "void");

    // int f(char *argv[]); -- array-of-pointer parameter decay: only the outer
    // array dimension decays, leaving "pointer to pointer to char".
    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    auto& argv_vt = (*f)["parameters"][0]["var-type"];
    ASSERT_EQ(argv_vt["type-kind"], "pntr");
    ASSERT_EQ(argv_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(argv_vt["base-type"]["base-type"]["type-name"], "int8");

    // int * const * p; -- const binds to the inner pointer (the one closer to
    // the base type), matching "p is a non-const pointer to a const pointer
    // to int".
    json* qual_ptr = find_struct("QualPtr");
    ASSERT_NE(qual_ptr, nullptr);
    json* p = find_field((*qual_ptr)["fields"], "p");
    ASSERT_NE(p, nullptr);
    auto& p_vt = (*p)["var-type"];
    ASSERT_EQ(p_vt["type-kind"], "pntr");
    ASSERT_FALSE(p_vt.contains("const"));
    ASSERT_EQ(p_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(p_vt["base-type"]["const"], true);
}

TEST(c2ast, array_size_expr_fold) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/030_array_size_expr.h");
    json ast = json::parse(output);
    auto& structs = ast["ast"]["structs"];

    json* sizes = nullptr;
    for (auto& s : structs)
        if (s["name"] == "Sizes") { sizes = &s; break; }
    ASSERT_NE(sizes, nullptr);

    auto find_field = [](json& fields, const string& name) -> json* {
        for (auto& f : fields)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // char a[100 - 10 - 3]; -- folded to a lit-int size-expr now that array
    // sizes go through the same expression chain as macro bodies.
    json* a = find_field((*sizes)["fields"], "a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ((*a)["var-type"]["size-expr"]["expr-type"], "lit-int");
    ASSERT_EQ((*a)["var-type"]["size-expr"]["value"], "87");

    // char b[4 * sizeof(int) - 2]; -- sizeof is never evaluated, so null must
    // still propagate through the surrounding fold (this is what keeps a
    // struct like FILE, whose glibc layout size expressions involve sizeof,
    // an incomplete type instead of resolving to a wrong size).
    json* b = find_field((*sizes)["fields"], "b");
    ASSERT_NE(b, nullptr);
    ASSERT_TRUE((*b)["var-type"]["size-expr"].is_null());
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

    // typedef void *P; -- pointer-bottomed typedefs are deliberately excluded
    // from this section this version.
    ASSERT_EQ(find_typedef("P"), nullptr);

    ASSERT_EQ(typedefs.size(), 6u);
}

// A header whose typedefs are never referenced by any C function -- stdint.h
// declares zero functions, so without the dedicated ast.typedefs section
// none of its typedefs would reach the AST at all (the only other export
// path is piggybacking on a function/global's var-type node).
TEST(c2ast, stdint_typedefs) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -s stdint.h");
    json ast = json::parse(output);
    auto& typedefs = ast["ast"]["typedefs"];

    auto find_typedef = [&](const string& name) -> json* {
        for (auto& t : typedefs)
            if (t["name"] == name) return &t;
        return nullptr;
    };

    auto expect_prim = [&](const string& name, const string& prim) {
        json* t = find_typedef(name);
        ASSERT_NE(t, nullptr) << "expected " << name << " to be registered";
        ASSERT_EQ((*t)["var-type"]["type-kind"], "prim");
        ASSERT_EQ((*t)["var-type"]["type-name"], prim) << "for " << name;
    };

    expect_prim("int32_t", "int32");
    expect_prim("int64_t", "int64");
    expect_prim("uint32_t", "uint32");
    expect_prim("uint64_t", "uint64");
}

// `(void)` normalizes to an empty parameter list, both at
// top level and inside a function-pointer's own parameter list, while a real
// parameter and an already-empty `()` are left untouched.
TEST(c2ast, void_param_list) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast ../test/testdata/c2ast/032_void_param_list.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    ASSERT_TRUE((*f)["parameters"].empty());

    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ((*g)["parameters"].size(), 1);
    ASSERT_EQ((*g)["parameters"][0]["name"], "a");

    json* h = find_func("h");
    ASSERT_NE(h, nullptr);
    ASSERT_TRUE((*h)["parameters"].empty());

    json* set_cb = find_func("set_cb");
    ASSERT_NE(set_cb, nullptr);
    auto& cb_vt = (*set_cb)["parameters"][0]["var-type"];
    ASSERT_EQ(cb_vt["type-kind"], "pntr");
    ASSERT_EQ(cb_vt["base-type"]["type-kind"], "func");
    ASSERT_TRUE(cb_vt["base-type"]["parameters"].empty());

    // No typedef in this header at all -- the "typedefs" key itself is
    // omitted, same convention as "structs".
    ASSERT_FALSE(ast["ast"].contains("typedefs"));
}

// --- Input file edge cases ---

TEST(c2ast, empty_header) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/033_empty_header.h");
    ASSERT_EQ(output, "");
}

TEST(c2ast, blank_lines) {
    cleanTestEnv();
    string output = execTestCommand("bin/palan-c2ast -d ../test/testdata/c2ast/034_blank_lines.h");
    ASSERT_EQ(output, "int f(int a);int g(int b);");
}
