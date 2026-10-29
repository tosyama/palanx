# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-10-29)
version: 0.2.3 — improvements found through the samples (ray tracer, echo server)

- Top-level variable declarations are resolved when first needed, as top-level types and consts
  are, so a const computed from one (`const LN = lights->len();`) can size a borrowed array
  parameter (`@[LN]Light`).
- Global variables: a module's functions can use its top-level variables, which are kept in
  static storage, and the access functions have to them can be restricted. A module's top level
  runs only when it is the entry, so importing a module whose functions use its top-level
  variables is a compile error.
- Operator functions: a function named by an operator (`func -(@vec3 a, @vec3 b) -> vec3`)
  defines binary `+ - * / %` or unary `-` when at least one operand is a struct. It is chosen
  like any overloaded function; an operand is passed to a `@T` parameter as it is written, and a
  `@!T` parameter is not allowed.
- Performance: a function returning a struct returns it into storage the caller provides (frame
  storage for a temporary or a `$T` variable) instead of allocating it on the heap on every call
  (Issues.md #13), and `flo32`/`flo64` values are kept in XMM registers (Issues.md #14). Both are
  measured on the ray tracer before and after.
- Samples: the ray tracer is rewritten with the above, and a libuv echo server is added; gaps
  found while writing them are fixed as they are found.

Out of scope: exporting global variables and initializing an imported module's top-level
variables; comparison operators as operator functions.
The ticket list is kept in `localtickets/iteration-2026-10-29-v0230-samples.md`.


## 3. Command-line Tools' Responsibilities and Design

### 3.1 Build Manager (palan)

Responsibility:
  The tool is responsible for orchestrating the compilation process.

```
Usage: palan [options] <source file> [program arguments...]
 options:
   -o, --output <file path> Specify output file path. If not specified, the binary is
                            linked in the working directory and run with the program
                            arguments.
   -c, --clean              Remove all cached work files (the whole working directory)
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The build manager generates the AST of the given source file by invoking palan-gen-ast, then
 follows its `import`s of other Palan source files and generates their ASTs in turn.
 For each module it then invokes palan-sa and palan-codegen and assembles the result with `as`;
 a module compiles the same whichever module is the entry. The allocators the modules need are
 generated as one more module (see 3.6). The entry is chosen only at link time: `palan-codegen
 --entry` emits a `_start` calling the given module's top-level function, and all object files are
 linked with `ld` to create the final executable.
 Options are read only before the source file; every argument after it goes to the program.
 With `-o`, the executable is written to the given path and not run, and an argument after the
 source file is an error. Without it, the executable is linked as `<source file path>.out` in the
 working directory (see 5.1) and run with those arguments, and left there like the other work
 files, which `--clean` removes. Only `-o` writes into the current directory, so palan can be used
 as a script runner without leaving build artifacts, without touching an `a.out` already there,
 and with several sources in one directory run at the same time.
 Before linking, the build manager unions the `libs` and `objs` arrays (see SASpec.md's Root)
 across every module's sa.json, in the same pass it already makes over all modules to collect
 `alloc-shapes`. It appends `-l<name>` to the `ld` invocation for each library named by a `link`
 clause on a `cinclude` statement (e.g. `cinclude <math.h> link "m";`) anywhere in the program,
 and passes each `.o` file named by one after the program's own objects.

### 3.2 AST Generator (palan-gen-ast)
Responsibility:
  The tool generates the Abstract Syntax Tree (AST) from Palan source code.

```
Usage: palan-gen-ast [options] <source file>
 options:
   -o, --output <file path> Specify output AST file path. If not specified, standard output is used.
   -i, --indent             Generate indented (pretty-printed) JSON output
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The AST generator reads the Palan source file, parses it according to the language grammar,
 and constructs the corresponding AST representation.
 The generated AST is then serialized into a JSON format and saved to the specified output file path.
 When a `cinclude` statement is encountered, the AST generator invokes palan-c2ast internally via popen()
 to translate the C header into AST nodes, and embeds the resulting function declarations directly
 into the cinclude statement node of the ast.json output.
 It passes each run the `c-state` the previous run output (`--state`, through a temporary file),
 so every header is preprocessed and parsed with the macros, typedefs, struct/union tags and
 enums in effect at its `cinclude`, in source order, as if all of a file's cincluded headers were
 included into one C translation unit.
 To parse the Palan source code, the AST generator uses a parser generated by Flex and Bison.
 The parser is a GLR parser (Bison's `glr2.cc`), which passes `$N` to grammar actions as `const`
 references because split stacks may share a value, so actions never move from or modify `$N`.
 A growing list is therefore a persistent list (`PList`) that shares its prefix, and statements
 hold nested blocks by handle (`LazyNode`) and are converted to JSON once at the end; appending or
 nesting never copies what is already built.

### 3.3 C-to-AST Translator (palan-c2ast)
Responsibility:
  The tool translates C header files into AST nodes compatible with Palan.

```
Usage: palan-c2ast [options] <C header file>
 options:
   -o, --output <file path> Specify output AST file path. If not specified, standard output is used.
   -h, --help               Display help information
   -p, --path               Specify search paths for C headers
   -c, --curdir             Specify current directory for relative includes
   -s, --sysheader          Specify when input C header file is a system header
   -i, --indent             Generate indented (pretty-printed) JSON output
   -S, --state <file>       Start from a previous run's `c-state` instead of predefined.h
   -v, --version            Display version information
```

Design:
 The C-to-AST translator reads the specified C header file, parses it using a C parser,
 and constructs AST nodes that represent the declarations and definitions found in the header.
 The generated AST nodes are then serialized into a JSON format written to stdout (or to a file via -o).
 In the normal pipeline, palan-gen-ast invokes palan-c2ast and reads its stdout output directly;
 no intermediate file is produced.
 The predefined macros will be read from `./c2ast/predefined.h` in the execution directory of palan-c2ast.
 The output's top-level `c-state` is where the header leaves off, for the next run to start from:
 `macros`, the macro table written as `#define` lines (each body re-spelled token by token) plus a
 `#pragma palan_once_included "<path>"` line per file read under `#pragma once`; and `typedefs`,
 `structs`, `enums` and `enumerators`, the parser's tables, including entries the AST leaves out
 (an enum whose values can't be computed). `--state` loads it in place of predefined.h, so
 predefined macros an earlier header `#undef`ed stay undefined, and a header whose include guard
 skips it still has its types for `sizeof` and its enumerators for constant expressions.
 Nothing loaded this way is exported to the AST again unless the header changes it: a redefined
 macro, or a struct the earlier header only referenced and this one defines. The macros the
 header `#undef`s are listed in `ast.undefined-macros`.
 The included C header files will be expanded recursively, searching for headers in the specified search paths and current directory.
 `__GNUC__` is not predefined, so headers take their non-GNU paths, but `__attribute__` is a
 keyword: `packed` and `aligned` reach the struct layout (ASTSpec.md, Struct definition model).

### 3.4 Semantic Analyzer (palan-sa)
Responsibility:
  The tool performs semantic analysis on the generated AST and checks type correctness and scope rules.

```
Usage: palan-sa [options] <AST file>
 options:
   -o, --output <file path> Specify output analyzed AST file path. If not specified, defaults to <source>.sa.json.
   -i, --indent             Generate indented (pretty-printed) JSON output
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The semantic analyzer reads the ast.json file and builds a new sa.json independently.
 It processes statements in order, maintaining a scope stack for C function declarations
 registered via cinclude statements (scope-aware: functions are visible from the cinclude
 point until the end of the enclosing scope).
 cinclude and import statements are consumed for scope resolution and are not emitted to sa.json.
 A cinclude's `typedefs` (ASTSpec.md) are registered into the type-alias table unconditionally,
 independent of whether the header declares any functions, and a cinclude's `link` clause libraries
 are validated, deduplicated, and emitted as sa.json's top-level `libs` array (SASpec.md's Root) --
 the one piece of a cinclude statement that survives past this consume-and-drop step.
 Expression statements are annotated with resolution results (e.g., func-type: "c" for calls
 resolved to C functions) and emitted to sa.json.
 Type checking is performed during expression processing. When an implicit widening conversion is required
 (e.g., int32 value used in an int64 context), palan-sa inserts a `convert` expression node into sa.json.
 Explicit casts (`expr as type`, or `type-name(expr)` for a named type) allow narrowing, signed↔unsigned,
 and pointer conversions; palan-sa resolves
 the `cast` AST node to a `convert` node (or removes it for identical types) and emits `convert` to sa.json.
 Top-level types and consts are resolved when first needed, before function signatures are
 registered, so source order does not decide whether a signature can use them. A const whose value
 names a top-level variable is evaluated from that variable's type alone, taken from the shape of
 its declaration (the written type and an array literal's element count); its initializer is still
 analyzed in statement order. A const sees only the variables declared before it in the source.

### 3.5 Code Generator (palan-codegen)
Responsibility:
  The tool generates x86-64 assembly from the analyzed AST (sa.json).

```
Usage: palan-codegen [options] <SA file>
       palan-codegen -e <symbol> -o <file path>
 options:
   -o, --output <file path> Specify output assembly file path. If not specified, defaults to <source>.s.
   -e, --entry <symbol>     Emit the program entry instead: a _start calling <symbol>, a module's
                            top-level function, and __dso_handle
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The code generator reads sa.json and emits x86-64 AT&T syntax assembly for use with `as`.
 A module's top-level statements are its exported function `<module>..main`, which takes argc and
 argv. The entry point `_start` (from `--entry`) calls it with argc and argv from the initial
 stack, then `exit(0)`. The final executable is produced by the build manager,
 which assembles the `.s` file with `as` and links with `ld` (specifying the dynamic linker
 and required libraries).

 Calling convention:
 - C and Palan function calls share the x86-64 System V argument assignment: integer/pointer
   arguments take rdi, rsi, rdx, rcx, r8, r9 and float arguments take xmm0-xmm7, each counted
   by its own class index; arguments beyond either sequence go to the stack in order.
 - Before a C call, al is set to the number of XMM argument registers used (required by
   variadic functions such as printf).
 - A single return value is returned in rax, or xmm0 for a float (System V compatible).
 - Two or more return values take each class's argument register sequence in order
   (integers in rdi/rsi/rdx/..., floats in xmm0/xmm1/...; caller-saved, read immediately after call).
 - Normal Palan functions use standard frame setup (pushq %rbp / movq %rsp, %rbp / subq $N, %rsp)
   with frameSize rounded to a multiple of 16, and epilogue `leave; ret`.
 - A module's top-level function returns like any Palan function; `return` statements are
   rejected there by palan-sa.
 - Symbol visibility: `_start`, each module's top-level function and `export func` functions are
   emitted with `.globl`. All other Palan
   functions are local symbols (no `.globl`), equivalent to `static` in C.
 - Symbol names: a Palan function's symbol is the `name` palan-sa gives it, built from its module,
   its name and its parameter types (`util..show.int32`; SASpec.md "Symbol names"), so overloads
   and same-named functions of different modules link side by side. codegen emits it as given.

 String literals are collected by palan-sa into the `str-literals` table in sa.json,
 placed in the `.rodata` section with generated labels (`.str0`, `.str1`, ...),
 and referenced via `leaq label(%rip), %rdi` (RIP-relative addressing).

 Palan links no crt startup objects (no crt1.o/crti.o/crtbegin.o -- the build manager
 links `ld <objs> -lc` directly), so the code generator supplies the ELF/libc glue those
 objects would otherwise provide: the `--entry` object defines `__dso_handle` (needed by
 glibc's `atexit`/`at_quick_exit`/`pthread_atfork`, which forward to `__cxa_atexit`-family
 calls taking it), and every object emits `.note.GNU-stack` (otherwise `ld` marks the
 whole binary's stack executable as soon as any note-carrying libc object joins the link).


### 3.6 Allocator Generation

An owned aggregate made of more than one heap block — a struct that owns fields, an array of
structs, a `[m][n]T` array — is allocated, released and copied by generated functions instead of
inline loops. A struct that owns nothing and a `[n]T` array of primitives use `malloc` and `free` directly.

palan-sa emits only calls to these functions; it records each shape it needs in sa.json's
`alloc-shapes` (SASpec.md's Root), as it collects `str-literals`. The build manager collects the
shapes of all modules, deduplicates them by key (each module defining its own copy would collide
at link time), and writes the functions as Palan source `__allocators.pa` in the working directory
of the given source file, with `__allocators.h` declaring the C functions it calls. It is compiled
as one more module. Each struct appears there as an exported type declaring only
its owned fields, at their offsets, with the rest of its bytes as padding, so a C struct or a type
of another module needs no Palan declaration of its other fields. A struct owning nothing is
only bytes to allocate, copy and free, so an element or owned field of one is declared as
`__pln_B<n>`, a type of its `n` bytes (`$[n]uint8`), whatever struct it is.

For a shape key `K`, the functions are:

| Function | Does |
|----------|------|
| `__pln_alloc_K(sizes...)` | allocates and builds the value |
| `__pln_free_K(p, sizes...)` | releases what `p` owns, then `p`; `p` may be NULL |
| `__pln_init_K(p, sizes...)` | builds the value in an existing block (a `$` variable) |
| `__pln_fini_K(p, sizes...)` | releases what `p` owns, but not `p` (a `$` variable) |
| `__pln_copy_K(dst, src, sizes...)` | copies `src` into `dst`, owned parts included |

| Shape | `K` | Functions |
|-------|-----|-----------|
| struct `T` owning fields | `T` | all |
| struct owning nothing, as an element or a field (`bytes`) | `__pln_B<n>` | none; only declared |
| `[n]T` whose element owns storage (a struct, a `[+]U`, a row) | `arr_E` | all |
| `[+]T` (`T` a primitive, a struct, a `[+]U` or a row `[n]U`) | `varr_E` | all, and `push`/`resize`/`pop`; `copy(h, src, n)` copies a run of elements |
| `[+]$T` with `T` a struct or rows `[n]U`, `[n]$[m]U`, … | `evarr_E` | as `varr_E` |

`E` is the layout of the element as the array's storage holds it, written as a parameter type
is in a symbol (SASpec.md's Symbol names). The layout replaces each integer type by the signed one
of its width and each struct owning nothing by its `__pln_B<n>`: with `Point` 16 bytes owning
nothing, `[n]Point` is `arr_P9__pln_B16` (an owned slot), `[+]$Point` is `evarr_9__pln_B16`,
`[+]int32` and `[+]uint32` are both `varr_int32`, `[+][4]int32` is `varr_Pint32`. A key thus
names how its elements are stored, moved and passed, and elements laid out alike share their
functions: a narrow integer passes as its width and whoever reads it extends it as its own type
says, so only the types the built-ins take and return differ, not the functions they call; a
float, passed in another register, has a key of its own. The build manager generates the arrays
of any element from one description of the element (`elem` in the shape, `ElemOps` in the build
manager): an owned element is built when its slot comes into use and dropped when it leaves, by
helpers `__pln_build/drop/set/copyrun_E` shared by `arr_E` and `varr_E`; any other element is
plain bytes, copied in bulk. A row element is described by its own element: a row in a
block of its own is an owned element built and dropped through its elements' helpers, and a row
laid out in place (`[+]$[n]U`) is `n` slots of its element, so it is owned exactly when that
element is; rows laid out in place in turn are one run of slots (with `S` owning fields,
`[2]$[3]S` is `arr_P1S` of 6).
A struct's owned field is a slot of the same kind, described the same way: `__pln_init/fini/copy_T`
build, drop and copy each field through its element's helpers, and the generated module declares
the field as the slot its build fills (`@!int32` for `[3]int32`), so it copies and frees nothing
on its own. Palan sets only what it reads to release a value: owned slots and `[+]U` headers
(an inner `[+]U` starts as an empty header, `calloc(1, 24)`). Everything else starts undefined,
as with C's `malloc`: a struct's other fields, pointers included, an array's elements, the
elements `resize` adds and those past a `[+n]T` literal's.
A length may be known only at run time (`[m][k]int32`), so keys leave every length out and every
`arr_E`/`varr_E` function and helper takes, after its own arguments, one length per array level
below it, outermost first, through rows and `[+]U` elements alike: `[m][3]int32` and `[+][3]int32`
pass 3, `[+][4]int32` passes 4, and all share `__pln_build_Pint32` and the rest. palan-sa appends
a type's lengths to its `push`/`resize`/`pop` calls itself.

Like any Palan function, each links by a symbol made from its module and parameter types, so
palan-sa names the module `__allocators` (`PALAN_ALLOCATORS_MODULE` in `src/common/PlnDefs.h`,
which the build manager also uses for the file name) and the parameter types of each signature
above when it calls one (`allocHelperCall`).

## 4. Expression Value Categories

palan-sa marks an expression whose value has, or needs, an owner with a `category`:

| Category | Expression |
|----------|------------|
| `owned`    | an `id` naming a variable that owns its storage and releases it at scope exit (a heap struct or array, not a `$` variable) |
| `expiring` | a Palan function call returning a pointer type; a returned struct has no owner yet |
| (absent)   | anything else |

- A returned struct initializing a variable becomes owned by it without a copy; a `$T` variable
  gets its contents and the returned block is freed. Assigned with `->`, it is copied and freed.
- A returned struct given as an argument, or not used, is freed once the statement has been
  evaluated: the callee only borrows it. Binding it to a `@T`/`@!T` borrow is an error.
- `return v` of an owned `v` drops it from scope-exit release; the caller owns the value.
  Returning a struct the function doesn't own returns a copy.
- `v ->> target` needs an owned `v` and sets `v` to NULL after the transfer, so its scope-exit
  free is a no-op; an `expiring` source has nothing to reset.
- `v -> target` with an owned array or struct `target` and a non-expiring `v` copies the contents
  (owned parts included); neither side's ownership changes.

## 5. Working Directory and Output Files
### 5.1 Working Directory
The working directory for all command-line tools is `~/.palan/work/` by default.
And the original source file absolute path is mirrored under the working directory.
For example, if the source file is located at `/home/user/project/main.pa`,
related output files will be stored under `~/.palan/work/home/user/project/`.

### 5.2 Output Files
- palan-gen-ast:
  - Output AST file: `<source file path>.ast.json`
    e.g., for source file `main.pa`, the output AST file will be `main.pa.ast.json`
  - C header declarations from `cinclude` are embedded within the ast.json; no separate file is produced by palan-c2ast in the normal pipeline.
- palan-sa:
  - Output analyzed AST file: `<source file path>.sa.json`
    e.g., for source file `main.pa`, the output analyzed AST file will be `main.pa.sa.json`
- palan-codegen:
  - Output assembly file: `<source file path>.s`
    e.g., for source file `main.pa`, the output assembly file will be `main.pa.s`
- palan (build manager):
  - Assembles `.s` files with `as` and links with `ld` to produce the final executable: the `-o`
    path, or without `-o` `<source file path>.out` in the working directory (e.g. `main.pa.out`).

### 5.3 Error Output Format

All tools write error messages to **stderr** and exit with code **1** on error.

| Tool | Error format |
|------|-------------|
| palan-gen-ast | `<source_file>:<line>:<col>: error: <message>` (parse errors); `<message>` (CLI errors) |
| palan-sa | `<source_file>:<line>:<col>: error: <message>` |
| palan-codegen | `<message>` |
| palan (build-mgr) | `<message>` |
| palan-c2ast | `<source_file>:<line>:<col>: error: <message>` (preprocessor errors); `<message>` (CLI errors) |

