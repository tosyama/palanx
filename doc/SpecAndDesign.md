# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-08-30)
version: 0.1.28 — **pointer dereference and general address-of**, together with the
c2ast ingestion fixes that a `sys/stat.h` audit exposed.

v0.1.27 (`time.h` + C struct interop, `@ID`/`@!ID` address-of) left pointers as a
write-only channel into C: a Palan program could produce an address and hand it to a C
function, but could not read or write through one itself, and `@`/`@!` were restricted to
local primitive variables. This iteration closes that hole and makes the read-only/mutable
distinction actually mean something.

The iteration was scoped from a pre-implementation audit of the two leading candidates
(general address-of/dereference, and `sys/stat.h` as the next struct-heavy header). That
audit produced three findings that reshaped the plan, recorded here because they are the
reason the ticket order is what it is.

**Audit finding 1 — the codegen IR already has everything.** `PlnVProg.h:56-62` already
defines `CalcAddr`, `CalcAddrIdx`, `DerefLoad`, `DerefStore` and `LeaLocal`, and both
`FieldAccessExpr` and `ArrIndexExpr` already carry an `addrOnly` flag (`PlnNode.h:247,285`)
that SA already emits as `addr-only` (`PlnSaExpr.cpp:246`) for embedded 2D row access and
embedded array fields. Generalizing address-of is therefore a gen-ast (syntax) and SA
(type rules) change, with no expected codegen work.

**Audit finding 2 — `p[0]` dereference already works, but unsoundly.** `sa_expr_arr_index`
requires only that the base's `value-type` be `pntr` (`PlnSaExpr.cpp:399`); a plain
`@!int64 p` carries none of the array attributes (`embedded`/`stride`/`inner-size`) and so
falls through to the scalar path (`PlnSaExpr.cpp:479-489`), which emits
`DerefLoadIdx{base, idx, scale=sizeof(elem)}` — exactly dereference semantics. Both
`printf("%ld", p[0])` and `99 -> p[0]` were verified to work on the v0.1.27 tree. This is
an accident of Palan's representation of arrays as `pntr(T)` plus attributes, not a
designed feature: it is undocumented, untested, and **unsound**, because a read-only `@T`
pointer accepts a write through `p[0]` just as a mutable `@!T` one does. (Same root cause
as the note in IT-2026-08-24-2703: `typeCompat` compares interned `PtrType`s and never
looks at the `mutable` flag.) The dereference work is therefore mostly specification,
soundness and tests rather than new lowering.

**Audit finding 3 — c2ast discards C array declarators, and that is a memory-safety bug.**
`declarator_tail` (`CParser.cpp:494-500`) parses `[...]` and throws away both the size and
the array-ness. Measured on the v0.1.27 tree: in `struct Rec { int id; char name[16];
long vals[4]; };` the fields `name` and `vals` are registered as a *single* `int8` and a
*single* `int64`, and in `int f(char buf[32], struct Rec recs[2])` the parameters become
bare `int8` and bare `strct Rec` instead of the pointers C's array-to-pointer decay
requires. `struct stat` is hit directly: its trailing `__syscall_slong_t
__glibc_reserved[3]` becomes one `int64`, so Palan computes `sizeof(struct stat)` as 128
instead of 144 — a buffer allocated by Palan and handed to `stat()` is overflowed by 16
bytes. `sys/stat.h` is not merely unsupported today, it is unsafe, so this fix is a
prerequisite for the header work rather than part of it.

#### Gap catalog

| # | Gap | Plan |
|---|-----|------|
| 1 | Native function named returns typed `@T`/`@!T` crash (`unknown prim type-name`, `PlnType.cpp:72`) — the return type never passes through `deepNormalizePrimToStruct` | **Fix (prerequisite bug, own commit)**: normalize at the point where a function's return types are registered, the same ingestion-boundary treatment IT-2026-08-24-2703 applied to `sa_var_decl`. Known since IT-2703; IT-2709 confirmed it is a distinct code path from the `gmtime` binding and left it open |
| 2 | c2ast discards C array declarators (audit finding 3) | **Fix (prerequisite bug, own commit)**: make `declarator_tail` reflect `[...]` in the type — an array type for struct fields and non-parameter declarators (so layout and total size are right), array-to-pointer decay for parameters. Normalize at the ingestion boundary so `structDefs_` and the function tables only ever see canonical shapes |
| 3 | Macro constants whose body is another macro name are dropped (`#define S_IFDIR __S_IFDIR` exports `__S_IFDIR` but not `S_IFDIR`) | **Fix (prerequisite bug, own commit)**: `exportMacroConstants` (`CParser.cpp:1191`) feeds the raw macro body to `constant_expression`; expand the body first, then evaluate |
| 4 | `@T` (read-only) pointers accept writes (audit finding 2) | **Implement**: enforce mutability in SA on the store path, so a write through a non-mutable `pntr` is rejected, and on every binding site (var-decl, assign, return, field-assign, Palan call arguments) so a `@T` value can never upgrade to `@!T`. The rule for passing a `@T` value to a non-`const`-qualified C parameter is deferred — see Non-goals |
| 5 | Pointer dereference is undocumented, untested, and unspecified | **Specify**: adopt `p[0]` as the dereference form — it needs no new token, reads the same as C, and reuses the existing `arr-index` path in both directions. Work is SA/build-mgr tests plus a `PalanReference.md` rewrite, not new lowering |
| 6 | Address-of is limited to whole local primitive variables | **Implement**: extend `@`/`@!` to struct fields (`@p.field`) and array elements (`@arr[i]`), reusing `resolveObjectChain` in SA and the existing `addr-only` / `CalcAddr` / `CalcAddrIdx` machinery in codegen |

#### Non-goals

- `sys/stat.h` function support itself. Gap 2 uses `struct stat` only as its verification
  subject; binding `stat`/`fstat`/`chmod`/… and their `S_IS*` predicates is the next
  iteration's work, once the layout it depends on is correct.
- Function-like macros (`S_ISDIR(m)` and friends). A genuinely new export mechanism, and
  not needed by anything in this iteration.
- Address-of on function parameters, and on whole struct or array variables. The latter
  would produce a pointer to a pointer, which needs the semantics settled first.
- `timer_create` / `struct sigevent`: still deferred. The type is not merely incomplete —
  the real definition (`bits/types/sigevent_t.h`) needs anonymous unions, an anonymous
  nested struct, function-pointer members, and `__sigval_t` (itself a union). That is a
  large type-system expansion in exchange for one function.
- `strftime_l` / `locale_t`: deferred again, same reasoning as v0.1.26 and v0.1.27.
- Rejecting `@T` (read-only) passed to a non-`const`-qualified C parameter. c2ast's
  `const` capture is itself incomplete (struct/union/enum pointee `const` and struct-field
  `const` are both dropped, `CParser.cpp:294,410-439`), so enforcing this now would produce
  false positives against `const struct T *` parameters (`asctime`, `strftime`, `nanosleep`).
  Tracked as its own follow-up: `IT-2026-08-31-c2ast-const-capture.md`.

#### Definition of done

- A native function may declare a `@T`/`@!T` named return without crashing the analyzer.
- A cincluded C struct containing array fields has a layout and total size matching the C
  ABI; `sizeof(struct stat)` is 144, and a Palan-allocated `struct stat` handed to `stat()`
  is not overflowed (verified under `mtrace`).
- A C parameter declared as an array is ingested as a pointer.
- `#define S_IFDIR __S_IFDIR` and equivalents are exported as usable constants.
- Writing through a `@T` pointer is a compile error; writing through `@!T` works.
- `p[0]` is documented in `PalanReference.md`, covered by sa-tester and build-mgr-tester
  cases in both directions, and its read/write asymmetry between `@T` and `@!T` is tested.
- `@p.field` / `@!p.field` and `@arr[i]` / `@!arr[i]` produce pointers usable as C
  arguments, and taking such an address neither allocates nor causes a free at scope exit
  (verified under `mtrace`).

The series' underlying goal is unchanged: header and feature support is the forcing
function for general C-interop language capability, not per-function coverage for its own
sake.


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

