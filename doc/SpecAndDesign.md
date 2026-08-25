# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-08-24)
version: 0.1.27
- This iteration continues the C standard library support series (started in v0.1.26 with `string.h`/`ctype.h`), now targeting `time.h`. **The series' goal is unchanged: header support is the forcing function, language-feature growth is the point.** `time.h`'s defining characteristic versus the previous headers is that essentially its whole useful surface is gated on C **struct-pointer interop** (`struct tm`, `struct timespec`, `struct itimerspec`) — v0.1.26 explicitly flagged this as the next planned struct-interop iteration (its gap 5 note). A pre-implementation audit (this section) found that Palan's existing native-struct machinery (`type Name { ... }`, v0.1.22–v0.1.25: C-ABI layout, heap allocation, borrowed-pointer function params, embedded/owned/non-owning field kinds, struct arrays) is representationally identical to what a `cinclude`d C struct needs — a Palan struct variable's declared type is already `pntr(struct X)` under the hood (`PlnSaExpr.cpp:20`), the same shape a C function returns for `struct tm *`. The gap is entirely on the **import path**: `palan-c2ast` parses and discards struct tag names and field lists (`CParser.cpp:275-317`, `struct_union_definition`), so nothing is there yet for SA to register. Fixing that import path is this iteration's main body of work, following the same discipline as v0.1.26: a completeness audit over every function `time.h` declares, each function classified by what capability it needs, every deferred category listed with its reason.

**Definition of done**

Same bar as v0.1.26: a function counts as "supported" only if it is registered end-to-end through the real pipeline (`gen-ast → sa → codegen → execute`) and has a `build-mgr` test exercising real semantics — for struct-typed functions specifically, that means asserting on actual field values read back after the call, not just a successful return code. A struct type counts as "supported" once cinclude-imported structs go through the exact same `structDefs_`/field-layout path as a native `type Name { ... }` — i.e. no parallel/shadow implementation for C-origin structs. `time.h` counts as "supported" once every one of its declared functions is accounted for in the inventory below as implemented-and-tested or deferred-with-reason.

**Gap catalog**

| # | Gap | Root cause | Disposition |
|---|---|---|---|
| 1 | c2ast parses but discards struct tag name and field list | `CParser.cpp:275-317` (`struct_union_definition`) consumes the `{ ... }` body positionally but never writes it into the returned `json`; the caller at `CParser.cpp:407` then emits only `{"type-kind":"strct"}` with no `type-name` and no fields, for both struct *definitions* and bare struct-pointer *references* in signatures | **Fix.** Capture the tag name and an ordered `(name, type)` field list during `struct_union_definition`, and thread `type-name` through onto every `"strct"`-kind reference (so a `struct tm *__tp` parameter names `tm`, not just "some struct"). Emit struct definitions into a new top-level AST section, analogous to how functions are already emitted. |
| 2 | SA has no path from a `cinclude`d C struct to `structDefs_` | `PlnTypeRegistry::fromJson` (`PlnType.cpp:79-82`) only understands Palan-native `"struct"` type-kind; c2ast's `"strct"` kind hits the `throw` at line 82 the moment any `time.h` function with a struct param/return is resolved | **Fix, by reusing existing dormant capacity rather than building a parallel struct system.** Once gap 1 supplies name+fields, register each cinclude-imported struct into the same `structDefs_`/`FieldLayout` table a native `type Name { ... }` populates (v0.1.22). This is a straight reuse, not new design — it's why `struct itimerspec { struct timespec it_interval; struct timespec it_value; }` needs no special handling: a C struct-typed (non-pointer) field maps directly onto the existing `$T` embedded-field kind (v0.1.23), which is already C-ABI-layout-compatible by construction. It also transparently unlocks *binding a C function's returned struct pointer* to a Palan variable, with no further design needed: `var_declaration` already accepts any `type_expr` (`PlnParser.yy:536,582`), `@T`/`@!T` are already general `type_expr` productions (not struct-field-only), and `PlnSaDecl.cpp:221` already reinterprets a `type-kind:"prim"` name as a struct when it matches a registered `structDefs_` entry — so `@!tm p = gmtime(t);` just works once `tm` is registered, going through the exact same non-owning-pointer path (`@T`/`@!T` are defined as "lifecycle is user-managed", no auto-free — `PalanReference.md` §19) that already fits a glibc-owned static buffer perfectly. |
| 3 | C-returned/param struct pointers must not be treated as Palan-owned, auto-freed memory | Palan's own struct locals are always heap-allocated via `__pln_alloc_T` and auto-freed at scope exit; a `struct tm *` returned by glibc (static internal buffer) or passed in by the caller is neither | **Fix.** Struct types arriving through a `cinclude` function signature are borrowed pointers, with no auto-free registration. Both directions reuse an existing convention, not new design: a struct passed *into* a C call is an owned Palan-declared local, matching the convention Palan already applies to its own struct-type function parameters ("passed as a pointer, borrowed, not freed by the callee", `PalanReference.md` §19); a struct pointer *returned by* a C call binds into an explicitly non-owning `@T`/`@!T`-typed Palan variable (see gap 2's note) — the ownership-transferring named-return convention (`-> Point p`) is deliberately *not* used here, since that model is wrong for a pointer Palan didn't allocate. |
| 4 | No address-of operator — blocks any C call needing a pointer to a primitive value the caller doesn't already hold as a pointer (`gmtime`/`gmtime_r`'s `const time_t *`, `clock_getcpuclockid`'s `clockid_t *` out-param) | Carried over from v0.1.26 gap 4; previously deferred as "a new language feature, out of scope for a header-support iteration" | **Fix, minimal scope only — reuse `@`/`@!`, don't borrow C's `&`.** `@T`/`AT_EXCL T` (`@!T`) already exist as `type_expr` productions (`PlnParser.yy:966-972`) meaning "read-only pointer to T" / "mutable pointer to T" respectively (`{"type-kind":"pntr", ...}`, `mutable:true` for `@!`) — this is Palan's own general pointer-type vocabulary, not struct-field-only syntax. Extend `@`/`AT_EXCL` into a matching *expression*-position production, `'@' ID` / `AT_EXCL ID` where `ID` names a local variable of primitive type, yielding a `pntr(T)` (`@`) or mutable `pntr(T)` (`@!`) to that variable's storage. Unlike reusing `&`, neither token has any existing expression-position meaning, so no `%prec` disambiguation against a binary use is needed (contrast with `-`'s existing `UNARY_MINUS` trick) — and the `@`/`@!` split gives the read-only/mutable pointer distinction C's `const T*`/`T*` needs for free, with no new symbol invented. General address-of-anything (struct fields, array elements, temporaries) and a general pointer-dereference operator stay out of scope; this narrow `'@' ID` form is what's needed to pass a primitive local by address into a `cinclude`d C call. |
| 5 | Incomplete/forward-declared struct type blocks `timer_create` | `time.h` forward-declares `struct sigevent;` with no body in this project's `predefined.h` visibility — no field list exists to capture even with gap 1 fixed | **Defer.** Not a scoped fix — there's nothing to register. `timer_create`/`timer_settime`/`timer_gettime`'s non-`sigevent` neighbors (`timer_delete`, `timer_getoverrun`, and `timer_settime`/`timer_gettime` themselves, which use `struct itimerspec` not `struct sigevent`) are unaffected and stay in scope. |
| 6 | `locale_t` param on `strftime_l` | `locale_t` = `typedef struct __locale_struct *locale_t`; unlike v0.1.26's assessment, `__locale_struct` technically *does* have a full field list now capturable via gap 1+2 — but its fields reference further incomplete types (`struct __locale_data *`, itself forward-declared only) and internal-only members no caller would touch | **Defer.** Technically reachable but not worth registering; the `_l` locale surface stays out of scope generally, independent of struct-interop capability now existing. Same disposition as v0.1.26 gap 5, refined reasoning. |
| 7 | `CLOCK_REALTIME`, `CLOCK_MONOTONIC`, `TIMER_ABSTIME`, `TIME_UTC`, etc. | Confirmed (against this project's real `/usr/include/x86_64-linux-gnu/bits/time.h`) to be plain object-like `#define` macros, not enum constants | **Scope boundary, not a gap.** Already covered by v0.1.26's gap-6 `const`-import mechanism — no new work needed; these are exercised in the tests below as ordinary imported constants. |

**Full function inventory — `time.h`** (30 declared functions under current `predefined.h`; ✅ = in scope this iteration)

| Category | Disposition | Functions |
|---|---|---|
| A — primitive-only, no new infra beyond v0.1.26 | ✅ implement + test | `clock`, `difftime`, `dysize`, `tzset`, `timer_delete`, `timer_getoverrun` |
| B — needs gaps 1+2+3 (struct-interop, struct always caller-owned/passed-in, no return-binding involved) | ✅ implement + test | `mktime`, `strftime`, `asctime`, `asctime_r`, `timegm`, `timelocal`, `nanosleep`, `clock_getres`, `clock_gettime`, `clock_settime`, `clock_nanosleep`, `timer_settime`, `timer_gettime`, `timespec_get` |
| C — needs gap 4 (`@`/`@!` address-of) only, no struct involved | ✅ implement + test | `time` (also reachable via `time(NULL)` alone, gap 6 v0.1.26), `ctime` (`@t`), `ctime_r` (`@t`), `clock_getcpuclockid` (`@!clk_id` — mutable out-param) |
| D — needs gaps 1+2+3+4 together: struct-interop, address-of for the `time_t*` input, *and* binding the returned `struct tm *` into an `@!tm`/`@tm`-typed local (gap 2's note — already-existing non-owning-pointer mechanism, no new design) | ✅ implement + test | `gmtime`, `localtime`, `gmtime_r`, `localtime_r` |
| E — needs gap 5 (incomplete `struct sigevent`) | ❌ deferred | `timer_create` |
| F — needs gap 6 (`locale_t`) | ❌ deferred | `strftime_l` |

**Explicit non-goals (deferred to later iterations, with reason already stated above)**
- `timer_create` — gap 5, blocked on `struct sigevent` having no visible definition.
- `strftime_l` and the rest of the `_l`/`locale_t` surface — gap 6, same as v0.1.26.
- General address-of (fields, array elements, temporaries) and a general pointer-dereference operator — only the narrow `@ID`/`@!ID` local-primitive-variable form ships this iteration (gap 4).
- `sys/stat.h` and other struct-heavy headers — not audited this iteration; `time.h` is the vehicle for landing the struct-interop mechanism, not an exhaustive struct-header pass.

The goal is that the following program produces correct output:

```palan
cinclude <time.h>;
cinclude <stdio.h>;

time_t t = time(NULL);                    // gap 6 (v0.1.26): NULL already usable where a time_t* out-param is optional

struct tm tv;
gmtime_r(@t, tv);                         // gap 4: @t is a read-only pointer to the primitive local t (matches the
                                            // C signature's `const time_t*`); tv is an owned Palan struct local,
                                            // passed by (borrowed) pointer like any Palan struct param
printf("%d-%02d-%02d\n",
       int32(tv.tm_year) + 1900, int32(tv.tm_mon) + 1, int32(tv.tm_mday));

@!tm p = gmtime(@t);                      // gap 2/3: p is a non-owning pointer local (no auto-free — matches
                                            // gmtime's static-buffer return); field access works the same as tv above
printf("%d\n", int32(p.tm_year) + 1900);

struct timespec ts;
clock_gettime(CLOCK_REALTIME, ts);        // CLOCK_REALTIME: a #define macro, already importable as a const (v0.1.26)
printf("clock_gettime ok: %d\n", ts.tv_sec >= int64(0));

struct tm mt;
70  -> mt.tm_year;   // 1970
0   -> mt.tm_mon;    // January
1   -> mt.tm_mday;
0   -> mt.tm_hour;   0 -> mt.tm_min;   0 -> mt.tm_sec;
printf("%ld\n", mktime(mt));              // 0 (epoch, UTC-equivalent local zone in test env)
```


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

