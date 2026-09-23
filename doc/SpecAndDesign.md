# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-09-19)
version: 0.1.32 — raw Linux syscall declarations

v0.1.31 closed the header-interop gaps in the "cinclude a C header, call the libc wrapper"
path. This iteration is a deliberate departure from that path: it lets Palan code issue a
Linux x86-64 syscall **directly**, bypassing libc entirely, via a new `syscall` declaration
that binds a syscall number (and a typed signature) to a name callable like any other
function:

```palan
export syscall write(int32 fd, @void buf, uint64 count) -> int64 = 1;

int64 n = write(1, msg, 13);   // called exactly like a normal function afterward
```

This is not a header-coverage gap; it is a new primitive. A libc wrapper like `write()`
(reachable today via `cinclude <unistd.h>;`) is an ordinary C function using the System V
AMD64 calling convention (`call`, arguments in rdi/rsi/rdx/**rcx**/r8/r9). A raw syscall uses
a different, lower-level ABI: the syscall number goes in `rax`, up to six arguments go in
rdi/rsi/rdx/**r10**/r8/r9 (note r10, not rcx — `syscall` clobbers rcx and r11 as a side
effect of the mode transition), the instruction is `syscall` rather than `call` (no symbol,
no PLT/GOT), and the return value is the raw contents of `rax` — a negative value conventionally
means `-errno`, which the caller must interpret itself since libc's `errno` variable is never
touched.

**Why a declaration, not a call-expression builtin.** An earlier design considered a single
variadic-shaped `syscall(nr, ...)` expression. It was rejected: the user wants a syscall to be
usable exactly like a typed function after one prototype declaration, including `export`.
`func_def` (`PlnParser.yy:876-897`) already carries prototype-only signatures into
`ast["export"]` when `do_export` is set — the same shape a syscall declaration needs, since it
has no block body of its own. Piggybacking on that existing export path was decisive over a
bespoke expression-level builtin, which cannot be exported or type-checked at call sites the
way a named function can.

**Why `syscall` is a reserved word, not a contextual keyword like `link`.** `unistd.h`
exports a real `long syscall(long, ...)` wrapper (`unistd.h:1091`), so reserving `syscall`
globally shadows that name — the same collision shape v0.1.31 hit with `link` (`unistd.h:819`),
which was resolved by keeping `link` a contextual keyword recognized only in `cinclude`'s
trailing clause. This iteration makes the opposite call for `syscall`: a contextual keyword
would have to disambiguate a statement-leading `ID ID '(' ...` from both a variable
declaration and an ordinary call-expression statement, which `link`'s position (after
`import_as`, where no other production can start) never had to do. A native syscall
declaration also makes the libc wrapper largely redundant — once `write` is declared as a
syscall, there is little reason to also want `cinclude <unistd.h>;`'s `syscall()` wrapper in
the same file. Verified empty: no test fixture in this repository uses `syscall` as an
identifier today.

**Design consequences of the ABI mismatch.** Every stage between AST and instruction emission
that currently assumes "a call is either a Palan call or a C call" (System V convention) gets
a third, register-convention-distinct case:
- `PlnDeserialize.cpp:178-211` switches on `func-type`; today anything other than `"c"` falls
  through to the Palan-call branch — a `"syscall"` func-type left unhandled there would silently
  miscompile as a Palan call rather than fail loudly.
- `PlnRegAlloc.cpp:322-341` binds call arguments to `phys.intArgs[arg_pos]` (rdi/rsi/rdx/rcx/r8/r9,
  table at `PlnX86CodeGen.cpp:7-11`) directly; a syscall's arguments bind to a different table
  (r10 in the fourth position). Per this project's normalization principle, the fix is not a
  syscall-shaped branch inside that shared consumer — it is giving the instruction its own
  argument-register table, so the consumer stays agnostic to which calling convention it is
  serving.
- Register clobbering has no generic per-instruction model today — `call` clobbers "every
  caller-saved register" as a blanket rule, and the one precedent for a *specific* pair
  (`%rax`/`%rdx` for `Div`/`Mod`) is a hand-written parallel index list (`divmod_indices`,
  `PlnRegAlloc.cpp:26,74-75,349-361`), not a data-driven clobber set. `syscall`'s rcx/r11
  clobber follows that same precedent (`syscall_indices`) rather than inventing a general
  mechanism this iteration doesn't otherwise need.
- Emission itself (`PlnX86Call.cpp:76-162` for the existing C-call path, 87 lines handling
  varargs, stack-argument overflow, and struct-return copyback) is structurally *larger* than
  what a syscall needs: fixed arity ≤ 6, all-integer-class arguments, no stack overflow, no
  symbol resolution, single-register return.

**The syscall number must be a literal integer, not a symbolic constant.** A cinclude'd macro
constant like `SYS_write` is fully folded by c2ast at gen-ast time, but a *reference* to that
name elsewhere in Palan source is only resolved later, in SA, against the referencing module's
own symbol table — it has no self-contained value by the time `ast.json` is written, so it
cannot cross an `import` boundary the way the declaration itself needs to (see "Design
consequences of the ABI mismatch" above: the number is carried in the export/import path as a
plain value, not re-resolved by the importing module). Restricting the syscall number to a
literal integer sidesteps this rather than fixing it; the underlying gap is tracked as
`doc/Issues.md` item 23.

Non-goals for this iteration: a general variadic `syscall(nr, ...)` call-expression builtin
(superseded by the declaration approach above); automatic errno translation (the raw `rax`
value is returned as-is); flo32/flo64 syscall arguments or return values, and 8-bit/16-bit
integer arguments or return values (the Linux syscall ABI has no partial-register argument/
return slot — diagnosed, not implemented); first-class function-pointer types (`doc/Issues.md`
item 14, unchanged); union/enum/`long double` representation (`doc/Issues.md` item 14,
unchanged); and any static-linking or CRT strategy beyond what already exists (`doc/Issues.md`
item 18, undesigned).

The full design decisions and the ticket breakdown are in
`localtickets/iteration-2026-09-19-v0132-syscall.md`.

The series' underlying goal shifts here: earlier header-interop iterations used C headers as a
forcing function for general C-interop language capability. This iteration instead adds a new
primitive orthogonal to header interop — direct OS-level access below libc — motivated by the
same practical need (calling POSIX-adjacent functionality) that made syscalls the next
candidate after pthread.h was assessed and set aside for the union/bitfield representation
gaps it would immediately hit (`doc/Issues.md` item 14).


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
 Before linking, the build manager unions the `libs` array (see SASpec.md's Root) across every
 module's sa.json, in the same pass it already makes over all modules to collect `alloc-shapes`,
 and appends `-l<name>` to the `ld` invocation for each library named by a `link` clause on a
 `cinclude` statement (e.g. `cinclude <math.h> link "m";`) anywhere in the program.
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
 A cinclude's `typedefs` (ASTSpec.md) are registered into the type-alias table unconditionally,
 independent of whether the header declares any functions, and a cinclude's `link` clause libraries
 are validated, deduplicated, and emitted as sa.json's top-level `libs` array (SASpec.md's Root) --
 the one piece of a cinclude statement that survives past this consume-and-drop step.
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

 Palan links no crt startup objects (no crt1.o/crti.o/crtbegin.o -- the build manager
 links `ld <objs> -lc` directly), so the code generator supplies the ELF/libc glue those
 objects would otherwise provide: the entry object defines `__dso_handle` (needed by
 glibc's `atexit`/`at_quick_exit`/`pthread_atfork`, which forward to `__cxa_atexit`-family
 calls taking it), and every object emits `.note.GNU-stack` (otherwise `ld` marks the
 whole binary's stack executable as soon as any note-carrying libc object joins the link).


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

