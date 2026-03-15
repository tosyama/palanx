# Palan

Palan is a programming language designed as a simpler, safer, and more enjoyable alternative to C.
It compiles to x86-64 assembly and supports calling C library functions via `cinclude`.

## Quick Start

Write a Palan source file:

```
# hello.pa
cinclude <stdio.h>;
printf("Hello World!\n");
```

Build and run:

```bash
bin/palan hello.pa
# Hello World!
```

When `-o` is not specified, `palan` executes the binary immediately after linking and removes it.
To keep the binary, use `-o`:

```bash
bin/palan -o hello hello.pa
./hello
# Hello World!
```

## Build Instructions

**Dependencies:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

```bash
# Build and run all tests
make

# Clean build artifacts
make clean

# Generate coverage report
make coverage
```

Binaries are placed in `build/bin/`.

## Toolchain

Palan compilation is performed by a pipeline of independent CLI tools:

| Tool | Input | Output | Description |
|---|---|---|---|
| `palan-gen-ast` | `.pa` source | `.ast.json` | Parse Palan source into AST |
| `palan-c2ast` | C header | AST JSON | Translate C headers into AST nodes |
| `palan-sa` | `.ast.json` | `.sa.json` | Semantic analysis |
| `palan-codegen` | `.sa.json` | `.s` assembly | x86-64 code generation |
| `palan` | `.pa` source | `a.out` | Build manager (orchestrates above) |

See [doc/SpecAndDesign.md](doc/SpecAndDesign.md) for full specification and design details.
