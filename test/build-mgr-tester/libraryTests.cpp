/// palan build manager tests: using a library through its interface and archive
///
/// @file libraryTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "../test-base/testBase.h"
#include "../../src/common/PlnDefs.h"
#include "buildMgrTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

static const char VEC_SOURCE[] = R"(library geo;
cinclude <math.h> link "m";
export type Vec { flo64 x; flo64 y; };
export type Poly { [2]Vec pts; [+]int32 tags; int32 n; };
export func vec(flo64 x, flo64 y) -> Vec v { x -> v.x; y -> v.y; }
export func +(@Vec a, @Vec b) -> Vec v { a.x + b.x -> v.x; a.y + b.y -> v.y; }
export func norm(@Vec a) -> flo64 { return sqrt(a.x * a.x + a.y * a.y); }
export func makePoly(int32 n) -> Poly p { n -> p.n; p.tags.resize(3); }
export func tagCount(@Poly p) -> int64 { return len(@p.tags); }
)";

static const char SHAPE_SOURCE[] = R"(library draw;
libpath "../libs";
import <geo/vec>;
export type Shape { Vec c; Poly p; };
export func size(@Shape s) -> flo64 { return norm(@s.c); }
)";

static string outAbsolute(const string& path) {
	return fs::weakly_canonical("out/" + path).string();
}

static void writeFile(const string& path, const string& text) {
	fs::create_directories(fs::path(path).parent_path());
	ofstream(path) << text;
}

static string fileText(const string& path) {
	ifstream f(path);
	stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

// The sources are removed, so what the program builds from is only what a library ships.
static void buildGeoLibrary() {
	cleanTestEnv();
	writeFile("out/src/vec.pa", VEC_SOURCE);
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/vec.pa"), "");
	fs::remove_all("out/src");
}

static void buildDrawLibrary() {
	writeFile("out/src/shape.pa", SHAPE_SOURCE);
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/shape.pa"), "");
	fs::remove_all("out/src");
}

TEST(build_mgr, library_used_without_its_source) {
	buildGeoLibrary();
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <geo/vec>;
cinclude <stdio.h>;
cinclude <mcheck.h>;
{
	mtrace();
	Vec a = vec(3.0, 4.0);
	Vec c = a + vec(1.0, 1.0);
	printf("%.1f %.1f %.1f\n", c.x, c.y, norm(@a));
	Poly p = makePoly(5);
	printf("%d %ld\n", p.n, tagCount(@p));
	[2]Poly ps;
	printf("%ld\n", tagCount(@ps[1]));
}
)");
	ASSERT_EQ(execTestCommand("bin/palan -o out/p out/app/main.pa"), "");
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so MALLOC_TRACE=out/t.log out/p");
	ASSERT_EQ(output, "4.0 5.0 5.0\n5 3\n0\n");
	// The same program importing the library's source allocates just as many.
	expectBalancedMtrace("out/t.log", 19);
}

TEST(build_mgr, library_import_forms) {
	buildGeoLibrary();
	writeFile("out/app/main.pa", R"(libpath "../libs";
import vec, norm, Vec from <geo/vec>;
import <geo/vec> as G;
cinclude <stdio.h>;
Vec a = vec(3.0, 4.0);
G.Vec b = G.vec(6.0, 8.0);
printf("%.1f %.1f\n", norm(@a), G.norm(@b));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0 10.0\n");
}

// The program reads the defaults from the interface alone, through an alias too.
TEST(build_mgr, library_parameter_defaults) {
	cleanTestEnv();
	writeFile("out/src/opt.pa", R"(library kit;
cinclude <stdio.h>;
export type Mode enum { Fast, Slow };
const LIMIT = 40;
export func run(int32 a, int32 lim = LIMIT + 2, Mode m = Mode.Slow, @[]uint8 tag = "dflt") {
	printf("%d %d %d %s\n", a, lim, m, tag);
}
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/opt.pa"), "");
	fs::remove_all("out/src");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/opt> as K;
K.run(1);
K.run(2, 7, K.Mode.Fast);
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "1 42 1 dflt\n2 7 0 dflt\n");
}

// A variadic parameter crosses the interface as written, also inside a
// function pointer type, and is called through an alias.
TEST(build_mgr, library_variadic) {
	cleanTestEnv();
	writeFile("out/src/va.pa", R"(library kit;
cinclude <stdio.h>;
export type P { int32 x; };
export func join(@[]uint8 sep, [...]@[]uint8 parts) {
	for copy s, i in parts { if i > 0 { printf("%s", sep); } printf("%s", s); }
	printf("\n");
}
export func sum([...]P ps) -> int32 t { for @p in ps { t + p.x -> t; } }
export func pick(@func(int32, [...]int32) -> int32 f) -> int32 { return f(1, 2, 3); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/va.pa"), "");
	fs::remove_all("out/src");
	string pai = fileText("out/libs/kit/va.pai");
	EXPECT_NE(pai.find("export func join(@[]uint8 sep, [...]@[]uint8 parts);\n"), string::npos) << pai;
	EXPECT_NE(pai.find("export func pick(@func(int32, [...]int32) -> int32 f) -> int32;\n"), string::npos) << pai;
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <kit/va> as K;
cinclude <stdio.h>;
func add(int32 a, [...]int32 xs) -> int32 t { a -> t; for copy x in xs { t + x -> t; } }
K.join(", ", "a", "b", "c");
K.P p = {x: 2};
printf("%d %d\n", K.sum(p, [3]), K.pick(add));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "a, b, c\n5 6\n");
}

// A libpath adds a place for the whole program, wherever it is written and relative to that file.
TEST(build_mgr, libpath_in_an_imported_module) {
	buildGeoLibrary();
	writeFile("out/app/sub/helper.pa", "libpath \"../../libs\";\n");
	writeFile("out/app/main.pa", R"(import "sub/helper.pa";
import <geo/vec>;
cinclude <stdio.h>;
Vec a = vec(3.0, 4.0);
printf("%.1f\n", norm(@a));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0\n");
}

// The object a `cinclude` links goes into the archive, since its source does not ship.
TEST(build_mgr, library_carries_a_linked_object) {
	cleanTestEnv();
	string c = "../test/testdata/build-mgr/295_link_obj";
	ASSERT_EQ(execTestCommand("mkdir -p out/src && cp " + c + ".h out/src/ && gcc -c -O1 " + c + ".c -o out/src/295_link_obj.o"), "");
	writeFile("out/src/sum.pa", R"(library calc;
cinclude "295_link_obj.h" link "295_link_obj.o";
export func sum3() -> int32 { return add3(1, 2, 3); }
)");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/sum.pa"), "");
	fs::remove_all("out/src");
	EXPECT_EQ(execTestCommand("ar t out/libs/calc/sum.a"), "calc.sum.o\n__obj0_295_link_obj.o\n");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <calc/sum>;
cinclude <stdio.h>;
printf("%d\n", sum3());
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "6\n");
}

TEST(build_mgr, library_using_a_library) {
	buildGeoLibrary();
	buildDrawLibrary();
	string pai = fileText("out/libs/draw/shape.pai");
	EXPECT_NE(pai.find("import <geo/vec>;\n"), string::npos) << pai;
	EXPECT_EQ(pai.find("libpath"), string::npos) << pai;

	writeFile("out/app/main.pa", R"(libpath "../libs";
import <draw/shape>;
cinclude <stdio.h>;
Shape s;
3.0 -> s.c.x;
4.0 -> s.c.y;
printf("%.1f\n", size(@s));
)");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "5.0\n");
}

TEST(build_mgr, library_changes_reach_its_users) {
	cleanTestEnv();
	writeFile("out/src/val.pa", "library lib;\nexport func val(int32 a) -> int32 { return a; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	writeFile("out/app/main.pa", R"(libpath "../libs";
import <lib/val>;
cinclude <stdio.h>;
printf("%d\n", val(1));
)");
	ASSERT_EQ(execTestCommand("bin/palan out/app/main.pa"), "1\n");

	writeFile("out/src/val.pa", "library lib;\nexport func val(int32 a) -> int32 { return a + 1; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "2\n");

	// A parameter's type is in the symbol, so an importer not compiled again fails to link.
	writeFile("out/src/val.pa", "library lib;\nexport func val(int64 a) -> int64 { return a + 2; }\n");
	ASSERT_EQ(execTestCommand("bin/palan --lib -o out/libs out/src/val.pa"), "");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "3\n");
}

TEST(build_mgr, library_lookup_errors) {
	buildGeoLibrary();
	string main_pa = outAbsolute("app/main.pa");

	writeFile(main_pa, "import <geo/none>;\nlibpath \"../libs\";\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: '" + main_pa + "' imports <geo/none>, which no libpath has\n");

	writeFile(main_pa, "import <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: '" + main_pa + "' imports <geo/vec>, which no libpath has\n");

	// Each libpath alone is enough for the import, and order must not decide between them.
	fs::create_directories("out/other/geo");
	writeFile(main_pa, "libpath \"../libs\";\nlibpath \"../other\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa),
		"return1::palan: library 'geo' is found in both '" + outAbsolute("libs") + "' and '" + outAbsolute("other") + "'\n");

	// One directory written twice is not two places.
	writeFile(main_pa, "libpath \"../libs\";\nlibpath \"../libs/\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan " + main_pa), "");
}

TEST(build_mgr, library_dependency_needs_the_programs_libpath) {
	buildGeoLibrary();
	buildDrawLibrary();
	fs::create_directories("out/libs2");
	fs::rename("out/libs/draw", "out/libs2/draw");
	writeFile("out/app/main.pa", "libpath \"../libs2\";\nimport <draw/shape>;\n");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs2/draw/shape.pai") + "' imports <geo/vec>, which no libpath has\n");
}

TEST(build_mgr, library_interface_errors) {
	buildGeoLibrary();
	string pai = "out/libs/geo/vec.pai";
	string text = fileText(pai);
	writeFile("out/app/main.pa", "libpath \"../libs\";\nimport <geo/vec>;\n");

	auto replaced = [&](const string& from, const string& to) {
		string edited = text;
		edited.replace(edited.find(from), from.size(), to);
		return edited;
	};
	writeFile(pai, replaced("built \"" PALAN_VERSION "\"", "built \"0.0.1\""));
	EXPECT_NE(execTestCommand("bin/palan out/app/main.pa").find("built by version 0.0.1"), string::npos);
	writeFile(pai, replaced("target \"" PALAN_TARGET "\"", "target \"aarch64-linux-gnu\""));
	EXPECT_NE(execTestCommand("bin/palan out/app/main.pa").find("is for the target aarch64-linux-gnu"), string::npos);
	writeFile(pai, text);
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"), "");

	fs::remove("out/libs/geo/vec.a");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs/geo/vec.pai") + "' has no archive beside it\n");
}

TEST(build_mgr, library_identity_met_by_a_source_module) {
	buildGeoLibrary();
	string source = outAbsolute("app/vec.pa");
	writeFile(source, "library geo;\n");
	writeFile("out/app/main.pa", "libpath \"../libs\";\nimport \"vec.pa\";\nimport <geo/vec>;\n");
	EXPECT_EQ(execTestCommand("bin/palan out/app/main.pa"),
		"return1::palan: '" + outAbsolute("libs/geo/vec.pai") + "' has the same module identity 'geo.vec' as '" + source + "'\n");
}
