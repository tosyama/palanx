# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-07-18)
version: 0.1.26
- This iteration is the first of a planned series of incremental C standard library support iterations, targeting `string.h` and `ctype.h`. **The actual goal of this whole series is not "make these specific functions callable" — it's to use concrete C headers as a forcing function to grow Palan's own C-compatible language surface.** Header support is the vehicle; language-feature growth is the point. Concretely, this means: when a gap traces back to an existing-but-dormant Palan grammar stub, the disposition is to *implement that stub for real*, not to route around it with a narrower, invisible substitute — see gaps 1 and 6 below, both of which land on unimplemented `type`/`const` declaration grammar already present in `PlnParser.yy`. The goal is defined by a **completeness audit**, not by a hand-picked list of convenient functions: every function declared by these two headers (as seen by `palan-c2ast` against this project's real glibc/x86-64 system headers) was enumerated and classified by what Palan capability it needs. Only categories with a concrete, scoped fix are shipped this iteration; every deferred category is listed explicitly with its reason, not silently dropped.

**Definition of done**

A function counts as "supported" only if it (a) is registered end-to-end through the real pipeline (`gen-ast → sa → codegen → execute`, not just inspected via `palan-c2ast` JSON output), and (b) has a `build-mgr` test that exercises real semantics — including the "not found" / boundary case where the function has one (e.g. a `strchr` miss, not just a hit). A header counts as "supported" only when every function in it is accounted for in one of: implemented-and-tested this iteration, or listed in the deferred/excluded tables below with a reason. `ctype.h` in particular currently yields **zero** usable functions (see gap 3) — "supported" for it means going from 0 to the full non-locale public surface, not incremental improvement.

**Gap catalog**

| # | Gap | Root cause | Disposition |
|---|---|---|---|
| 1 | No typedef table in c2ast — `size_t` surfaces as opaque `{"type-kind":"user","type-name":"size_t"}` | `CParser.cpp:559-624` consumes and discards `typedef` declarations | **Fix, via a dormant Palan language feature.** Palan's grammar already has a type-alias declaration, `type Name = type_expr;` (`PlnParser.yy:619`), but its action is a no-op stub (`{ $$ = json{}; }` — parses, produces nothing). Implement it for real: a named alias resolving to any `type_expr`, registered so the alias name itself is usable in Palan code and diagnostics. Then have c2ast track `typedef <underlying> <name>;` chains and, when a chain bottoms out in a primitive, have SA register the C typedef as a genuine Palan alias through this same mechanism (`size_t` → a real alias to `uint64`, not an invisible rewrite — a `strlen()` result keeps the name `size_t` in Palan source and error messages). Typedefs that don't bottom out in a primitive (e.g. a struct typedef) stay tagged `"user"`, unchanged from today — still errors if actually used; that's struct-interop work for a later iteration. |
| 2 | No `void` handling in Palan's type system — `pntr(void)` throws in `fromJson` | `PlnType.cpp` `PrimTypeNames` has no `"void"` entry | **Fix.** Recognize `pntr(void)` as an opaque, sizeless-element pointer, accepted wherever a `pntr(T)` argument is passed and vice versa. No dereference/element-access support needed — pass-through only. |
| 3 | Relational/equality operators unimplemented in c2ast's constant-expression parser — **blocks 100% of `ctype.h`**, not a partial gap | `CParser.cpp:869-893`, both `relational_expression` and `equality_expression` are stubs marked `// TODO`, falling through without consuming `<`/`>`/`<=`/`>=`/`==`/`!=` | **Fix.** glibc's `<ctype.h>` defines its internal `_IS*` enum via a `_ISbit(bit)` macro expanding to a ternary containing `<`, and the parser aborts entirely at that `enum` (confirmed by minimal repro: `enum{X=0<8};` fails, `enum{X=1<<8};` succeeds). Because the parser aborts the *whole file* on first unparseable construct, every declaration after it — including all of `isalpha`/`isdigit`/etc. — is never reached today. Fix: implement the four relational + two equality operators, same binary-expr AST shape as `shift_expression`. Enum constant *values* stay parsed-and-discarded (`enum_definition` never stores them) — this fix only unblocks parsing, not enum-value usability. |
| 4 | `char**` (pointer-to-pointer) out-params, needed by `strtok_r`/`strsep` | Palan has no address-of operator (`&` is bitwise-and only) and no confirmed path to construct `pntr(pntr(T))` from a local variable | **Defer.** This is a new language feature (address-of / out-params), not a C-header-binding gap — out of scope for a header-support iteration. The existing `strtof` test's `int64(0)` workaround for its `char**` param is evidence this has never actually worked. |
| 5 | `locale_t` param on `_l`-suffixed functions | `locale_t` = `typedef struct __locale_struct *locale_t` — an opaque struct pointer; `fromJson` throws on `"strct"`/`"union"`/`"enum"`/`"user"` (`PlnType.cpp:64-77`) the moment such a signature is resolved | **Defer**, same root cause as the planned `time.h`/`struct tm` struct-interop iteration (opaque-type handling in `fromJson`, manual C-binding syntax, `raw-ptr`-to-primitive field bug in `PlnSaInternal.h:58`). Not re-solved here. |
| 6 | No mechanism to import C constants (`#define`, enum) as Palan values — blocks `NULL`, needed to test the "not found" return of `strchr`/`strstr`/`strpbrk`/`memchr`, and to use `strtok`'s standard multi-call idiom (`strtok(NULL, delim)`) | `palan-c2ast`'s AST has only a `"functions"` section; `NULL` is just `#define NULL ((void*)0)` (confirmed for this target: C mode, not C++/G++) — not special syntax. Separately, Palan's own grammar has a `const_decl: KW_CONST ID '=' expression;` rule that is also unimplemented (no real action). | **Fix, via the dormant `const` declaration.** Implement `const_decl` for real: a named, typed, immutable value usable like any other identifier. c2ast already keeps a macro table (`CPreprocessor.h`: `vector<CMacro*> &macros`, each with `name`/`params`/`body` tokens) and already has a constant-expression parser (`cast_expression` etc.) — reuse both: after preprocessing, walk the macro table, and for each object-like (no-params) macro whose `body` parses as a `constant_expression`, emit it into a new AST `"constants"` section. SA registers these through the newly-implemented `const` mechanism, in parallel with `sa_cinclude`'s existing function registration — so an imported C constant is a first-class Palan `const`, not a special-cased shim. `NULL` (typed `pntr(void)`, implicitly compatible with and `==`/`!=`-comparable against any `pntr(T)` — the same compatibility rule gap 2 already needs) is the concrete instance this iteration needs and tests, but the mechanism itself is general. **Enum-constant export stays out of scope** — unlike gaps 1 and 6's macro-constant path, there is no existing dormant Palan feature it would complete; it would be new design, and no in-scope function requires one (`ctype.h`'s `_IS*` bits are internal/double-underscore-adjacent; callers use the wrapping functions, not the bits). |
| 7 | Reserved/internal symbols (leading double-underscore) | e.g. `__ctype_b_loc`, `__memcmpeq`, `__strtok_r` | **Excluded from scope**, not a gap — these are implementation-reserved identifiers, not public API, matching how a C caller would treat them. |
| 8 | Header content visible to `palan-c2ast` depends on feature-test macros (`_GNU_SOURCE` etc.) | c2ast's `predefined.h` defines a fixed default set | **Scope boundary, not a gap.** "Supported" means whatever `palan-c2ast -s <header>` exposes under this project's current `predefined.h`, as audited below — not chasing every possible glibc extension. |
| 9 | No ownership tracking for C-allocated raw pointers (e.g. `strdup`/`strndup` return heap memory Palan doesn't know needs freeing) | Palan's auto-free machinery only covers `structDefs_`/array allocations, not raw `pntr(prim)` returned from `cinclude`d calls | **Documented accepted limitation**, not a blocker — same manual-free burden C itself has. The user must `cinclude <stdlib.h>;` and call `free()` explicitly. Called out here so it isn't mistaken for an oversight. |

**Full function inventory — `string.h`** (59 declared functions under current `predefined.h`; ✅ = in scope this iteration)

| Category | Disposition | Functions |
|---|---|---|
| A — already resolves to supported types, no new infra | ✅ implement + test | `strcat`, `strcasecmp`, `strcasestr`, `strchr`, `strchrnul`, `strcmp`, `strcoll`, `strcpy`, `strdup`, `strerror`, `strncasecmp`, `strpbrk`, `strrchr`, `strsignal`, `strstr`, `strtok`, `stpcpy`, `index`, `rindex`, `ffs`, `ffsl`, `ffsll` |
| B — needs gap 1 (size_t) only | ✅ implement + test | `strlen`, `strncpy`, `strncat`, `strncmp`, `strcspn`, `strspn`, `strnlen`, `strndup`, `strxfrm`, `strlcat`, `strlcpy`, `stpncpy` |
| C — needs gaps 1+2 (size_t + void*) | ✅ implement + test | `memcpy`, `memmove`, `memset`, `memcmp`, `memchr`, `memccpy`, `memmem`, `mempcpy`, `bcmp`, `bcopy`, `bzero`, `explicit_bzero` |
| D — needs gap 4 (`char**`) | ❌ deferred | `strtok_r`, `strsep` |
| E — needs gap 5 (`locale_t`) | ❌ deferred | `strcasecmp_l`, `strncasecmp_l`, `strcoll_l`, `strxfrm_l`, `strerror_l` |
| F — reserved/internal (gap 7) | ❌ excluded | `__memcmpeq`, `__mempcpy`, `__stpcpy`, `__stpncpy`, `__strtok_r`, `__xpg_strerror_r` |

**Full function inventory — `ctype.h`** (all currently unreachable — gap 3 blocks the whole header)

| Category | Disposition | Functions |
|---|---|---|
| A, blocked only by gap 3 | ✅ implement + test (once gap 3 fixed) | `isalnum`, `isalpha`, `iscntrl`, `isdigit`, `islower`, `isgraph`, `isprint`, `ispunct`, `isspace`, `isupper`, `isxdigit`, `isblank`, `tolower`, `toupper`, `isctype`, `isascii`, `toascii`, `_toupper`, `_tolower` |
| E — needs gap 5 (`locale_t`) | ❌ deferred | `isalnum_l`, `isalpha_l`, `iscntrl_l`, `isdigit_l`, `islower_l`, `isgraph_l`, `isprint_l`, `ispunct_l`, `isspace_l`, `isupper_l`, `isxdigit_l`, `isblank_l`, `tolower_l`, `toupper_l`, `__tolower_l`, `__toupper_l` |
| F — reserved/internal (gap 7) | ❌ excluded | `__ctype_b_loc`, `__ctype_tolower_loc`, `__ctype_toupper_loc` |

**Explicit non-goals (deferred to later iterations, with reason already stated above)**
- Struct-by-value or struct-pointer C interop (`time.h`/`struct tm`, `sys/stat.h`, `locale_t`-based `_l` functions) — gap 5.
- `char**` out-params (`strtok_r`, `strsep`) — gap 4; revisit once/if an address-of operator is designed.
- Enum-constant export (`ctype.h`'s `_IS*` bits and similar) — gap 6, not needed by any in-scope function.
- Typedefs that resolve to non-primitive (struct) types.
- Wide-character (`wctype.h`/`wchar.h`) variants — not audited this iteration.

The goal is that the following program produces correct output:

```palan
cinclude <string.h>;
cinclude <ctype.h>;
cinclude <stdio.h>;

[32]uint8 buf;
strcpy(buf, "Hello, Palan!");
size_t n = strlen(buf);                           // gap 1: size_t is a real Palan alias type (-> uint64), not an invisible rewrite
printf("%s (len=%ld)\n", buf, n);                 // Hello, Palan! (len=13)

printf("%d\n", strcmp("abc", "abc"));             // 0
printf("%d\n", strncmp("abcdef", "abcxyz", 3));   // 0

[8]uint8 dst;
memset(dst, 0, 8);
memcpy(dst, "abc", 3);
printf("%s\n", dst);                              // abc

printf("%d %d\n", isalpha(97), isdigit(97));      // 1 0 ('a' = 97)
printf("%c\n", toupper(97));                      // A

// NULL (gap 6) exercised via the strchr "not found" case and strtok's multi-call idiom
[16]uint8 line;
strcpy(line, "a,bb,ccc");
if (strchr(line, 122) == NULL) {                  // 'z' not present
    printf("not found\n");                         // not found
}
printf("%s\n", strtok(line, ","));                 // a
printf("%s\n", strtok(NULL, ","));                 // bb
printf("%s\n", strtok(NULL, ","));                 // ccc
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

