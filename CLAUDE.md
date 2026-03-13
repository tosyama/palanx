# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Build all targets and run all tests
make

# Clean build artifacts
make clean

# Generate coverage report (HTML + lcov.info in build/)
make coverage
```

**Dependencies:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

### Running Individual Test Suites

```bash
cd build && bin/c2ast-tester       # C header → AST parser tests
cd build && bin/gen-ast-tester     # Palan parser tests
cd build && bin/sa-tester          # Semantic analyzer tests
cd build && bin/codegen-tester     # Code generator tests
cd build && bin/build-mgr-tester   # Build manager integration tests
```

### Running a Single Test

```bash
cd build && bin/sa-tester --gtest_filter=sa.call_c_function_annotated
```

### Running Individual Tools

```bash
cd build && bin/palan-c2ast -ds stdio.h                        # Parse system header to AST
cd build && bin/palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa
cd build && bin/palan ../test/testdata/build-mgr/001_helloworld.pa && ./a.out
```

## Architecture

Palan is a multi-stage compiler where independent CLI tools communicate via JSON AST files.

### Compilation Pipeline

```
source.pa
  ↓ palan-gen-ast   (PlnLexer.ll + PlnParser.yy → ast.json)
  ↓ palan-c2ast     (invoked internally for each cinclude; C header → AST nodes embedded in ast.json)
  ↓ palan-sa        (ast.json → sa.json; scope resolution, C function annotation, str-literal collection)
  ↓ palan-codegen   (sa.json → .s; VCodeGen → VProg IR → PlnX86CodeGen → AT&T assembly)
  ↓ as              (.s → .o)
  ↓ ld              (.o → a.out)
```

`palan` (build manager) orchestrates the full pipeline. Intermediate files are stored under `~/.palan/work/<absolute-source-path>/`.

### Key Design Points

**codegen IR layer** (`src/codegen/`): The code generator uses a virtual register + virtual instruction set (VProg) as an intermediate representation between the AST and x86-64 assembly. This allows future register allocation strategies to be plugged in via `PlnRegAlloc`.

- `PlnVCodeGen`: AST (Module) → VProg
- `PlnRegAlloc`: VProg → RegMap (VReg → physical register)
- `PlnX86CodeGen`: VProg + RegMap → AT&T assembly

**cinclude scoping**: C function declarations from `cinclude` are scope-aware in palan-sa. Functions are visible from the `cinclude` point to the end of the enclosing scope. `cinclude` nodes are consumed and not emitted to sa.json.

**str-literals**: String literals are collected by palan-sa (not codegen) into a `str-literals` table in sa.json. Codegen uses this table to emit `.rodata` entries.

### File Naming Convention

- Palan modules: `Pln<Name>.h/cpp` (e.g., `PlnSemanticAnalyzer`, `PlnX86CodeGen`)
- C2ast modules: `C<Name>.h/cpp` (e.g., `CLexer`, `CParser`, `CPreprocessor`)
- Entry points: `main.cpp` in each tool's directory

### Source Layout

```
src/
├── build-mgr/       palan — build manager (orchestrates pipeline)
├── c2ast/           palan-c2ast — C header parser
│   ├── CLexer.*         Hand-written C lexer
│   ├── CParser.*        Recursive descent C parser
│   ├── CPreprocessor.*  Macro expansion and #include handling
│   ├── CToken.*         Token definitions
│   └── predefined.h     Built-in macro definitions (copied to build/bin/c2ast/)
├── gen-ast/         palan-gen-ast — Palan parser
│   ├── PlnLexer.ll      Flex lexer rules
│   └── PlnParser.yy     Bison grammar rules
├── semantic-anlyzr/ palan-sa — semantic analyzer
│   └── PlnSemanticAnalyzer.*
├── codegen/         palan-codegen — x86-64 code generator
│   ├── PlnNode.h        AST node types (Module, Stmt, Expr variants)
│   ├── PlnVProg.h       VReg / VInstr / VProg IR definitions
│   ├── PlnVCodeGen.*    AST → VProg
│   ├── PlnRegAlloc.*    VProg → RegMap (register allocator plug-in point)
│   ├── PlnX86CodeGen.*  VProg → x86-64 AT&T assembly
│   ├── PlnDeserialize.* sa.json → Module
│   └── PlnCodeGen.h     Abstract base class
└── common/          Shared utilities (PlnFileUtils)
test/
├── testdata/        Test input files (numbered sequentially per tool)
└── test-base/       Shared test helpers (execTestCommand, cleanTestEnv)
doc/
├── SpecAndDesign.md Language spec and toolchain design
└── ASTSpec.md       JSON AST / sa.json format specification
localtickets/        Local task management (Markdown, not tracked by git)
```

## Testing

- Framework: Google Test (fetched via CMake FetchContent, v1.12.0)
- Generated files (`PlnLexer.cpp`, `PlnParser.cpp/h`) are excluded from coverage
- `execTestCommand` returns stdout + `:` + stderr; empty string means success
- Test data files are numbered sequentially (e.g., `001_helloworld.pa`)
