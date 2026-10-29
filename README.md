# Palan

Palan is a programming language designed as a simpler, safer, and more enjoyable alternative to C.
It compiles to native x86-64 Linux binaries and calls C libraries directly through their headers.

```palan
cinclude <stdio.h>;

func divmod(int32 a, int32 b) -> int32 q, int32 r {
    a / b -> q;
    a % b -> r;
}

(int32 q, r) = divmod(17, 5);
printf("%d %d\n", q, r);    // 3 2
q * 2 -> q;                 // values flow left to right
printf("%d\n", q);          // 6
```

## Features

**Ownership without a garbage collector.** Arrays and structs are freed automatically at the end
of their scope. `->` copies, `->>` moves ownership, and a function borrows with `@` (read-only)
or `@!` (writable), which the caller writes too:

```palan
cinclude <stdio.h>;
const N = 4;

func sum(@[N]int32 a) -> int32 s {
    int32 i = 0;
    while i < N { s + a[i] -> s; i + 1 -> i; }
}
func fill(@![N]int32 a, int32 v) {
    int32 i = 0;
    while i < N { v -> a[i]; i + 1 -> i; }
}

[N]int32 a = [1, 2, 3, 4];  // allocated on the heap, freed at the end of its scope
[N]int32 b;
a -> b;                     // copies the elements
fill(@!b, 7);               // lends b writable (@ lends read-only)
printf("%d %d\n", sum(@a), sum(@b));   // 10 28

[N]int32 c;
a ->> c;                    // moves a's array into c; a is left NULL
printf("%d\n", c[3]);       // 4
```

**C libraries as they are.** `cinclude` reads a C header and makes its functions, types, structs,
enums and macro constants usable from Palan; `link` names the library to link. `$` places a
struct or array in the stack frame instead of the heap:

```palan
cinclude <stdio.h>;
cinclude <math.h> link "m";

type Vec { flo64 x; flo64 y; };

func len(@Vec v) -> flo64 { return sqrt(v.x * v.x + v.y * v.y); }

$Vec v = {x: 3.0, y: 4.0};
printf("%.1f\n", len(@v));  // 5.0
```

Other features include variable-length arrays (`[+]T` with `len`/`push`/`resize`/`pop`),
`for copy a, i in arr` loops, command-line arguments as `args`, function overloading, multiple named
return values, method-form calls (`c.add(1)` for `add(@!c, 1)`), function pointers, enums,
modules (`import`/`export`), and raw Linux syscalls.
See the [Palan Language Reference](doc/PalanReference.md) for the whole language.

## Quick Start

Build the toolchain (see [Building](#building)), then write `hello.pa`:

```palan
cinclude <stdio.h>;
printf("Hello World!\n");
```

```bash
build/bin/palan hello.pa
# Hello World!
```

Without `-o`, `palan` builds the binary among its work files and runs it right after linking;
arguments after the source file go to the program (`build/bin/palan hello.pa a b`). To write the
binary to the current directory instead:

```bash
build/bin/palan -o hello hello.pa
./hello
```

## Samples

- [samples/tetris.pa](samples/tetris.pa) — Tetris in the terminal with ncurses (needs
  `libncurses-dev`). Arrow keys move and drop, space rotates, `q` quits.

  ```bash
  build/bin/palan samples/tetris.pa
  ```

- [samples/raytracer/](samples/raytracer/) — a port of
  [tinyraytracer](https://github.com/ssloy/tinyraytracer) that writes `ray.jpg` with libjpeg
  (needs `libjpeg-dev`).

  ```bash
  cd samples/raytracer && ../../build/bin/palan raytracer.pa
  ```

## Building

**Dependencies:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

```bash
make            # build (build/, -O2 -g) and run all tests
make debug      # unoptimized build for gdb (build-debug/)
make clean      # remove build/ and build-debug/
make coverage   # coverage report (build-cov/lcov.info)
```

The tools are placed in `build/bin/`.

## Toolchain

Palan compiles through a pipeline of independent tools that pass JSON between them:

| Tool | Input | Output | Description |
|---|---|---|---|
| `palan-gen-ast` | `.pa` source | `.ast.json` | Parse Palan source into an AST |
| `palan-c2ast` | C header | AST JSON | Translate a C header (run by `palan-gen-ast` for each `cinclude`) |
| `palan-sa` | `.ast.json` | `.sa.json` | Semantic analysis |
| `palan-codegen` | `.sa.json` | `.s` assembly | x86-64 code generation |
| `palan` | `.pa` source | executable | Build manager (runs the tools above, `as` and `ld`) |

## Documentation

- [doc/PalanReference.md](doc/PalanReference.md) — language reference
- [doc/SpecAndDesign.md](doc/SpecAndDesign.md) — toolchain design
- [doc/ASTSpec.md](doc/ASTSpec.md), [doc/SASpec.md](doc/SASpec.md) — JSON formats between the tools
- [doc/Issues.md](doc/Issues.md) — known limitations and open issues

## License

MIT License — see [LICENSE](LICENSE). The bundled [nlohmann/json](lib/json) is distributed under its own MIT license (`lib/json/LICENSE.MIT`).
