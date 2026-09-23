# Palan Language Reference

**Version:** v0.1.33

Palan is a compiled systems programming language designed as a simpler, safer, and more enjoyable alternative to C. It targets developers who want low-level control and direct access to C libraries, without the sharp edges of C syntax. Palan code compiles to native x86-64 binaries via AT&T assembly, with no runtime overhead.

---

## Hello World

```palan
cinclude <stdio.h>;
printf("Hello World!\n");
```

---

## Quick Look

```palan
cinclude <stdio.h>;

// Define a function with multiple named return values
func sumsOf(int64 a, int64 b, int64 c) -> int64 ab, int64 bc {
    a + b -> ab;
    b + c -> bc;
    return;
}

// Top-level statements are the program entry point
int64 x = 1, y = 2, z = 3;
(int64 ab, bc) = sumsOf(x, y, z);
printf("%ld %ld\n", ab, bc);   // 3 5
```

---

## Table of Contents

1. [Command-Line Usage](#1-command-line-usage)
2. [Lexical Rules](#2-lexical-rules)
3. [Type System](#3-type-system)
4. [Variable Declarations](#4-variable-declarations)
5. [Expressions](#5-expressions)
6. [Statements](#6-statements)
7. [Function Definitions](#7-function-definitions)
8. [Receiving Multiple Return Values](#8-receiving-multiple-return-values)
9. [C Library Integration](#9-c-library-integration)
10. [If / If-Else Statements](#10-if--if-else-statements)
11. [Block Statements](#11-block-statements)
12. [Modules (import / export)](#12-modules-import--export)
13. [Program Structure](#13-program-structure)
14. [While Loops](#14-while-loops)
15. [break / continue](#15-break--continue)
16. [Optional Semicolons](#16-optional-semicolons)
17. [Floating-Point Types](#17-floating-point-types)
18. [Arrays](#18-arrays)
19. [Struct Types](#19-struct-types)
20. [Type Aliases](#20-type-aliases)
21. [Constant Declarations](#21-constant-declarations)
22. [Address-Of Operator](#22-address-of-operator)
23. [Raw Syscalls](#23-raw-syscalls)

---

## 1. Command-Line Usage

```
palan source.pa                  # compile, link, run -> removes a.out after execution
palan -o myapp source.pa         # compile & link -> ./myapp
palan --clean                    # clean intermediate files
```

The build manager (`palan`) orchestrates the full pipeline: parse → semantic analysis → code generation → assemble → link.

---

## 2. Lexical Rules

**Identifiers:** `[A-Za-z_][A-Za-z0-9_]*`

**Integer literals:**
- Signed: `[0-9]+` (e.g., `42`, `1000`)
- Unsigned: `[0-9]+u` (e.g., `42u`)

**String literals:** `"..."` with the following escape sequences:

| Escape | Meaning |
|--------|---------|
| `\n`   | newline |
| `\t`   | tab |
| `\r`   | carriage return |
| `\0`   | null byte |
| `\xHH` | hex byte |

**Comments:** `// line comment`

---

## 3. Type System

### Built-in Types

| Type    | Size    | Signedness |
|---------|---------|------------|
| `int8`  | 8-bit   | signed     |
| `int16` | 16-bit  | signed     |
| `int32` | 32-bit  | signed     |
| `int64` | 64-bit  | signed     |
| `uint8` | 8-bit   | unsigned   |
| `uint16`| 16-bit  | unsigned   |
| `uint32`| 32-bit  | unsigned   |
| `uint64`| 64-bit  | unsigned   |
| `flo32` | 32-bit  | float      |
| `flo64` | 64-bit  | float      |

### Implicit Widening

Within the same signedness group, narrower types widen automatically to wider types:

- `int8` → `int16` → `int32` → `int64`
- `uint8` → `uint16` → `uint32` → `uint64`

### Explicit Cast (Narrowing / Sign Conversion)

Narrowing conversions and signed↔unsigned conversions require an explicit cast using the `type(expr)` syntax:

```palan
int64 x = 100;
printf("%d\n", int32(x));   // explicit narrowing cast
```

### Usual Arithmetic Conversions (Binary Operators and Comparisons)

A binary arithmetic operator (`+ - * / % & | ^`) or a comparison (`< <= > >= == !=`) first
converts its two operands to one common type, then evaluates. The common type is chosen by
these rules, in order:

1. Either operand is a floating-point type → the float side wins (both float → the wider of
   the two; float paired with an integer → the float type — an integer is always widened to a
   float type it's compared or combined with, regardless of the integer's own width).
2. Both operands have the same signedness (both signed or both unsigned) → the wider
   (higher-ranked) type wins.
3. Signedness differs, and the signed operand is strictly wider than the unsigned operand →
   the signed type wins (e.g. `int64 & uint32` → `int64`).
4. Signedness differs, otherwise (including equal width) → the **unsigned** type wins (e.g.
   `int32 & uint32` → `uint32`).

Unlike C, there is **no integer promotion to a machine word**: `int8 + int8` stays `int8`,
preserving that width's own wraparound rather than promoting to a wider type first.

This rule governs binary arithmetic, bitwise operators (§5), and comparisons only — it is
**more permissive** than the rule for a binding site (a variable initializer, a plain or array
assignment, `return`, or a struct field assignment): all five of those always require an
explicit cast for a narrowing or a signedness change, with no exception for same-width
sign reinterpretation (see Explicit Cast above). A function call argument sits in between: it
accepts everything a binding site would, plus a same-width signedness reinterpretation (e.g. an
`int32` value passed where a `uint32` parameter is declared, or vice versa) — an argument only
needs to fit the callee's declared width, not match a variable's exact declared type. An integer
*literal* argument or initializer is the one further exception at any of these sites: it simply
adopts the destination's type instead of being checked for narrowing (this is what lets
`add(1, 2)` bind to `int32` parameters without a cast).

A pointer or struct operand has no common type with anything under this rule: using one with
`+ - * / % & | ^` is a compile error (Palan has no pointer arithmetic). A comparison is the
exception — `p == NULL` and similar pointer comparisons are valid and leave both operands
unconverted; a comparison's result is always `int32` regardless of operand type.

### Variadic Argument Promotion

When passing to variadic C functions (e.g., `printf`), small integer types are promoted:
- `int8`, `int16` → `int32`
- `uint8`, `uint16` → `uint32`

---

## 4. Variable Declarations

```palan
int64 x = 10;
int32 a = 5, b = 10;   // type inheritance: b is also int32
```

**Type inheritance:** In a comma-separated declaration, all variables after the first inherit the type of the first variable.

---

## 5. Expressions

| Expression | Syntax | Example |
|-----------|--------|---------|
| Integer literal | `[0-9]+` | `42` |
| String literal | `"..."` | `"hello\n"` |
| Identifier | `name` | `x` |
| Addition | `expr + expr` | `a + b` |
| Subtraction | `expr - expr` | `a - b` |
| Multiplication | `expr * expr` | `a * b` |
| Division | `expr / expr` | `a / b` |
| Modulo | `expr % expr` | `a % b` |
| Unary minus | `-expr` | `-x` |
| Bitwise AND | `expr & expr` | `a & b` |
| Bitwise OR | `expr \| expr` | `a \| b` |
| Bitwise XOR | `expr ^ expr` | `a ^ b` |
| Bitwise NOT | `~expr` | `~a` |
| Grouping | `(expr)` | `-(a + b)` |
| Comparison | `expr < expr`, `<=`, `>`, `>=`, `==`, `!=` | `x < 10` |
| Function call | `name(args)` | `add(3, 4)` |
| Explicit cast | `type(expr)` | `int32(x)` |
| Logical AND | `expr && expr` | `a && b` |
| Logical OR | `expr \|\| expr` | `a \|\| b` |
| Logical NOT | `!expr` | `!x` |
| Assignment expression | `expr -> var` | `x + 1 -> x` |

Operator precedence (high to low): unary minus / `!` / `~`, `* / % & | ^`, `+ -`, comparisons,
`&&`, `||`, `->`. All binary operators are left-associative.

Note the bitwise operators `&`/`|`/`^` share a precedence class with `*`/`/`/`%` — one level
**above** `+`/`-` — rather than C's much lower placement below comparisons. This is a deliberate
simplification, not an oversight, and it changes how mixed expressions parse relative to C:

- `a & b == c` parses as `(a & b) == c` — this actually *avoids* a well-known C pitfall, where
  the same expression parses as `a & (b == c)` because C's `&` sits below `==`.
- `a + b | c` parses as `a + (b | c)` — this is **different from C**, where `|`'s low precedence
  would instead parse it as `(a + b) | c`. Parenthesize explicitly when porting C expressions
  that mix `+`/`-` with `&`/`|`/`^`.

Bitwise operators require integer operands; using one with a float operand is a compile error.
There is no shift operator (`<<`/`>>`) in this version — `>>` is already used for
ownership-transfer syntax (`->>`, `[n]@![]T`; see [Arrays](#18-arrays)), and reusing it for a
shift would conflict with that grammar.

Comparison operators produce `int32` (1 if true, 0 if false). Both operands are converted to a
common type first — see [Usual Arithmetic Conversions](#3-type-system) in Type System.

Logical operators `&&` and `||` use **short-circuit evaluation**: the right operand is not
evaluated if the result is already determined by the left operand. Both operands must be
integer types (float operands are a compile error). The result is always `int32` (1 if true,
0 if false).

The assignment expression `expr -> var` evaluates `expr`, stores it in `var`, and the result is the stored value.

---

## 6. Statements

```palan
int64 x = 10;              // variable declaration
x + 1 -> x;               // assignment statement
printf("%ld\n", x);        // expression statement (function call)
return;                    // return from function (no value)
return expr;               // return with single value
(int64 a, b) = foo();      // tapple declaration (receive multiple return values)
import "lib.pa";           // import Palan source file (see §12)
if expr { ... }            // conditional (see §10)
if expr { ... } else { ... }  // conditional with else (see §10)
```

---

## 7. Function Definitions

Functions are defined with the `func` keyword. Return values are declared after `->`.
The optional `export` keyword makes the function callable from other Palan files that import this file (see §12).

### No Return Value

```palan
cinclude <stdio.h>;

func greet() {
    printf("hello\n");
}
```

### Single Return Value (unnamed)

```palan
cinclude <stdio.h>;

func add(int32 a, int32 b) -> int32 {
    return a + b;
}

printf("%d\n", add(3, 4));
```

### Single Named Return Value

Named return variables are pre-declared and can be assigned via `->`. An implicit `return` at the end of the function is valid.

```palan
func passthrough(int32 x) -> int32 y {
    x -> y;
    return;
}
```

### Multiple Named Return Values

```palan
func sumsOf(int64 a, int64 b, int64 c) -> int64 ab, int64 bc {
    a + b -> ab;
    b + c -> bc;
    return;
}
```

---

## 8. Receiving Multiple Return Values

A **tapple declaration** `(type name, ...) = call(...)` receives multiple return values into newly declared variables in one statement:

```palan
(int64 ab, int64 bc) = sumsOf(1, 2, 3);
printf("%ld %ld\n", ab, bc);
```

**Type inheritance** applies here as well — variables after the first can omit the type:

```palan
(int64 ab, bc) = sumsOf(1, 2, 3);   // bc is also int64
```

---

## 9. C Library Integration

```palan
cinclude <stdio.h>;          // system header — functions visible as unqualified calls
cinclude "myheader.h";       // local header
printf("%ld\n", x);          // call C function directly

cinclude <stdio.h> as S;     // alias — functions accessible only as S.xxx()
S.printf("%d\n", 42);        // qualified call

cinclude <math.h> link "m";  // link against libm (-lm) when building
```

### Function Calls

- `cinclude` makes C functions visible from the declaration point to the end of the enclosing scope.
- With an alias, functions are accessible only via the qualified form `alias.funcName(...)`.

### Link Libraries

- Some C headers declare functions that live outside libc, in a separate shared library — `math.h`'s
  `sqrt`/`pow`/`sin`/etc, for instance, are only in libm. A `link` clause on `cinclude` names the
  library to add to the final link step: `cinclude <math.h> link "m";` appends `-lm` to the `ld`
  invocation, the same as passing `-lm` on a C compiler's command line. Palan does not keep a
  built-in header-to-library lookup table — every third-party library must be named explicitly with
  `link`, since a lookup table covering every header a program might ever cinclude would itself need
  ongoing maintenance.
- Multiple libraries in one clause are comma-separated: `cinclude <stdio.h> link "rt", "m";`. The
  `link` clause comes last, after an optional `as` alias.
- A library name may contain only letters, digits, `_`, `.`, `+` and `-`, and may not be empty — it
  is appended directly after `-l` when invoking the linker, so this is checked at compile time
  rather than left to fail at the `ld` step.
- Library names are collected across the whole program, not scoped to the file or block where
  `link` appears: a `link` clause inside a function body or `{ }` block, or one that reaches the
  final binary only through an `import`ed module, still ends up on the final `ld` command line. The
  same library named more than once (in one `link` clause, in different clauses, or across
  different modules) is deduplicated.
- A header may be cincluded with `link` in one place and without it elsewhere in the same program —
  the library is added to the link step regardless of which cincluding site names it.
- `link` is not a reserved word — it is only recognized as introducing this clause immediately after
  a `cinclude` path (and optional `as` alias). `unistd.h`'s `link()` function, for example, remains
  callable as an ordinary C function.

### Typedefs

- A `typedef` a cincluded C header defines is registered as soon as the header is cincluded,
  whether or not any C function or global in that same header references it. For example,
  `cinclude <stdint.h>; int32_t x = 5;` works even though `stdint.h` declares no C functions at
  all — the typedef itself is what gets registered, not something a function signature happens to
  carry past SA. `size_t n = strlen(s);` after `cinclude <string.h>;` works the same way, just less
  visibly, since `string.h` also declares functions.
- Typedefs that resolve to a primitive type are registered as a Palan type alias (see
  [Type Aliases](#20-type-aliases)). Typedefs that resolve to a struct (e.g. `FILE`, `typedef struct
  _IO_FILE FILE;`, including a multi-level chain) are registered and usable the same way as a
  native struct type — see [Incomplete Struct Types](#incomplete-struct-types-opaque-handles) below
  for the common case where the struct's own layout isn't fully known. This includes a struct body
  with no tag of its own (`typedef struct { int quot; int rem; } div_t;`) — the typedef's own name
  is used as the struct's tag, so `div_t`/`ldiv_t`/`lldiv_t` (from `stdlib.h`'s `div`/`ldiv`/`lldiv`)
  are usable struct types exactly like a tagged one.
- Typedefs that bottom out in a pointer type (e.g. `timer_t`, `typedef void *timer_t;`) are *not*
  registered as a usable Palan type alias name — only resolved at the C function signatures that
  reference them (e.g. `timer_delete(timer_t)` can still be called). A local variable standing in
  for such a typedef's pointee can be declared directly instead, e.g. `@!void t;` in place of a
  `timer_t` local (see `doc/Issues.md` for the underlying limitation).
- Typedefs that bottom out in a union or enum are not resolved and remain unusable this version.
- If multiple cincluded headers introduce the same typedef name resolving to the *same* underlying
  type, the first registration silently wins. If they resolve to *different* underlying types, this
  is a compile error. Because typedef registration is no longer gated on being referenced by some
  function's signature, this conflict is more likely to actually surface than before.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace imported typedefs — they are
  always registered globally. Only C function calls require the `X.` qualifier.

### Macro Constants

- An object-like `#define` macro whose body folds down to a single compile-time integer value is
  usable by name from Palan the moment the header is cincluded — not as an imported symbol, but
  as a compile-time text substitution: every later reference to the name is replaced with the
  folded literal value, the same way the C preprocessor itself would substitute it. This covers a
  bare integer literal, a pointer-cast of one (e.g. `#define NULL ((void *)0)`), and an
  arithmetic or bitwise expression built from `| & ^ << >> + - * / %` over such forms (e.g.
  `sys/stat.h`'s `S_IRWXU`, defined as `(S_IREAD|S_IWRITE|S_IEXEC)`) — including one that
  references another already-defined macro (`#define S_IFDIR __S_IFDIR`).
- A macro whose body doesn't fold this way — a function-like macro referenced without a call
  (e.g. `S_ISDIR`, which takes an argument), a string literal, a relational/equality/logical/
  ternary expression, or a reference to an identifier that never resolves — is left untouched.
  Referencing such a name from Palan is an ordinary `Undefined function`/`Undefined variable`
  diagnostic, not a compiler crash:

  ```palan
  cinclude <sys/stat.h>;
  stat st;
  stat("/etc", st);
  if (S_ISDIR(st.st_mode)) { ... }   // error: Undefined function 'S_ISDIR'
  ```

  `S_IFMT`/`S_IFDIR` and similar bare constants *do* fold, so the same check can be written
  directly with a bitwise AND (see [Expressions](#5-expressions)):

  ```palan
  if ((st.st_mode & S_IFMT) == S_IFDIR) { ... }   // works
  ```

  ```palan
  cinclude <string.h>;
  if (strchr(s, 'x') == NULL) {
      // not found
  }
  ```

  `NULL` is a generic pointer value: it is compatible with and can be compared (`==`/`!=`)
  against any pointer-typed value, including C function return values, `[]T` array
  pointers, and struct pointer fields.
- A macro constant's own type is always signed — `int32` or `int64`, sized by the value's
  magnitude — never unsigned, regardless of what the resulting value is used for. `S_IFMT` is
  `int32`; when `st.st_mode & S_IFMT` above comes out `uint32`, that is the [usual arithmetic
  conversion](#3-type-system) rule for `&` applying to a `uint32`/`int32` pair, not a property of
  the constant itself. A macro constant's folded type also passes an [argument
  conversion](#5-expressions) unchanged where a narrower plain literal would instead adopt the
  parameter's type — e.g. `sys/stat.h`'s `mkdir(path, S_IRWXU)` binds `S_IRWXU`'s `int32` to the
  `uint32`-typed `mode_t` parameter via same-rank signedness reinterpretation, not by `S_IRWXU`
  being re-typed to `uint32`.
- Substitution is text-order only, not lexically scoped like a cincluded C function or `const`:
  a reference resolves against whichever cincluded header defines the name first, textually
  before that reference, anywhere in the rest of the file — including past the end of the block
  the `cinclude` itself appears in. A reference that textually precedes every cincluding
  `cinclude` is left unresolved (`Undefined function`/`Undefined variable`), even if a later
  `cinclude` in the same file would otherwise define it.
- If multiple cincluded headers define the same name, the first one (by source position) wins;
  later `cinclude`s defining the same name are silently ignored for that name.
- A macro constant silently takes priority over any same-named variable, parameter, or `const` in
  scope at the reference site — the opposite of the const/variable [shadowing
  rule](#21-constant-declarations), and undiagnosed either way.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace macro constants — they fold the
  same regardless of alias. Only C function calls require the `X.` qualifier.
- A macro constant is not visible across an `import`/`export` boundary — it is purely a
  same-file, parse-time substitution. A value built from one (e.g. an `export syscall`'s number,
  see [Raw Syscalls](#23-raw-syscalls)) is already a plain literal by the time it could be
  exported, so the folded *value* does cross that boundary even though the macro *name* never
  does.

### C Global Variables

- A file-scope `extern` object declared in a cincluded header, whose type is a primitive or a
  pointer (e.g. `extern FILE *stdout;`), is captured as a readable Palan variable of the same
  name, visible from the cinclude point to the end of the enclosing scope — the same rule as a
  cincluded function. `static` declarations, block-scope declarations, and arrays or by-value
  struct/union/enum globals are not captured.

  ```palan
  cinclude <stdio.h>;
  fprintf(stderr, "starting up\n");   // stderr resolves to the C global
  ```

- **Read-only from Palan**: assigning to a C global (`x -> stdout`) is a compile error, and so is
  taking its address (`@stdout`) or reaching a field through it. This is a restriction on the
  variable binding itself, not on what it points to — the *pointee* keeps its own C-declared
  mutability, so `fwrite(data, 1, n, stdout)` (writing through the `FILE *` value `stdout` holds)
  works normally.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace C globals either — like typedefs
  and constants, they are always registered globally; only a function call needs the `X.`
  qualifier.

### Struct Types

- A C `struct Name { ... }` defined (with a full field list, not just forward-declared) in a
  cincluded header becomes a usable [struct type](#19-struct-types) under that same name — field
  access, embedded struct-typed fields, and struct-typed function parameters all work identically
  to a struct declared natively with `type Name { ... }`. A tag that is only forward-declared, or
  whose full definition can't be captured, is still usable under its name — see [Incomplete
  Struct Types](#incomplete-struct-types-opaque-handles) below.
- A struct pointer *returned* by a C function should be bound to a non-owning `@T`/`@!T`-typed
  local variable, not a named return — the pointer may be owned by C (e.g. a static internal
  buffer), so no automatic freeing is registered for it:

  ```palan
  cinclude <time.h>;
  cinclude <stdio.h>;

  time_t t = int64(0);
  @!tm p = gmtime(@t);                        // non-owning: gmtime owns the storage, not Palan
  printf("%d\n", int32(p.tm_year) + 1900);    // 1970
  ```

  A struct passed *into* a C call follows the same convention as a native struct-typed function
  parameter: it is passed by pointer, borrowed by the callee, and not freed by it (see
  [Struct types in function signatures](#struct-types-in-function-signatures)).

### Structs Returned By Value

A C function that returns a struct **by value** (not by pointer) can be received directly into a
struct-typed local variable's initializer, the same syntax as a native call:

```palan
cinclude <stdlib.h>;
cinclude <stdio.h>;

div_t d = div(7, 2);
printf("%d %d\n", d.quot, d.rem);   // 3 1
```

- The variable's own storage is what the C call writes into — this is an ordinary owning struct
  local, freed automatically at scope exit like any other, not a `@T`/`@!T`-typed handle to
  something C owns (contrast with the pointer-returning case above).
- This is implemented as a general System V AMD64 return-value classification, not a
  `div_t`-specific special case: a struct of 16 bytes or less comes back in registers (`%rax`/
  `%rdx` for integer-classified eightbytes, `%xmm0`/`%xmm1` for floating-point ones, mixed if the
  struct has both), and a struct larger than 16 bytes comes back through a hidden destination
  pointer, the same ABI a C caller would use.
- Writing a call that returns a struct by value anywhere other than a struct variable's
  initializer — as a bare statement, discarding the result — is a compile error: the call still
  has to happen, but there is nowhere to write the result, so the compiler rejects it instead of
  silently dropping the call.
- Not supported: a struct whose size classifies into an eightbyte of fractional width other than
  1, 2, 4, or 8 bytes (e.g. a 3-byte struct) is diagnosed rather than miscompiled. Passing a
  struct *to* a C function by value (the reverse direction, e.g. `stdio.h`'s `fopencookie`) is
  also not supported — the C function itself is rejected as having an unsupported signature, so
  the mismatch is caught at the `cinclude` boundary rather than at the call site. Native Palan
  functions do not gain a struct-by-value return syntax from this — this feature is about
  receiving a C function's by-value return, not a Palan-side language addition.

### Passing a Palan Function as a Callback

A bare Palan function name, written in a C function's argument position, is passed as that
function's address — the common C idiom of handing a callback to a library function like
`qsort`:

```palan
cinclude <stdlib.h>;
cinclude <stdio.h>;

func cmp(@int32 a, @int32 b) -> int32 { return a[0] - b[0]; }

[4]int32 arr;
5 -> arr[0];  3 -> arr[1];  1 -> arr[2];  4 -> arr[3];
qsort(arr, uint64(4), uint64(4), cmp);
printf("%d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);   // 1 3 4 5
```

- This is narrowly scoped to *passing* a function name where a C parameter expects a function
  pointer. There is no first-class function-pointer type, no function-pointer variable, and no
  way to call through one from Palan code — a bare function name written anywhere other than a C
  callback argument position is an ordinary `Undefined variable` error, not an address.
- The Palan function's signature must match the C parameter's function-pointer signature exactly:
  the same number of parameters, each Palan parameter type identical to the corresponding C
  parameter type (no implicit widening or narrowing — there is no place to insert a conversion,
  since the C library calls the Palan function directly), matching pointer mutability (a `const`
  C parameter accepts a Palan `@T`/`@void`; a non-`const` one accepts `@T` or `@!T`), and a
  matching return type, including a `void` C parameter type requiring a `void` Palan return and
  vice versa. A Palan function with multiple named returns can't be used as a callback. Any
  mismatch is a compile error naming both functions.
- `@void`/`@!void` — a pointer whose pointee type is `void` — exists to write the callback
  signatures C itself uses generically, such as `qsort`'s and `bsearch`'s `const void *`
  comparator arguments:

  ```palan
  cinclude <stdlib.h>;
  cinclude <stdio.h>;

  func cmp(@void a, @void b) -> int32 {
      @int32 x = a;   // give the void pointer a concrete type before reading through it
      @int32 y = b;
      return x[0] - y[0];
  }

  [4]int32 arr;
  1 -> arr[0];  3 -> arr[1];  4 -> arr[2];  5 -> arr[3];
  int32 key = 4;
  @!int32 found = bsearch(@key, arr, uint64(4), uint64(4), cmp);
  printf("%d\n", found[0]);   // 4

  int32 miss = 2;
  @!int32 none = bsearch(@miss, arr, uint64(4), uint64(4), cmp);
  printf("%d\n", none == NULL);   // 1 -- not found
  ```

  `void` can only appear as a pointer's pointee (`@void`/`@!void`) — a bare `void x;` variable is
  still rejected. A `@void`/`@!void` value is compatible with any other pointer type in both
  directions, so assigning it to a concretely-typed pointer variable (as `@int32 x = a;` does
  above) gives it a type to read or write through. Indexing or dereferencing a void pointer
  directly (`a[0]`) without first assigning it a concrete pointer type is a compile error.
- A function registered with `atexit`/`at_quick_exit` must not read or write any of the calling
  scope's Palan-owned local variables: program exit frees all still-live owned locals before
  invoking the registered handlers, so by the time the handler runs, that storage is gone.

  ```palan
  cinclude <stdlib.h>;
  cinclude <stdio.h>;

  func say_bye()  { printf("bye\n"); }
  func say_last() { printf("last\n"); }

  atexit(say_bye);
  atexit(say_last);
  printf("hello\n");
  // output: hello / last / bye -- exit handlers run in LIFO order, same as C's atexit
  ```

### C Array Fields

- A fixed-size C array field (`char name[16];`) becomes a fixed-size [array field](#array-fields)
  on the Palan side, laid out inline in the struct exactly like a native `[16]int8 name;` field —
  element access and taking the address of an element both work. A C function parameter declared
  as an array (`void take(char buf[32])`) decays to a plain pointer, same as in C — only the
  outermost dimension decays, so `char buf[2][3]` becomes a pointer to a 3-element row. A C
  struct field with two or more array dimensions (`int cells[2][3];`) is not supported this
  version — the field is left with no known layout, which downgrades the whole struct to an
  [incomplete struct type](#incomplete-struct-types-opaque-handles) rather than making just that
  field unusable.

### Incomplete Struct Types (Opaque Handles)

A struct tag whose full layout isn't known to Palan — either because the header only
forward-declares it, or because one of its fields uses a shape [C Array Fields](#c-array-fields)
above doesn't support — is registered as an **incomplete struct**: its name resolves and it can
be used, but only through a pointer (`@T`/`@!T`), the same restriction C itself places on an
incomplete type. `FILE` (from `stdio.h`) is the running example: its underlying `struct _IO_FILE`
has a field c2ast can't size, so `FILE` is incomplete, and `fopen`/`fprintf`/etc. all work with it
purely as an opaque handle:

```palan
cinclude <stdio.h>;
@!FILE f = fopen("/tmp/out.txt", "w");
fputs("hello\n", f);
fclose(f);
```

`@!FILE f = fopen(...)` is **not** automatically freed at scope exit — unlike an owning struct
pointer variable, the pointee here belongs to C (closed by `fclose`, not Palan's own allocator).

An incomplete struct cannot be used anywhere a full layout is required: a by-value variable
declaration (`FILE f;`), an owned or embedded array of it (`[n]FILE`/`[n]$FILE`), a field access
through it, subscripting an array of it, or embedding it in another struct. Each of these is a
compile error naming the struct and the reason (forward-declared in the header, or a field type
this version can't represent) — never a compiler crash.

---

## 10. If / If-Else Statements

An `if` statement conditionally executes a block. The condition expression must be an integer type; zero is false, non-zero is true.

```palan
if x < 0 {
    printf("negative\n");
}
```

An optional `else` branch executes when the condition is false:

```palan
if x < 0 {
    printf("negative\n");
} else {
    printf("non-negative\n");
}
```

`else if` chains are supported:

```palan
if x < 0 {
    printf("negative\n");
} else if x == 0 {
    printf("zero\n");
} else {
    printf("positive\n");
}
```

- The condition can be any expression that produces an integer value (comparison, variable, call, etc.).
- The `then` and `else` bodies are block statements and create their own scope.

---

## 11. Block Statements

A block `{ ... }` creates a new scope. Variables and functions declared inside are not visible after the closing brace.

```palan
int64 x = 10;
{
    int64 y = 20;      // y visible only inside this block
    printf("%ld\n", y);
}
// y is no longer visible here
printf("%ld\n", x);   // x is still visible
```

Shadowing — declaring a variable or Palan function with the same name as one in an outer scope — is a compile error.

Palan function definitions inside a block are block-scoped and support forward references within the same block.

`cinclude` inside a block makes the imported C functions available only within that block. Shadowing of C function names is allowed.

---

## 12. Modules (import / export)

Palan supports multi-file compilation. Functions declared with `export` are visible to other files that `import` the declaring file.

### Exporting a function

```palan
// lib_add.pa
export func add(int32 a, int32 b) -> int32 {
    return a + b;
}
```

Functions without `export` are file-private and not accessible from other files.

### Importing a file

```palan
// main.pa
cinclude <stdio.h>;
import "lib_add.pa";

printf("%d\n", add(3, 4));   // 7
```

- The path in `import` is relative to the importing file.
- Imported functions are visible from the `import` statement to the end of the enclosing scope.
- Block-scoped `import` (inside `{ }`) makes the imported functions visible only within that block.
- Circular imports (A imports B and B imports A) are supported.

### Selective import

Import only named functions from a file:

```palan
cinclude <stdio.h>;
import square from "lib_math.pa";

printf("%ld\n", square(4));   // 16  (cube is not imported)
```

Multiple names can be listed with commas: `import square, cube from "lib_math.pa"`.

### Alias import

Import all exports under a namespace prefix to avoid name conflicts:

```palan
cinclude <stdio.h>;
import "lib_math.pa" as L;

printf("%ld\n", L.square(3));  // 9
printf("%ld\n", L.cube(2));    // 8
```

- All calls must use the `L.f()` qualified form. Unqualified `square()` is a compile error.

### Selective alias import

Import specific functions under a namespace prefix:

```palan
cinclude <stdio.h>;
import square from "lib_math.pa" as L;

printf("%ld\n", L.square(5));  // 25
```

---

## 13. Program Structure

```palan
cinclude <stdio.h>;

int64 x = 10;
printf("%lld\n", x);
```

- Top-level statements execute as the `_start` entry point.
- User-defined functions can be placed before or after top-level statements.
- `return` is not valid at top-level. To exit early, call `exit()` from `<stdlib.h>`.

---

## 14. While Loops

`while cond { body }` repeats `body` as long as `cond` is non-zero.

```palan
cinclude <stdio.h>;

int64 i = 1;
while i <= 5 {
    printf("%ld\n", i);
    i + 1 -> i;
}
```

The condition is evaluated before each iteration. The loop exits when the condition is zero (false).

```palan
cinclude <stdio.h>;

func fizzbuzz(int64 n) {
    int64 i = 1;
    while i <= n {
        if i % 15 == 0 { printf("FizzBuzz\n"); }
        else if i % 3 == 0 { printf("Fizz\n"); }
        else if i % 5 == 0 { printf("Buzz\n"); }
        else { printf("%ld\n", i); }
        i + 1 -> i;
    }
}

fizzbuzz(20);
```

---

## 15. break / continue

`break` exits the innermost `while` loop immediately. `continue` skips the rest of the loop body and jumps to the next iteration's condition check. Both are only valid inside a `while` loop; using them outside a loop is a compile error.

```palan
cinclude <stdio.h>;

func print_primes(int64 limit) {
    int64 n = 2;
    while n <= limit {
        int64 i = 2;
        int64 is_prime = 1;
        while i * i <= n {
            if n % i == 0 {
                0 -> is_prime;
                break
            }
            i + 1 -> i;
        }
        if is_prime == 1 { printf("%ld\n", n); }
        n + 1 -> n;
    }
}

func print_nonmult3(int64 limit) {
    int64 i = 0;
    while i < limit {
        i + 1 -> i;
        if i % 3 == 0 { continue }
        printf("%ld\n", i);
    }
}

print_primes(20);
printf("---\n");
print_nonmult3(10);
```

---

## 16. Optional Semicolons

The semicolon at the end of the last statement in a block (or at top level) may be omitted. Statements ending with `}` (`if`, `while`, standalone block) never require a trailing semicolon. Other statements require a semicolon when followed by another statement.

```palan
int64 x = 5;
int64 y = x + 1     // semicolon optional here (last statement)
```

```palan
cinclude <stdio.h>;
int64 i = 0;
while i < 3 {
    i + 1 -> i;
    printf("%ld\n", i)   // semicolon optional at end of block body
}
```

## 17. Floating-Point Types

Palan supports 32-bit and 64-bit IEEE 754 floating-point types:

| Type    | Width  | C equivalent |
|---------|--------|--------------|
| `flo32` | 32-bit | `float`      |
| `flo64` | 64-bit | `double`     |

### Declaration and Initialization

Float variables are declared like integer variables. The literal format requires digits on both sides of the decimal point.

```palan
flo64 pi = 3.14159;
flo32 half = 0.5;
flo64 f;              // uninitialized
```

Float literals adopt the declared variable's type. A `flo32` variable initialized with `1.5` stores the value as a 32-bit float; a `flo64` variable stores it as a 64-bit double.

Integer literals can also initialize float variables:

```palan
flo64 n = 5;    // stored as 5.0
```

### Arithmetic Operators

`+`, `-`, `*`, `/` work on `flo32` and `flo64` operands and produce a float result.

```palan
flo64 a = 3.0, b = 2.0;
flo64 sum = a + b;    // 5.0
flo64 diff = a - b;   // 1.0
flo64 prod = a * b;   // 6.0
flo64 quot = a / b;   // 1.5
```

The `%` (modulo) operator is **not** supported on float types; using it is a compile error.

### Comparison Operators

All six comparison operators (`<`, `<=`, `>`, `>=`, `==`, `!=`) work on float operands and produce `int32` (1 if true, 0 if false), the same as integer comparisons.

```palan
flo64 x = 1.5;
if x > 1.0 { printf("big\n"); }
```

### Unary Minus

`-expr` negates a float value.

```palan
flo64 v = 3.14;
-v -> v;   // v is now -3.14
```

### Type Promotion

Float operand conversion for binary operators and comparisons follows the same Usual Arithmetic
Conversions rule as integer operands — see [Type System](#3-type-system): the wider float wins
between two floats, and a float always wins over a paired integer regardless of the integer's
width (`flo32 op flo64` → `flo64`; `int op flo32` → `flo32`; `int op flo64` → `flo64`).

### Explicit Cast to Integer

Use `int64(x)` or `int32(x)` to convert a float to an integer (truncation toward zero):

```palan
cinclude <stdio.h>;
flo64 pi = 3.14159;
printf("%ld\n", int64(pi));   // prints 3
```

### Example — Newton's Method

```palan
cinclude <stdio.h>;

flo64 x = 2.0;
flo64 guess = 1.0;
flo64 eps = 0.000001;
flo64 diff = guess * guess - x;
if diff < 0.0 { -diff -> diff; }
while diff > eps {
    (guess + x / guess) / 2.0 -> guess;
    guess * guess - x -> diff;
    if diff < 0.0 { -diff -> diff; }
}
printf("sqrt(2) = %f\n", guess);
```

Expected output: `sqrt(2) = 1.414214`

---

## 18. Arrays

A 1D array is declared with `[size-expr]type name`. The element count is given as an integer expression before the type.

```palan
[64]uint8 buf;
```

### Heap Allocation and Automatic Cleanup

Array variables are heap-allocated (via `malloc`) when the declaration is reached. At the end of the enclosing scope, the array is automatically freed (via `free`). No manual memory management is required.

```palan
cinclude <stdio.h>;

{
    [64]uint8 buf;            // malloc(64) called here
    sprintf(buf, "hello\n");
    printf("%s", buf);
}                             // free(buf) called here automatically
```

### Passing to C Functions

An array variable is passed to C functions as a pointer to its first element, matching the C `uint8 *` / `char *` convention.

```palan
cinclude <stdio.h>;

[64]uint8 buf;
sprintf(buf, "Hello, array! %d\n", 2025);
printf("%s", buf);
```

Expected output:
```
Hello, array! 2025
```

### Element Access

Individual elements are read with `arr[i]` and written with `val -> arr[i]`. The index must be an integer type.

```palan
cinclude <stdio.h>;

[10]int64 fib;
1 -> fib[0];
1 -> fib[1];
int64 i = 2;
while i < 10 {
    fib[i-1] + fib[i-2] -> fib[i];
    i + 1 -> i;
}
0 -> i;
while i < 10 {
    printf("%lld\n", fib[i]);
    i + 1 -> i;
}
```

Expected output:
```
1
1
2
3
5
8
13
21
34
55
```

### Unsized Array Types in Function Signatures

`[]T` and `[][]T` can be used as parameter types and return types in function declarations.
The semantic analyzer resolves them to plain pointer types with no ownership tracking —
`[]T` becomes a pointer to `T`, and `[][]T` becomes a pointer to a pointer to `T`. The caller
is responsible for managing the lifetime of the returned pointer.

```palan
func sum_arr([]int32 a, int64 n) -> int64 {
    int64 s = 0;
    int64 i = 0;
    while i < n {
        s + a[i] -> s;
        i + 1 -> i;
    }
    return s;
}
```

Using `[]T` in a variable declaration is a compile error.

### Array of Pointer Slots (`[n]@![]T`)

`[n]@![]T` declares an array of `n` writable pointer slots, each capable of holding a `[]T`
pointer. The outer array is heap-allocated (`malloc(n * 8)`) and automatically freed at scope
exit. The inner arrays stored in each slot must be freed explicitly or transferred via `->>`.

```palan
int64 rows = 4;
[rows]@![]int32 ptrs;   // malloc(rows * 8) — outer array
// ... store inner arrays into ptrs[i] ...
// free(ptrs) emitted automatically at scope exit
```

### Ownership Transfer (`->>`)

`val ->> arr[i]` transfers ownership of `val` into the array slot `arr[i]`. The semantic
analyzer emits a null assignment (`NULL -> val`) immediately after the store, so that the
automatic `free(val)` at scope exit becomes `free(NULL)` — a no-op by C standard.

```palan
int64 n = 3;
[n]int32 inner;          // inner: owned, will be freed automatically
int64 m = 2;
[m]@![]int32 outer;      // outer: owns the slot array

inner ->> outer[0];      // transfers inner into outer[0]; inner is set to NULL
// free(inner) at scope exit → free(NULL) = no-op
// free(outer) at scope exit frees the slot array (inner arrays must be freed separately)
```

`return` on a tracked array variable also transfers ownership: the variable is removed from
free-tracking and the caller receives the pointer.

### Two-Dimensional Arrays (`[m][n]T`)

`[m][n]T` declares a two-dimensional array with `m` rows and `n` columns, where `T` must be a
primitive type. The outer array is heap-allocated; each row is independently heap-allocated by
the auto-generated allocator.

```palan
int64 rows = 2;
int64 cols = 3;
[rows][cols]int32 mat;
```

**Element access:**
- Read:  `mat[i][j]`
- Write: `val -> mat[i][j]`

Both row and column indices must be integer types.

**Memory management:** The compiler automatically generates `__pln_alloc_arr_arr_<leaf>` and
`__pln_free_arr_arr_<leaf>` functions (via build-mgr) and inserts the allocation call at
declaration and the free call at scope exit. No manual memory management is required.

### Contiguous Two-Dimensional Arrays (`[n]$[m]T`)

`[n]$[m]T` declares a contiguous 2D array where all `n × m` elements occupy a single
heap-allocated block (`malloc(n * m * sizeof(T))`). Unlike `[n][n]T`, only one allocation and
one free are needed. The inner dimension `m` can be a compile-time constant or a runtime
variable expression.

```palan
int64 rows = 3;
[rows]$[4]int32 mat;   // malloc(rows * 4 * sizeof(int32)) = malloc(rows * 16)
```

**Element access:**
- Read:  `mat[i][j]`
- Write: `val -> mat[i][j]`

Both indices must be integer types.

**Row access:** `mat[i]` yields a transient `[]T` pointer to the start of row `i`. This
pointer is non-owning and must not be freed.

```palan
func row_sum([]$[4]int32 mat, int64 r) -> int32 {
    return mat[r][0] + mat[r][1] + mat[r][2] + mat[r][3];
}
```

**Function parameters:** Declare as `[]$[m]T` with a fixed inner dimension `m`. The compiler
rejects calls where the argument's inner dimension is variable or does not match `m`.

**Memory management:** A single `malloc` is called at declaration and a single `free` is
inserted at scope exit. No helper functions are generated.

### Struct Arrays

Palan supports four forms of struct array declarations. All are heap-allocated and automatically freed at scope exit.

| Form | Memory | Element type |
|---|---|---|
| `[n]$T pts` | `malloc(n * T.totalSize)` / `free(pts)` | contiguous inline elements |
| `[n]T pts` | `__pln_alloc_arr_T(n)` / `__pln_free_arr_T(pts, n)` | owned pointers, each element allocated separately |
| `[n]@T pts` | `malloc(n * 8)`, null-initialized / `free(pts)` | non-owning read-only pointers |
| `[n]@!T pts` | `malloc(n * 8)`, null-initialized / `free(pts)` | non-owning mutable pointers |

`pts[i]` yields a `T` pointer for all four forms. Fields are accessed with `pts[i].field`.

```palan
cinclude <stdio.h>;
type Point { int64 x; int64 y; };

// [n]$T — contiguous inline struct array
[3]$Point pts;
10 -> pts[0].x;  20 -> pts[0].y;
printf("%ld %ld\n", pts[0].x, pts[0].y);  // 10 20

// [n]T — owned pointer array (each element separately allocated)
[2]Point pts2;
5 -> pts2[0].x;
printf("%ld\n", pts2[0].x);  // 5

// [n]@T / [n]@!T — non-owning pointer arrays
Point p;
99 -> p.x;
[4]@Point rpts;    // read-only slots
p -> rpts[0];
printf("%ld\n", rpts[0].x);   // 99

[4]@!Point wpts;   // writable slots
p -> wpts[0];
42 -> wpts[0].x;
printf("%ld\n", p.x);         // 42 (write-through via pointer)
```

**Restriction:** `[n]$T` requires that `T` has no owned sub-struct fields. Use `[n]T` instead when `T` contains owned pointer fields.

Array **fields** (declared inside `type { ... }`, see Section 19) use the same four forms but
require `n` to be a compile-time integer literal, since struct layout must be statically known.
Array **variables** (this section) allow non-constant `n`.

### Limitations (current version)

- Top-level (global) array variables are not freed at scope exit (the OS reclaims memory at process exit).
- Boundary checking is not performed.
- `[n]$T` is not supported when `T` has owned sub-struct fields.

## 19. Struct Types

Define a struct type with `type`:

```palan
type Point { int64 x; int64 y; };
```

Declare a variable, assign fields, and read fields:

```palan
Point p;
10 -> p.x;
20 -> p.y;
printf("%ld %ld\n", p.x, p.y);   // 10 20
```

- **Memory management**: heap-allocated (zero-initialized); automatically freed at scope exit.
- **Layout**: C ABI-compatible (natural alignment, padding, total size rounded up to max-field alignment). Matches System V AMD64 ABI struct layout.

### Field types

Integer and float primitives (`int8`–`int64`, `uint8`–`uint64`, `flo32`, `flo64`) are declared without a prefix:

```palan
type Point { int64 x; int64 y; };
```

Struct-type fields are written with a prefix that controls ownership and memory layout:

| Syntax     | Meaning                              | Memory          |
|------------|--------------------------------------|-----------------|
| `$T field` | Inline embedding — T's bytes are part of the parent struct's allocation | parent calloc   |
| `T field`  | Owned pointer — 8-byte pointer; T is auto-allocated/freed with parent | `__pln_alloc_T` |
| `@T field` | Non-owning read-only pointer — 8-byte null pointer; lifecycle is user-managed; pointer value may be set but field write-through is not allowed | none            |
| `@!T field`| Non-owning mutable pointer — same as `@T` but field write-through is also allowed | none            |

`@T`/`@!T` are general pointer-type expressions, not exclusive to struct fields — they may
also be used as a plain local variable's declared type, as a non-owning alias into existing
storage (no allocation, no automatic freeing at scope exit). Field access on such a variable
works the same as on a struct-field pointer of the same kind:

```palan
type Point { int64 x; int64 y; };

Point original;
5 -> original.x;  10 -> original.y;

@!Point view = original;   // non-owning alias — view and original share the same storage
20 -> view.x;               // write-through
printf("%ld %ld\n", original.x, original.y);   // 20 10
```

### Array fields

| Syntax        | Meaning                                  | Memory                          |
|---------------|-------------------------------------------|----------------------------------|
| `[n]$T field` | Embedded contiguous array                 | parent's block                   |
| `[n]T field`  | Owned pointer array                       | cascaded alloc/free with parent  |
| `[n]@T field` | Non-owning read-only pointer-slot array   | parent's block (slots only)      |
| `[n]@!T field`| Non-owning mutable pointer-slot array     | parent's block (slots only)      |

`T` may be a primitive type or a struct name. `n` must be a compile-time integer literal.
`field[i]` accesses an element; for struct-leaf forms, `field[i].sub` continues the field
chain.

```palan
type Point { int64 x; int64 y; };

// [n]$T -- embedded array field (struct leaf)
type Polygon { [4]$Point pts; };
Polygon poly;
10 -> poly.pts[0].x;  20 -> poly.pts[0].y;
printf("%ld %ld\n", poly.pts[0].x, poly.pts[0].y);  // 10 20

// [n]T -- owned pointer array field (struct leaf, cascades with parent's alloc/free)
type Cluster { [4]Point pts; };
Cluster c;
5 -> c.pts[0].x;
printf("%ld\n", c.pts[0].x);  // 5

// [n]@T / [n]@!T -- non-owning pointer-slot array field
type Ring { [4]@!Point nodes; };
Point p;  99 -> p.x;
Ring r;
p -> r.nodes[0];
printf("%ld\n", r.nodes[0].x);   // 99
42 -> r.nodes[0].x;              // write-through (mutable only)
printf("%ld\n", p.x);            // 42
```

**`$T` — inline embedding**

```palan
type Point { int64 x; int64 y; };
type Line  { $Point a; $Point b; };

Line l;
10 -> l.a.x;  20 -> l.a.y;
printf("%ld %ld\n", l.a.x, l.a.y);   // 10 20
```

`$Point` fields are stored directly inside `Line`'s memory block. No separate allocation.

**`T` — owned struct pointer**

```palan
type Point { int64 x; int64 y; };
type Rect  { Point tl; Point br; };

Rect r;
10 -> r.tl.x;  20 -> r.tl.y;
printf("%ld %ld\n", r.tl.x, r.tl.y);   // 10 20
```

`r` and its `tl`/`br` sub-structs are all automatically freed at scope exit.

**`@T` — non-owning read-only pointer**

```palan
type Node { int64 val; @Node next; };

Node n1;  Node n2;
42 -> n1.val;  100 -> n2.val;
n2 -> n1.next;                          // set pointer value: OK
printf("%ld %ld\n", n1.val, n1.next.val);   // 42 100
```

`@T` allows reading through the pointer but not writing. Use `@!T` for write-through:

```palan
type Node { int64 val; @!Node next; };

Node n1;  Node n2;
n2 -> n1.next;
42 -> n1.next.val;                      // write through mutable pointer: OK
printf("%ld\n", n2.val);               // 42
```

### Struct types in function signatures

A struct-type parameter is passed as a pointer (borrowed, not freed by the callee):

```palan
func getX(Point p) -> int64 x {
    p.x -> x;
}
```

A named return of struct type transfers ownership to the caller:

```palan
func makePoint(int64 x, int64 y) -> Point p {
    Point p;
    x -> p.x;  y -> p.y;
}
```

### Restrictions

- Nested/2D array fields (`[n]$[m]T field`, etc.) are not supported.
- Recursive embedding (`type A { $A a; }`) is a compile error.
- An [incomplete struct type](#incomplete-struct-types-opaque-handles) — a tag whose layout
  isn't known, e.g. a cincluded `FILE` — cannot be used as a by-value variable (`FILE f;`), an
  owned or embedded array (`[n]FILE`/`[n]$FILE`), an embedded field (`$FILE field;`), or an
  array-of-struct element (`arr[i]`/`p[i]` subscripting). It is usable only through a pointer
  (`@T`/`@!T`) — as a local variable, function parameter, or return type.

---

## 20. Type Aliases

Declare an alias for a primitive type with `type`:

```palan
type MyInt = int64;

MyInt x = 42;

func addOne(MyInt n) -> MyInt result {
    n + 1 -> result;
}
```

- The alias may be used in variable declarations and function signatures.
- Only aliasing a primitive type is supported this version.

### Restrictions

- Alias use inside array element types (`[n]MyInt`) is not supported.
- Alias use inside struct field types is not supported.
- The alias target being a struct type is not supported — the target must resolve to a primitive.
- Unlike primitive types' `int64(x)` cast syntax, there is no constructor-cast syntax `MyAlias(x)`.

---

## 21. Constant Declarations

Declare a compile-time constant with `const`:

```palan
const MaxLen = 256;

[MaxLen]uint8 buf;
printf("%ld\n", MaxLen);   // 256
```

- Only a compile-time literal value is accepted — `lit-int`, `lit-uint`, `lit-flo`, or `lit-str` —
  not an arbitrary compile-time-constant expression (e.g. `const X = 1 + 2;` is not supported).
  This is unrelated to the C macro-constant folding described under [C Library
  Integration](#macro-constants) — that folds a *C preprocessor* macro body during header
  ingestion, using C's own integer semantics; it does not extend what a Palan `const`
  declaration itself accepts.
- Every reference to the constant is inlined with the literal value; the constant's name does not
  appear in `sa.json`.
- A const may reference another const declared earlier (`const B = A;`); this chains naturally
  through inlining.

### Restrictions

- There is no name-collision check between a const and a variable — a variable declaration of the
  same name silently shadows a same-named const.
- A const cannot be used at a point where a function signature is pre-registered, e.g. as an
  array-size in a parameter type. It is only usable from ordinary statement processing onward.

---

## 22. Address-Of Operator

Take the address of a local primitive-typed variable with `@` (read-only) or `@!` (mutable):

```palan
int64 x = 42;
@!int64 p = @!x;   // p now holds the address of x
```

- `@ID` yields a read-only pointer to `ID`'s storage; `@!ID` yields a mutable pointer. The
  compiler enforces this: writing through a `@ID` pointer, or assigning/passing one where a
  `@!`-typed (mutable) destination is expected, is a compile error.
- `ID` may itself already be a pointer to a primitive (`@T`/`@!T`); `@`/`@!` on it then yields a
  pointer to that pointer's own storage slot, matching the C `T **` out-param idiom used by
  functions like `strtol`:

  ```palan
  cinclude <stdlib.h>;

  @!int8 end;
  int64 v = strtol("42abc", @!end, 10);   // strtol writes end's own storage: end now points at "abc"
  printf("%ld %s\n", v, end);             // 42 abc
  ```
- `@` also takes the address of a primitive-typed struct field reached from a local variable
  (`@s.x`, including through a chain of fields — `@!s.inner.x` — and through pointer-typed
  fields — `@!p.next.val`):

  ```palan
  type Point { int64 x; int64 y; };
  Point s;
  memcpy(@!s.x, @src, 8);   // out-param write into s.x, same as memcpy(@!x, ...) on a plain variable
  ```

  The same read-only/mutable rule applies at every step of the chain: `@!` on a field reached
  through a read-only pointer (a `@T`-typed base variable, or a `@T`-typed pointer field along
  the way) is a compile error, not silently downgraded to a read-only address.
- `@` also takes the address of an embedded-struct field (`$T`) reached from a local variable
  (`@!s.inner`, C's `&st.st_atim` idiom) — the result is a plain pointer to the inner struct
  (`@!Inner`), the same shape a struct-typed local variable already has, not a pointer to a
  pointer:

  ```palan
  cinclude <sys/stat.h>;

  stat st;
  stat("/", st);
  @!timespec atim = @!st.st_atim;   // pointer to the embedded timespec field
  printf("%ld\n", atim.tv_sec);
  ```
- `@` also takes the address of a primitive-typed array element (`@arr[2]`, `@!arr[2]`),
  including an element of a fixed-size array field (`@!s.data[2]`) and the innermost element of a
  multi-dimensional array (`@!mat[0][1]`):

  ```palan
  [4]int64 arr;
  memcpy(@!arr[2], @src, 8);   // out-param write into arr[2]
  ```

  The same write-permission rule applies: `@!arr[i]` requires write permission on `arr` itself, so
  taking a mutable address through a read-only array (or a read-only array field reached through a
  `@T`-typed pointer) is a compile error.
- The resulting pointer can be handed to a cincluded C function that expects a pointer parameter
  — as an input the function reads through (`const T*`), or as an out-param it writes into:

  ```palan
  cinclude <time.h>;
  cinclude <stdio.h>;

  time_t t = int64(0);
  printf("%s", ctime(@t));                      // read-only: ctime takes const time_t*

  int32 clk_id = 0;
  @!int32 p = @!clk_id;
  int32 rc = clock_getcpuclockid(int32(0), p);   // mutable out-param
  printf("%d\n", rc == int32(0));
  printf("%d\n", p[0]);                          // read the C-written value back — see below
  ```

  The read-only/mutable rule extends to C parameters too: a `@T` (read-only) value may only be
  passed where the C parameter is `const`-qualified (as `ctime`'s `const time_t*` is above);
  passing `@T` to a non-`const` parameter — an out-param like `clock_getcpuclockid`'s second
  argument — is a compile error, same as passing `@T` where a Palan function expects `@!T`.
- It can also be dereferenced from Palan code itself, using `p[i]` — subscript notation, not a
  separate operator, since Palan already represents an array as a pointer plus attributes and a
  plain pointer as a bare pointer. `p[0]` reads or writes the pointee; `@T` (read-only) allows only
  the read, `@!T` (mutable) allows both — the same rule as writing a struct field through a
  pointer. `p[i]` for `i != 0` is ordinary C-style pointer arithmetic (the element at `i` element-
  widths past `p`) with no bounds check — as in C, staying within the bounds of what `p` actually
  points to is the programmer's responsibility, not something the compiler verifies.

  ```palan
  int64 x = 42;
  @!int64 p = @!x;
  99 -> p[0];        // writes through p — x is now 99
  int64 y = p[0];     // reads through p — y is 99
  ```

  When the pointee is itself a struct (`@T`/`@!T` where `T` is a struct type), Palan has no
  register-sized representation of a struct value — every struct-typed expression is already a
  pointer — so `p[i]` computes an address rather than loading a value. Access its fields with
  `p[i].field`, exactly as with `p.field`:

  ```palan
  cinclude <time.h>;

  time_t t = int64(0);
  @!tm p = gmtime(@t);
  1972 -> p[0].tm_year;
  printf("%d\n", p.tm_year);   // p[0] and p name the same pointee — prints 1972
  ```
- The pointer returned by `@`/`@!` is a borrowed reference: taking it never allocates or frees
  anything, and the storage it points into keeps its own ownership and free timing unchanged.
  Nothing checks that the pointer does not outlive that storage — if it escapes the scope that
  owns the storage (e.g. assigned to a variable declared in an outer block), reading through it
  after the storage is freed is undefined behavior, not a compile error. See `doc/Issues.md`
  for a worked example.

### Restrictions

`@`/`@!` produces a pointer to one storage slot: a primitive value, or a pointer to a primitive
(pointer-to-pointer). A struct or array variable is never re-addressed this way, because it's
already its own pointer to its storage — pass it by name instead (`random_r(st, ...)`, not
`random_r(@!st, ...)`); `@!st` would build a meaningless `struct T **`.

- Not usable on function parameters, or on a whole struct or array variable (`@s`, `@arr`) — see
  above.
- On a local variable, usable only when the variable is primitive-typed or itself a pointer to a
  primitive (`@T`/`@!T`) — a struct-typed local, a pointer-to-struct local, or a plain array
  variable (`[n]T`, itself a pointer, see above) are all rejected.
- On a struct field reached from a local variable, usable only when the leaf field is
  primitive-typed or an embedded struct (`$T`) — a pointer-typed field (`@T`/`@!T`), an
  embedded-struct-array element (`[n]$T`), or an owned-pointer field are all rejected.
- Not usable on a 2D array row (`@mat[i]`) or a pointer-slot array element (`[n]@T`/`[n]@!T`) —
  only a primitive-typed array element.
- Not usable on a general expression (a call result, a parenthesized tuple, etc.) — only a local
  variable, a field reached from one, or an array element reached from one.
- A fixed-size array variable (`[n]T`) is, like a struct variable, already a pointer to its own
  storage — but unlike a struct variable it *is* representable as a plain pointer-to-primitive
  local, so `@!arr` compiles: it yields the address of the variable's own pointer slot, not a new
  view into the array's elements. Since the array is freed automatically when its owning scope
  exits, handing that slot to a C function that overwrites it (e.g. an out-param realloc-style
  API) will make the automatic free operate on whatever the C call left behind — get this pattern
  right or avoid it, the compiler does not check it.

---

## 23. Raw Syscalls

A `syscall` declaration binds a Linux x86-64 syscall number to a name, callable afterward like
any other function — bypassing libc entirely:

```palan
syscall sys_write(int32 fd, @void buf, uint64 count) -> int64 = 1;

sys_write(1, "hello, syscall\n", 15u);
```

A cinclude'd macro constant works here too, since it folds to a literal before this point is
checked (see [Macro Constants](#macro-constants)):

```palan
cinclude <sys/syscall.h>;

syscall sys_write(int32 fd, @void buf, uint64 count) -> int64 = SYS_write;
```

- Syntax: `[export] syscall name(parameters) [-> type] = number;`, where `number` must be an
  integer literal (0 to 2^32-1) — a bare literal or a cinclude'd macro constant that folds to
  one; any other expression is not allowed.
- Once declared, the name is called exactly like a normal function, including from other
  functions or across an `import`/`export` boundary (see §12).
- A block-scoped `syscall` declaration is visible only within that block, same as a nested
  `func`.
- At most 6 parameters. The return, if any, must be a single unnamed type (`-> type`); named or
  multiple return values are not allowed.
- Parameter and return types must be representable as a full 8-byte register: `flo32`/`flo64`
  and the 8-bit/16-bit integer types are rejected.
- The return value is the kernel's raw result in `rax`: a negative value conventionally means
  `-errno` (e.g. `-9` for `EBADF`), which the caller must interpret itself — libc's `errno`
  variable is never touched.
- `syscall` is a reserved word, so a cinclude'd `<unistd.h>`'s `syscall()` wrapper function
  cannot be called from a file that uses this feature.

---
