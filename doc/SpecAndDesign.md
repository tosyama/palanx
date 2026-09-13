# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-09-12)
version: 0.1.30 — stdlib.h: anonymous-struct typedefs, by-value struct returns, pointer
address-of, and C callbacks

An audit of stdlib.h found 94 of its 104 functions already usable with no compiler change at
all. The ten that are not split into two groups — five needing C function-pointer parameters
(`qsort`, `bsearch`, `atexit`, `at_quick_exit`, `on_exit`) and five needing typedefs of
anonymous struct bodies (`div`/`ldiv`/`lldiv` return `div_t`/`ldiv_t`/`lldiv_t`;
`select`/`pselect` take `fd_set`/`__sigset_t`) — and a third, orthogonal gap blocks the
common C out-parameter idiom, since `@`/`@!` accepts only a primitive-typed operand:

```
cinclude <stdlib.h>;
div_t d = div(7, 2);                → error: unrepresentable type: div_t
@!int8 endp;
strtol("42abc", @!endp, 10);        → error: cannot take the address of 'endp'
qsort(arr, n, 4, cmp);              → error: C function 'qsort' has an unsupported signature
```

Three independent gaps cause this. (1) `typedef struct { ... } div_t;` never reaches c2ast's
`structs` channel: `captureStructTag` is called only for a *named* tag, so an anonymous body
attached to a typedef registers nowhere and every later reference falls through to an
unresolved `user` type-kind. (2) `@`/`@!` rejects a pointer-typed operand, which is exactly
the shape `char **endptr` and `void **memptr` need. (3) A C function-pointer parameter is
classified unrepresentable at the cinclude ingestion boundary, so the whole function is
rejected even though the only value that could ever fill that slot — the address of a Palan
function — is already just an assembler label.

This iteration closes all three, plus the address-of gap for an embedded struct field.
c2ast synthesizes the typedef's own name as the struct's tag, so an anonymous-body typedef
registers through the same single path a named tag already uses and SA sees one canonical
shape. `@`/`@!` extends to a pointer-to-primitive local variable, producing the
pointer-to-pointer a C out-parameter expects, and to an embedded struct field. A Palan
function named in a C callback argument position becomes a function reference, lowered to
the existing `LeaLabel` instruction — no indirect-call machinery is needed, because Palan
never calls through the pointer; the C library does, in its own compiled code. And receiving
a struct returned **by value** from a C function is implemented as a general System V AMD64
return classification (one or two eightbytes in `%rax`/`%rdx` or `%xmm0`/`%xmm1`, or a hidden
destination pointer for anything over 16 bytes), not as a `div_t`-shaped special case.

Planning surfaced a prerequisite bug that is worse than any of the above: a struct-typed
local variable's initializer is discarded outright — `Pair p = make_pair(3, 4);` compiles to
code that never calls `make_pair`, with no diagnostic — and a by-value struct value passes
unchecked through every argument, assignment and return binding site, because those sites
treat a non-primitive type mismatch as a deliberate pass-through. By-value struct
*parameters* are admitted on the same reasoning, so `fopencookie` type-checks today and would
be called with a wrong ABI. All of these are made loud first, in their own ticket, before any
new capability is added on top of them.

Non-goals for this iteration: unions as a type (c2ast's capture paths are all guarded on
`is_struct`, and C's overlapping storage is not Palan's `struct`); practical `select`/
`pselect` support (`fd_set`'s only field is an array sized by `sizeof`, which c2ast still
cannot fold, so the type stays incomplete even once the typedef resolves); passing a struct
to a C function by value (the inverse of the return path — diagnosed, not implemented); a
struct-by-value return type for native Palan functions; first-class function-pointer
variables or indirect calls from Palan; and `@`/`@!` on array elements — a pointer-typed
array element is indistinguishable in SA from the out-of-scope `[n]@T` pointer-slot-array
element, and no C API in the audit needs either.

Full gap catalog, the SysV classification rules and sa.json shapes, the address-of scope
decision and its evidence, and the ticket breakdown are in
`localtickets/iteration-2026-09-12-v0130-stdlib.md`.

The series' underlying goal stays the same: header/feature support is the forcing function
for general C-interop language capability, not per-function coverage for its own sake.


## 3. Command-line Tools' Responsibilities and Design

### 3.1 Build Manager (palan)

Responsibility:
  The tool is responsible for orchestrating the compilation process.

```
Usage: palan [options] <source files>
 options:
   -o, --output <file path> Specify output file path. If not specified, the binary is
                            executed immediately after linking and then removed.
   -c, --clean              Clean build artifacts
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The build manager searches for specified Palan source files and generates the output file paths for the AST generator.
 After generating ASTs of the source files by invoking palan-gen-ast, it checks for dependencies on other Palan source files and invokes palan-gen-ast for those dependent files if needed.
 After generating ASTs for all source files, it invokes palan-sa to perform semantic analysis on the generated ASTs.
 It then invokes palan-codegen to generate x86-64 assembly files from the analyzed ASTs,
 assembles them with `as`, and links the resulting object files with `ld` to create the final executable (default: `a.out`).
 If `-o` is not specified, the resulting `a.out` is executed immediately after linking and then removed.
 This allows palan to be used as a script runner without leaving build artifacts.
 Before linking, the build manager reads ast.json for each source file and collects `link` declarations
 from `cinclude` statements (e.g. `cinclude <stdio.h> link "c";`), passing the corresponding `-l` flags to `ld`.
 This link declaration feature is designed but not yet implemented; linking flags are handled manually in the interim.
 During each step, the build manager will check the creation times of source files and their corresponding output files
 to determine if recompilation is necessary, optimizing the build process by avoiding redundant work.

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
 To parse the Palan source code, the AST generator uses a parser generated by Flex and Bison.

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
   -v, --version            Display version information
```

Design:
 The C-to-AST translator reads the specified C header file, parses it using a C parser,
 and constructs AST nodes that represent the declarations and definitions found in the header.
 The generated AST nodes are then serialized into a JSON format written to stdout (or to a file via -o).
 In the normal pipeline, palan-gen-ast invokes palan-c2ast and reads its stdout output directly;
 no intermediate file is produced.
 The predefined macros will be read from `./c2ast/predefined.h` in the execution directory of palan-c2ast.
 The included C header files will be expanded recursively, searching for headers in the specified search paths and current directory.

### 3.4 Semantic Analyzer (palan-sa)
Responsibility:
  The tool performs semantic analysis on the generated AST and checks type correctness and scope rules.

```
Usage: palan-sa [options] <AST file>
 options:
   -o, --output <file path> Specify output analyzed AST file path. If not specified, defaults to <source>.sa.json.
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The semantic analyzer reads the ast.json file and builds a new sa.json independently.
 It processes statements in order, maintaining a scope stack for C function declarations
 registered via cinclude statements (scope-aware: functions are visible from the cinclude
 point until the end of the enclosing scope).
 cinclude and import statements are consumed for scope resolution and are not emitted to sa.json.
 Expression statements are annotated with resolution results (e.g., func-type: "c" for calls
 resolved to C functions) and emitted to sa.json.
 Type checking is performed during expression processing. When an implicit widening conversion is required
 (e.g., int32 value used in an int64 context), palan-sa inserts a `convert` expression node into sa.json.
 Explicit casts (`type-name(expr)`) allow narrowing and signed↔unsigned conversions; palan-sa resolves
 the `cast` AST node to a `convert` node (or removes it for identical types) and emits `convert` to sa.json.

### 3.5 Code Generator (palan-codegen)
Responsibility:
  The tool generates x86-64 assembly from the analyzed AST (sa.json).

```
Usage: palan-codegen [options] <SA file>
 options:
   -o, --output <file path> Specify output assembly file path. If not specified, defaults to <source>.s.
   --no-entry               Suppress _start generation (use for library files)
   -h, --help               Display help information
   -v, --version            Display version information
```

Design:
 The code generator reads sa.json and emits x86-64 AT&T syntax assembly for use with `as`.
 The entry point is `_start`. The final executable is produced by the build manager,
 which assembles the `.s` file with `as` and links with `ld` (specifying the dynamic linker
 and required libraries).

 Calling convention:
 - C function calls follow the x86-64 System V ABI: arguments passed in rdi, rsi, rdx, rcx, r8, r9.
 - For variadic C functions (e.g. printf), al is set to 0 (no floating-point arguments).
 - Palan function calls use the same argument registers (rdi/rsi/rdx/rcx/r8/r9).
 - A single return value is returned in rax (System V compatible).
 - Two or more return values are returned in rdi/rsi/rdx/... (caller-saved; read immediately after call).
 - Normal Palan functions use standard frame setup (pushq %rbp / movq %rsp, %rbp / subq $N, %rsp)
   with frameSize rounded to a multiple of 16, and epilogue `leave; ret`.
 - The `_start` entry point uses `call exit` as its epilogue; `return` statements are rejected by palan-sa.
 - Symbol visibility: `_start` and `export func` functions are emitted with `.globl`. All other Palan
   functions are local symbols (no `.globl`), equivalent to `static` in C.

 String literals are collected by palan-sa into the `str-literals` table in sa.json,
 placed in the `.rodata` section with generated labels (`.str0`, `.str1`, ...),
 and referenced via `leaq label(%rip), %rdi` (RIP-relative addressing).


### 3.6 Array Allocator Generation (Future Design)

For complex array element types (nested arrays, structs with array fields), a simple `malloc` call
is insufficient — recursive allocation and deallocation loops are required.

**Design decision:** palan-sa auto-generates allocator and free functions for complex array shapes,
and the build manager aggregates them across modules.

#### Allocator function model

Allocator functions are keyed by the **structural shape** of the element type, with all sizes
stripped out and passed as arguments. This avoids per-size function proliferation.

##### Shape key

The shape key encodes the full structural type path from outermost to innermost using `_` as
separator. Constructor tokens (`arr`, `ptr`, `emb`) appear first; the leaf type name is always last.

**Encoding rules:**
1. Each type constructor contributes one token: `arr`, `ptr`, or `emb`
2. The leaf type name (primitive or user-defined) is appended last
3. Underscores within user-defined type names are escaped to `__`
   (constructor tokens never contain underscores, so no escaping is needed for them)

**Decoding rule:** scan left to right splitting on `_`; a `__` sequence is an escaped `_`
within the current token, not a separator. The last token is always the leaf type name.

| Palan type           | Shape notation         | Shape key              |
|----------------------|------------------------|------------------------|
| `[n]int32`           | `arr(int32)`           | — (no allocator; direct malloc) |
| `[m][n]int64`        | `arr(arr(int64))`      | `arr_arr_int64`        |
| `[l][m][n]int64`     | `arr(arr(arr(int64)))` | `arr_arr_arr_int64`    |
| `[n]MyStruct`        | `arr(MyStruct)`        | `arr_MyStruct`         |
| `[m][n]MyStruct`     | `arr(arr(MyStruct))`   | `arr_arr_MyStruct`     |
| `[n]arr`             | `arr(arr)`             | `arr_arr`              |
| `[n]my_type`         | `arr(my_type)`         | `arr_my__type`         |
| `[m]$my_type`        | `arr(emb(my_type))`    | `arr_emb_my__type`     |

##### Function naming convention

- Alloc: `__pln_alloc_<shape-key>`
- Free:  `__pln_free_<shape-key>`

The `__pln_` prefix reserves the symbol as a Palan internal (double-underscore per C standard,
`pln` namespace to avoid collision with other tools or libraries).

##### Signatures

Alloc takes one `int64_t` argument per dimension (outermost to innermost) and returns `void*`:

```c
void* __pln_alloc_arr_arr_int64(int64_t d0, int64_t d1);
void* __pln_alloc_arr_arr_arr_int64(int64_t d0, int64_t d1, int64_t d2);
void* __pln_alloc_arr_MyStruct(int64_t d0);
```

Free takes the array pointer (`void*`) plus all dimensions **except the innermost**
(the innermost size is not needed because `free` does not require it):

```c
void __pln_free_arr_arr_int64(void* arr, int64_t d0);
void __pln_free_arr_arr_arr_int64(void* arr, int64_t d0, int64_t d1);
void __pln_free_arr_MyStruct(void* arr, int64_t d0);
```

Each shape maps to exactly one allocator function and one free function.

#### Cross-module deduplication (build manager aggregation)

When multiple source files require allocators for the same shape, generating the function
independently in each module would cause duplicate symbol errors at link time.

To avoid this, palan-sa does **not** emit allocator function bodies into sa.json directly.
Instead, it records the required allocator shapes in a dedicated `alloc-shapes` metadata
section of sa.json. The build manager collects `alloc-shapes` from all sa.json files,
deduplicates by shape key, generates a single allocator source file, compiles it once,
and links it with all modules.

This follows the same pattern as the existing `str-literals` table: SA collects metadata,
codegen (or build manager) handles the actual emission.

#### Allocation strategy selection (palan-sa)

palan-sa determines the allocation strategy per element type:

```
allocKind(elem_type):
  prim or pntr  → Direct: malloc(count * sizeof(elem))
  arr or struct → Generated: call __alloc_<shape>(counts...)
```

For generated allocators, SA records the shape in `alloc-shapes` and emits a call to the
corresponding function name. The function body is produced by the build manager step.

## 4. Expression Value Categories

Every expression in Palan has a **value category** that determines ownership semantics.
palan-sa annotates expressions with their category and uses it to drive free-tracking decisions.

| Category | Description | Examples |
|----------|-------------|---------|
| `owned`    | A named variable with ownership; SA tracks it for free at scope end | `[n]T` variable, `[n]@![]T` variable |
| `expiring` | An owned value being relinquished; must be consumed exactly once | return value of a function whose return type is a tracked array type; result of `->>` |
| `transient` | A plain temporary with no ownership; no tracking needed | integer literal, arithmetic result, `prim` function return value |

### Rules

- Assigning an `expiring` value to a variable → the variable becomes `owned`, SA adds it to free-tracking.
- Passing an `expiring` value as a function argument → the callee takes ownership (parameter becomes `owned`).
- An unused `expiring` value → freed at the end of the statement (or immediately after use).
- `return expr` where `expr` is `owned` → SA removes it from free-tracking (no free emitted); the caller is responsible for freeing.
- `val ->> target` (ownership-transfer arr-assign) → SA emits `val = NULL` after the transfer; the scope-end `free(val)` becomes `free(NULL)` which is a no-op.

### SA determination of expiring

A function call expression is `expiring` if the function's return type is a tracked array type
(`[]T`, `[][]T`, or any unsized array type). Otherwise it is `transient`.

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
  - Assembles `.s` files with `as` and links with `ld` to produce the final executable (default: `a.out`).

### 5.3 Error Output Format

All tools write error messages to **stderr** and exit with code **1** on error.

| Tool | Error format |
|------|-------------|
| palan-gen-ast | `<source_file>:<line>:<col>: error: <message>` (parse errors); `<message>` (CLI errors) |
| palan-sa | `<source_file>:<line>:<col>: error: <message>` |
| palan-codegen | `<message>` |
| palan (build-mgr) | `<message>` |
| palan-c2ast | `<source_file>:<line>:<col>: error: <message>` (preprocessor errors); `<message>` (CLI errors) |

