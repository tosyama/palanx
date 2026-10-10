# Palan Language Reference

**Version:** v0.1.40

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

A literal must fit the type it adopts from its context (the declared variable, parameter, or the
other operand); otherwise it is a compile error — `int8 x = 300;` and `uint8 y = -1;` are both
rejected. A minus sign directly on an integer literal is part of the value, so `int8 x = -128;`
is accepted.

**Character literals:** `'a'` is an integer literal whose value is the character's ASCII code, so
it follows the same rules as any other integer literal (`int8 c = 'a';` is accepted). Only
printable ASCII characters and the escapes `\n` `\t` `\r` `\0` `\\` `\'` are allowed; an empty,
multi-character, or non-ASCII literal is a compile error.

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
| `bool`  | 8-bit   | holds 0 or 1 |

### bool

- `true` and `false` are the `bool` literals (reserved words), with the values 1 and 0. They are
  always `bool`, so `int32 x = true;` is a widening and `true + 1` is 2.
- An integer literal converts to `bool` implicitly only when it is `0` or `1`. Any other integer
  or float value needs `bool(x)`, which yields 1 for every nonzero value rather than truncating.
- `bool` widens implicitly to every integer and float type.
- An operator promotes a `bool` operand to `int32`, as C does, so `b + 1` is 2. Storing such a
  result back into a `bool` needs `bool(...)`.
- A `bool` can be an `if`/`while` condition. Comparisons and logical operators yield `bool`.
- A C `_Bool` parameter or field (e.g. ncurses' `keypad`) is a `bool`.

### enum

```palan
type Color enum {
    RED,          // 0
    GREEN = 5,
    BLUE          // 6
};

Color c = Color.GREEN;
int32 n = c;            // an enum widens implicitly
Color d = Color(n + 1); // an integer needs Name(x)
```

- An enumerator without `=` is the previous value + 1, starting at 0. `=` takes an integer literal
  or a `const`.
- Enumerators are referenced as `Name.X`. A type alias of the enum works the same way
  (`type Shade = Color;` then `Shade.RED`).
- An enum's base type is the type C gives its enumerators: `int32` when every value fits, which
  covers almost every enum. Otherwise it is `uint32` (no negative value, all fit), else `int64` or
  `uint64`. An enum is passed to and from C as its base type.
- An enum is its own type, like `bool`. It widens implicitly to its base type and to anything its
  base type widens to. An integer, a `bool`, or a different enum becomes an enum only through
  `Name(x)`; an integer literal is no exception (`Color c = 5;` is an error).
- An operator computes an enum operand as its base type, so `Color.RED + 1` is an `int32`.
  Storing the result back into a `Color` needs `Color(...)`.

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
unconverted; a comparison's result is always `bool` regardless of operand type.

### Variadic Argument Promotion

When passing to variadic C functions (e.g., `printf`), small integer types are promoted:
- `int8`, `int16` → `int32`
- `uint8`, `uint16` → `uint32`
- `bool` → `int32`

---

## 4. Variable Declarations

```palan
int64 x = 10;
int32 a = 5, b = 10;   // type inheritance: b is also int32
```

**Type inheritance:** In a comma-separated declaration, all variables after the first inherit the type of the first variable.

**Type is required:** `x = 10;` is a declaration without a type, reserved for future type inference,
and is currently a compile error. It is not an assignment — assignment is written `10 -> x;`.

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
| Explicit cast | `type(expr)` | `int32(x)`, `Color(n)` |
| Enumerator | `Name.X` | `Color.RED` |
| Type size | `sizeof(type)` | `sizeof(Vec3f)` |
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

Comparison operators produce `bool` (1 if true, 0 if false). Both operands are converted to a
common type first — see [Usual Arithmetic Conversions](#3-type-system) in Type System.

Logical operators `&&` and `||` use **short-circuit evaluation**: the right operand is not
evaluated if the result is already determined by the left operand. Both operands must be
integer types (float operands are a compile error). The result is always `bool` (1 if true,
0 if false).

The assignment expression `expr -> var` evaluates `expr`, stores it in `var`, and the result is the stored value.

Assignments chain as a statement: `v -> a -> b;` is `v -> a; a -> b;`, and the same holds
for `->>` (`x ->> s.p ->> t.p;` moves ownership from `x` to `s.p`, then from `s.p` to `t.p`).
A target followed by another `->` is read back as the next value, so its index is evaluated
twice (`v -> arr[f()] -> x` calls `f` twice). Targets are stored left to right: in
`n -> i -> arr[i]`, `arr[i]` uses the `i` just stored. An assignment used inside any other
expression is not supported.

`sizeof(type)` is the byte size of a type, as C's `sizeof` gives it, and is typed like an
unsigned integer literal (`uint64` unless the context is another unsigned type). Its operand is
a type, not an expression. A struct, including a `cinclude`d one, has its C layout size; an
incomplete struct such as `FILE` is an error. An array is the total size of its elements, and
every size must be a compile-time constant. Only a `$` element is stored in the array itself:
a struct or row element without `$` is a pointer, so `sizeof([3]Vec3f)` is `3 * 8` while
`sizeof([3]$Vec3f)` is `3 * sizeof(Vec3f)`.

```palan
jpeg_compress_struct cinfo;
jpeg_CreateCompress(@!cinfo, JPEG_LIB_VERSION, sizeof(jpeg_compress_struct));
```

---

## 6. Statements

```palan
int64 x = 10;              // variable declaration
x + 1 -> x;               // assignment statement
printf("%ld\n", x);        // expression statement (function call)
return;                    // return from function (no value)
return expr;               // return with single value
(int64 a, b) = foo();      // tapple declaration (receive multiple return values)
foo() -> (a, b);           // assign multiple return values to existing variables
import "lib.pa";           // import Palan source file (see §12)
if expr { ... }            // conditional (see §10)
if expr { ... } else { ... }  // conditional with else (see §10)
```

---

## 7. Function Definitions

Functions are defined with the `func` keyword. Return values are declared after `->`.
The optional `export` keyword makes the function callable from other Palan files that import this file (see §12).
Parameters and return values may be float types as well as integer types (see [Floating-Point Types](#17-floating-point-types)).

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

A named return starts at `0` unless it has an initializer. Initializers are evaluated in order at function entry and can use the parameters:

```palan
func range(int32 from) -> int32 lo = from, int32 hi = from + 10 { }
```

A struct-type named return is declared in the body instead, so it can't have an initializer.

### Method-Form Calls

A function whose first parameter is a writable borrow (`@!T`, `@![n]T`, or a C function's
non-const pointer) can also be called as `x.f(args)`, which is the same as `f(@!x, args)`:

```palan
type Counter { int32 n; };
func add(@!Counter c, int32 d) { c.n + d -> c.n; }

Counter c;
c.add(1);               // add(@!c, 1)

jpeg_compress_struct cinfo;  // cinclude <jpeglib.h>
cinfo.jpeg_set_defaults();  // jpeg_set_defaults(@!cinfo)
```

- `x` must be something `@!` can be applied to: a variable, a struct field, or an array element.
- If `x` is already a borrow (a `@!T` variable, parameter or field), it is passed as it is, so a
  function taking `@!Counter c` can write `c.add(1)`. A read-only `@T` borrow is a compile error.
- A function whose first parameter is not `@!` (a value, `@T`, or no parameters) cannot be called
  this way.
- If `x` is both a variable and a module alias (§12), the variable is used.

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

Each variable is initialized from its return value with the same type rules as a single declaration `int64 ab = ...;` (widening is implicit, narrowing is an error). A returned struct is owned by the variable that receives it.

To store the values into existing variables, array elements or fields instead, assign the call to a parenthesized target list:

```palan
sumsOf(1, 2, 3) -> (ab, arr[1]);
```

All return values are received first, then assigned to the targets from left to right, with the same type rules as a single `->` (a struct is copied into its target). So in `f() -> (i, arr[i])`, `arr[i]` uses the newly assigned `i`.

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
- The number of arguments must match the number of parameters; a variadic C function takes at least
  its fixed parameters. A C function declared with empty parentheses `f()` takes no arguments, the
  same as `f(void)`.

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
- Typedefs that bottom out in a union are usable the same way as struct typedefs (see
  [Union Types](#union-types) below). A typedef of an enum is another name for it (see
  [C Enum Types](#c-enum-types) below).
- A header may use a typedef from a header cincluded before it, as in C where both are included
  into one translation unit: `cinclude <stdio.h>; cinclude <jpeglib.h>;` lets `jpeglib.h`'s
  declarations use `size_t` and `FILE`. Typedefs that bottom out in a pointer type are not carried
  over this way, and neither are macros (see `doc/Issues.md`).
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
  stat("/etc", @!st);
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
- A macro constant whose type C leaves open — a plain number such as `#define COLOR_RED 1` or
  `#define ERR (-1)`, or arithmetic over plain numbers like `S_IRWXU` — behaves exactly like an
  integer literal written in Palan: it takes the type its context expects and must fit in it. So
  `st.st_mode & S_IFMT` is `uint32` because `S_IFMT` takes `st_mode`'s type, and ncurses'
  `init_pair(1, COLOR_RED, COLOR_BLACK)` passes the constants to `short` parameters without a
  cast.
- A macro constant whose type C fixes — an unsigned or long suffix (`1U`, `5L`) or an integer
  cast, as in ncurses' `A_REVERSE` (`unsigned int`) — keeps that type, and meets other operands
  through the [usual arithmetic conversions](#3-type-system) like any typed value.
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

- A file-scope `extern` object declared in a cincluded header, whose type is a primitive, an enum
  or a pointer (e.g. `extern FILE *stdout;`), is captured as a readable Palan variable of the same
  name, visible from the cinclude point to the end of the enclosing scope — the same rule as a
  cincluded function. `static` declarations, block-scope declarations, and arrays or by-value
  struct/union globals are not captured.

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

  A struct passed *into* a C call's struct pointer parameter is borrowed the same way as for a
  native `@T`/`@!T` parameter: write `@s` for a `const` pointer parameter and `@!s` otherwise
  (`mktime(@!t)`, `asctime(@t)`; see
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

### Union Types

A C `union Name { ... }` (or `typedef union { ... } Name;`) from a cincluded header is usable under
its name everywhere a C struct is: a by-value variable declaration, `@`/`@!` address-of, field
read/write, a `$Name` field of a native struct, and passing it to a C function by pointer. All
members start at the same address, so writing one member changes what the others read, as in C;
the size is that of the largest member, rounded up to the largest member alignment.

`pthread.h`'s `pthread_mutex_t`, `pthread_cond_t` and `pthread_attr_t` are unions:

```palan
cinclude <pthread.h>;
cinclude <stdio.h>;

type Shared { int64 count; $pthread_mutex_t m; };

func worker(@!void arg) -> @!void {
    @!Shared sh = arg;
    pthread_mutex_lock(@!sh.m);
    sh.count + 1 -> sh.count;
    pthread_mutex_unlock(@!sh.m);
    return NULL;
}

Shared s;
pthread_mutex_init(@!s.m, NULL);
pthread_t t1;
pthread_t t2;
pthread_create(@!t1, NULL, worker, s);
pthread_create(@!t2, NULL, worker, s);
pthread_join(t1, NULL);
pthread_join(t2, NULL);
pthread_mutex_destroy(@!s.m);
printf("%ld\n", s.count);    // 2
```

`PTHREAD_MUTEX_INITIALIZER` and the other `*_INITIALIZER` macros expand to a brace initializer,
which Palan has no syntax for; call the matching `_init` function instead.

Not supported this version:

- Passing a union by value to a C function — like a struct, calling such a function is a compile
  error.
- A C11 anonymous member with no member name (`struct S { int k; union { int a; float b; }; };`)
  — a header containing one cannot be cincluded. An anonymous body that *does* have a member name
  (`union { ... } u;`) works; its fields are reached through that name.

### C Enum Types

A C enum from a cincluded header is a Palan [enum](#enum): its enumerators are referenced as
`Name.X`, and it follows the same type rules. `Name` is the tag (`enum Tag { ... }`) or the
typedef name (`typedef enum { ... } Name;`); when the enum has both, either name works. It can be
the type of a variable, a struct field, a C function's parameter or return value, and a C global.

```palan
cinclude <jpeglib.h>;

jpeg_compress_struct cinfo;
J_COLOR_SPACE.JCS_RGB -> cinfo.in_color_space;
JDCT_DEFAULT -> cinfo.dct_method;    // #define JDCT_DEFAULT JDCT_ISLOW
int32 cs = cinfo.in_color_space;     // widens to its base type
```

- A macro that is just one enumerator (`#define JDCT_DEFAULT JDCT_ISLOW`, or glibc's
  `#define SOCK_STREAM SOCK_STREAM`) has the enum's type. A macro computing with enumerators
  (`#define X (A + 1)`) is a plain integer constant, like any other [macro constant](#macro-constants).
- The enumerators of an enum with neither a tag nor a typedef name (`enum { A, B };`) are
  referenced without qualification (`A`), as plain integer constants.
- An enum with an enumerator whose value can't be computed at compile time (e.g. `= sizeof(int)`)
  is not available; a function or struct using it is treated as unsupported.

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

Palan supports multi-file compilation. Functions and types declared with `export` are visible to other files that `import` the declaring file.

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
- A variable named `L` hides the alias: `L.f()` is then a method-form call on the variable (see §7).

### Selective alias import

Import specific functions under a namespace prefix:

```palan
cinclude <stdio.h>;
import square from "lib_math.pa" as L;

printf("%ld\n", L.square(5));  // 25
```

### Exporting types

A struct, enum, or type alias declared with `export type` can be imported like a function:

```palan
// vec3.pa
export type vec3 { flo32 x; flo32 y; flo32 z; };
export type Color enum { Red, Green };
export type Row = [3]vec3;
export func len2(@vec3 v) -> flo32 {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}
```

```palan
// main.pa
import "vec3.pa";
vec3 p;
Color c = Color.Green;
flo32 l = len2(@p);
```

- An exported type is visible in the whole importing file, like a type declared in it. A
  type imported inside a block is visible from the `import` on.
- Under an alias import, write the type with the alias: `V.vec3`, `V.Color.Green`,
  `V.Color(1)`. The unqualified name is a compile error.
- A selective import lists types alongside functions: `import vec3, len2 from "vec3.pa";`.
- An exported type may use the declaring file's non-exported types, consts, and cincluded
  headers; those are not made nameable in the importing file.
- A type name must be unique in the program: declaring or importing a type whose name another
  file already defines is a compile error.
- Two files may import each other and use each other's types anywhere, including in type
  declarations, as long as no type contains itself (see [Declaration order](#declaration-order)).

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

All six comparison operators (`<`, `<=`, `>`, `>=`, `==`, `!=`) work on float operands and produce `bool` (1 if true, 0 if false), the same as integer comparisons.

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

### Float Parameters and Return Values

Palan functions take and return `flo32`/`flo64` values like any other type, and may mix them with
integer parameters and return values:

```palan
func scale(flo64 x, int32 n) -> flo64 {
    return x * n;
}
flo64 r = scale(1.5, 4);   // 6.0
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
is responsible for managing the lifetime of the returned pointer. A struct element is laid out
as in a sized array: `[]T` takes the elements of a `[n]T` array, and `[]$T` those of a `[n]$T`
array. `[][m]T` takes the rows of a `[n][m]T` array (and `[][m]$T` those of a `[n][m]$T` struct
array); `m` must be a constant, and an array whose rows have another size is a compile error.
`[]@![]T` takes the slots of a `[n]@![]T` array.

Only the outermost size may be left out: the elements must match exactly. An element owned by
the array (a row, or a struct of `[n]T`) cannot be given as a `@T`/`@!T` pointer slot, or the
other way around, since one side would free or overwrite what the other one owns. For example,
a `[n]@![]int32` array is returned as `[]@![]int32`, not as `[][]int32`, and a `[n]P` array
cannot be passed as `[]@!P`. The same holds for storing an array into a `[n]@![]T` slot.

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

### Borrowing Arrays

`@[n]T` borrows an array read-only and `@![n]T` borrows it writable. Pass the array with
`@arr` / `@!arr`: the callee asks for a borrow, so the caller writes one. Nothing is copied or
freed; the array keeps its owner.

```palan
const H = 3;
const W = 4;

[H][W]int32 grid;
fill(@!grid, 7);
printf("%d\n", sum(@grid));

func sum(@[H][W]int32 g) -> int32 {
    return g[0][0] + g[H-1][W-1];
}

func fill(@![H][W]int32 g, int32 x) {
    x -> g[0][0];
    x -> g[H-1][W-1];
}
```

- The forms are `@[n]T`, `@[n][m]T`, `@[n]$[m]T`, `@[n]$T`, and `@[n][m]$T`, each with the `@!` variant. `T`
  must be a primitive type, or a struct type in `@[n]T` and `@[n][m]T` (an array of owned structs,
  `[n]T` / `[n][m]T`) and in `@[n]$T` / `@[n][m]$T` (contiguous struct arrays, `[n]$T` / `[n][m]$T`). Every size must be a constant: an integer literal or a `const`.
- An array of pointer slots (`[n]@T` / `[n]@!T`) is borrowed as `@[n]@T`, `@![n]@!T`, and so on.
  The borrow's `@`/`@!` controls writing the slots (`@q -> g[i]`); the element's `@T`/`@!T`
  controls writing through a pointer (`1 -> g[i].x`). The element permission must match the
  array's, except that a `@` borrow may take `[n]@!T` as `@[n]@T`.
- The array's shape must match exactly: the number of dimensions, contiguous (`$`) or not, and
  every size. An array whose size is only known at run time cannot be borrowed as `@[n]T`.
- Through `@`, no element can be written, including elements reached through a row
  (`g[i][j]`) and the fields of a struct element (`g[i].x`). A `@` borrow cannot be passed
  where `@!` is expected.
- A struct element is still passed to a `@T`/`@!T` parameter as `@g[i]` / `@!g[i]`.
- A row of a multi-dimensional array is borrowed the same way: `@g[i]` / `@!g[i]` gives the row of
  `[n][m]T`, `[n]$[m]T`, `[n][m]$T` or `[n][m]T` (struct `T`) as `@[m]T`, `@[m]$T` and so on, also
  when `g` is itself a borrowed array.
- A borrowed array can also be a local variable (`@[4]int64 p = @v;`).
- A borrowed array is already a borrow, like a `@T`/`@!T` pointer: pass it on, bind it, and
  rebind it by name (`sum(g)`, `@[4]int64 q = p;`, `q -> p`). `@g` on it is a compile error. A
  `@!` borrow may be given where `@` is expected. A row (`g[i]`) is part of the borrowed storage,
  not a borrow of its own, and cannot be given by name.
- `@!arr` also works where a C function takes a pointer to the elements (`memset(@!v, 0, 32)`).
- A borrowed array cannot be a return type.

### Array of Pointer Slots (`[n]@![]T`)

`[n]@![]T` declares an array of `n` writable pointer slots, each capable of holding a `[]T`
pointer. The outer array is heap-allocated (`malloc(n * 8)`) and automatically freed at scope
exit. A slot borrows what it points to and never owns it: store into it with `->` (for example
memory from C `malloc`) and free that memory yourself. `->>` into a slot is a compile error.
`[n]@![]$[m]T` and `[n]@![]$T` are slots the same way, pointing at contiguous rows or structs.

```palan
int64 rows = 4;
[rows]@![]int32 ptrs;   // malloc(rows * 8) — outer array
malloc(3 * 4) -> ptrs[0];
// ...
free(ptrs[0]);
// free(ptrs) emitted automatically at scope exit
```

### Copying (`->`)

`src -> dst` where `dst` is an array or a struct copies the contents of `src` into the storage
`dst` already has; the two stay independent. The destination can be a variable, an array
element or row (`a[i]`, `m[i]`), or a field. Initializing a declaration from another array or
struct (`[4]$Point p = minos[1];`, `Point q = p;`) copies the same way.

```palan
[2][4]$Point minos = [[0,0][0,1][0,2][0,3], [0,0][0,1][1,1][2,1]];
[4]$Point mino = minos[1];   // copy row 1
minos[0][3] -> mino[0];      // copy one struct element
2 -> mino[1].x;              // minos is unchanged
```

- The source must have the same element type and the same sizes as the destination, all known
  at compile time; otherwise it is a compile error. A source may be borrowed (`@[n]T`, `@T`).
- What the destination owns is copied too: owned struct fields and arrays, and the structs of a
  `[n]T` array, get copies of the source's. A `@T`/`@!T` pointer is copied as a pointer.
- A struct returned by a function is copied the same way, and then freed. A `[]T` return has
  no size known at compile time, so it cannot be copied into an array.

### Ownership Transfer (`->>`)

`val ->> arr[i]` transfers ownership of `val` into an owned element or row: a struct of a
`[n]T` array, or a row of an `[m][n]T` array. What the element or row held is freed first. `val`
is then set to NULL, so that the automatic `free(val)` at scope exit becomes `free(NULL)` — a
no-op by C standard.

```palan
[3]int32 inner;
[2][3]int32 outer;

inner ->> outer[0];      // outer's original row 0 is freed; inner is set to NULL
// free(inner) at scope exit → free(NULL) = no-op
```

A row must have the same element type and the same size as `val`, both known at compile time.
A `@T`/`@!T` pointer slot (including `[n]@![]T`) cannot take ownership; `->>` into one is a
compile error.

`val ->> obj.field` transfers `val` into an owned struct field (`T` or `[n]T`). What the field
held is freed first, and `val` is set to NULL as above. Other fields are a compile error.

```palan
type W { Point pt; };
W w;
Point p;
p ->> w.pt;              // w's original Point is freed; p is set to NULL
```

The source must own what it gives away: a variable that owns its array or struct, an owned
field or element (`v.pt ->> w.pt`, `pts[1] ->> w.pt`, `m[1] ->> w.arr`), or a value returned by
a function. A field or element source is set to NULL like a variable. A borrow (`@T`/`@!T`,
`@[n]T`), a struct parameter, or a `@T` field is a compile error.

`return` on a tracked array variable also transfers ownership: the variable is removed from
free-tracking and the caller receives the pointer.

### Two-Dimensional Arrays (`[m][n]T`)

`[m][n]T` declares a two-dimensional array with `m` rows and `n` columns. The outer array is
heap-allocated; each row is independently heap-allocated by the auto-generated allocator.
`T` can also be a struct type; see [Struct Arrays](#struct-arrays).

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

### Array Literals

An array variable can be initialized with an array literal `[a, b, c]` in its declaration.
When the size is omitted (`[]T`), it is taken from the number of elements.

```palan
cinclude <stdio.h>;
int32 n = 10;
[3]int32 a = [1, -2, 3];
[]flo64 f = [1.5, 2, n];
printf("%d %d %d\n", a[0], a[1], a[2]);
printf("%.1f %.1f %.1f\n", f[0], f[1], f[2]);
```

Expected output:
```
1 -2 3
1.5 2.0 10.0
```

A two-dimensional literal can be written either as a sequence of rows (`[1,2,3][4,5,6]`) or as
nested rows (`[[1,2,3],[4,5,6]]`); both mean the same. It initializes both `[m][n]T` and
`[n]$[m]T` arrays, and omitted dimensions (`[][]T`, `[]$[]T`) are taken from the literal.

```palan
cinclude <stdio.h>;
[2][3]int32 a = [1,2,3][4,5,-6];
[]$[3]int32 d = [[1,2,3],[4,5,6]];
printf("%d %d %d %d\n", a[0][0], a[0][2], a[1][1], a[1][2]);
printf("%d %d %d\n", d[0][2], d[1][0], d[1][2]);
```

Expected output:
```
1 3 5 -6
3 4 6
```

Elements may be any expressions, converted to the element type by the same rules as a scalar
variable's initializer: implicit narrowing is an error (use an explicit cast such as `uint8(n)`),
and integer literals are range-checked against the element type. The elements are evaluated
before the array variable is declared, so they cannot refer to the variable itself.

A trailing comma after the last element of a row is allowed (`[1, 2, 3,]`), which is convenient
when a literal is written over several lines.

An array of structs (`[n]T`, `[n]$T`, `[m][n]T`, `[m][n]$T`) is initialized the same way, with each element
written either as its field values in declaration order (`[1, 2]`) or by field name
(`{y: 2, x: 1}`). Every field must be given exactly once. A struct field (`T` or `$T`) is written
as a nested struct value; pointer and array fields cannot be initialized by a literal. Since a
struct element is itself written in brackets, `[0,1][2,3]` is one row of two `Point`s.

```palan
cinclude <stdio.h>;
type Point { int32 x; int32 y; };
type Line { Point a; $Point b; };
[2][2]Point m = [[0,1][2,3], [4,5][6,7]];
[]$Point e = [{y: 5, x: 6}, [7, 8]];
[1]Line l = [[{x: 1, y: 2}, [3, 4]]];
printf("%d %d %d %d\n", m[1][0].y, e[0].x, l[0].a.y, l[0].b.x);
```

Expected output:
```
5 6 2 3
```

In a comma-separated declaration, each initializer belongs to its own variable:
`[2]int16 r, s = [7, 8];` initializes only `s`.

**Restrictions:**
- An array literal can only be used as an array variable's initializer — not as a function
  argument or the source of `->`.
- A given dimension must be a compile-time constant that matches the literal's element or row
  count, and every row of a 2D literal must have the same length.
- Only numeric (integer and float) and struct element types are supported. Arrays of pointers
  and arrays of three or more dimensions cannot be initialized with a literal.
- A `{name: value}` literal can only be used as a struct element of an array literal.
- Omitting only the inner dimension (`[2][]T`, `[2]$[]T`) is not supported.

### Struct Arrays

Palan supports four forms of struct array declarations. All are heap-allocated and automatically freed at scope exit.

| Form | Memory | Element type |
|---|---|---|
| `[n]$T pts` | `malloc(n * T.totalSize)` / `free(pts)` | contiguous inline elements |
| `[n]T pts` | `__pln_alloc_arr_T(n)` / `__pln_free_arr_T(pts, n)` | owned pointers, each element allocated separately |
| `[n]@T pts` | `malloc(n * 8)`, null-initialized / `free(pts)` | non-owning read-only pointers |
| `[n]@!T pts` | `malloc(n * 8)`, null-initialized / `free(pts)` | non-owning mutable pointers |

`pts[i]` yields a `T` pointer for all four forms. Fields are accessed with `pts[i].field`.

The pointer-slot forms also take a primitive element type: `[n]@int32` / `[n]@!int32` hold
`@int32` / `@!int32` pointers, and `pts[i][0]` reads or writes the value one points to.

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
@p -> rpts[0];
printf("%ld\n", rpts[0].x);   // 99

[4]@!Point wpts;   // writable slots
@!p -> wpts[0];
42 -> wpts[0].x;
printf("%ld\n", p.x);         // 42 (write-through via pointer)
```

A two-dimensional struct array is declared as `[m][n]T`: each of the `m` rows is an `[n]T` owned
pointer array. `pts[i][j]` yields a `T` pointer, and `pts[i][j].field` accesses a field.

```palan
cinclude <stdio.h>;
type Point { int64 x; int64 y; };
[2][3]Point grid;
7 -> grid[1][2].y;
printf("%ld\n", grid[1][2].y);  // 7
```

`[m][n]$T` instead makes each row an `[n]$T` contiguous array, so the structs sit in the rows
themselves. It is accessed the same way.

```palan
[2][3]$Point tiles;
7 -> tiles[1][2].y;
printf("%ld\n", tiles[1][2].y);  // 7
```

**Restrictions:**
- `[n]$T` and `[m][n]$T` require that `T` owns no fields (no `U field` or `[k]U field`). Use `[n]T` / `[m][n]T` instead when it does.
- Other two-dimensional struct array forms (`[m]$[n]T`, `[m]$[n]$T`) are not supported.

Array **fields** (declared inside `type { ... }`, see Section 19) use the same four forms but
require `n` to be a compile-time integer literal, since struct layout must be statically known.
Array **variables** (this section) allow non-constant `n`.

### Limitations (current version)

- Top-level (global) array variables are not freed at scope exit (the OS reclaims memory at process exit).
- Boundary checking is not performed.
- `[n]$T` is not supported when `T` owns fields.

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

@!Point view = @!original;   // non-owning alias — view and original share the same storage
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

`T` may be a primitive type or a struct name. `n` must be a compile-time integer: a literal, a
[const](#21-constant-declarations), or `sizeof(T)`.
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
Because nothing allocates on its behalf, a `$T` field (and a `[n]$T` field) requires that `T`
owns no fields (no `U field` or `[k]U field`); use `T field` instead.

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
@n2 -> n1.next;                         // set pointer value: OK
printf("%ld %ld\n", n1.val, n1.next.val);   // 42 100
```

`@T` allows reading through the pointer but not writing. Use `@!T` for write-through:

```palan
type Node { int64 val; @!Node next; };

Node n1;  Node n2;
@!n2 -> n1.next;
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

A `@T`/`@!T` parameter borrows a struct, and the caller writes `@s` (read-only) or `@!s`
(mutable), as with a [borrowed array](#borrowing-arrays):

```palan
func moveX(@!Point p, int64 dx) {
    p.x + dx -> p.x;
}

Point pt;
moveX(@!pt, 3);
```

A struct given by name where a `@T`/`@!T` is expected — a parameter, a variable's initializer, an
assignment, or a pointer field or slot — is a compile error. `@`/`@!` works on a struct variable
(including a struct-type parameter), an owned (`T`) or embedded (`$T`) struct field, and a struct
array element (`@!pts[1]`). A `@T`/`@!T` pointer itself is already a borrow and is passed by name; `@` on it is a compile error.

`q -> p` copies struct `q` into `p`, including what `q` owns; see [Copying](#copying--).

A named return of struct type transfers ownership to the caller:

```palan
func makePoint(int64 x, int64 y) -> Point p {
    Point p;
    x -> p.x;  y -> p.y;
}
```

The caller receives the returned struct with `->` (`Point pt; makePoint(1, 2) -> pt;`). Passed
directly as an argument (`getX(makePoint(1, 2))`), it is freed right after that call; called as a
statement on its own, it is freed at once. Binding it to a `@T`/`@!T` — a variable's initializer,
an assignment, a pointer field or slot, or a return — is a compile error, since nothing would
own it.

### Declaration order

A top-level struct, enum, type alias, or const may be used before its declaration, and so may
the C types of a top-level `cinclude`. A declaration inside a block is visible from that point
on.

```palan
type Shape { $Pos at; @Shape next; };   // Pos is declared below
type Pos { flo64 x; flo64 y; };
```

A type must not contain itself. Embedding or owning in a cycle (`type A { $B b; };` with
`type B { $A a; };`, possibly across files that import each other) or an alias cycle is a
compile error. A pointer field (`@T`/`@!T`) may close a cycle.

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

Declare an alias for a type with `type`:

```palan
type MyInt = int64;

MyInt x = 42;

func addOne(MyInt n) -> MyInt result {
    n + 1 -> result;
}
```

- The target may be a primitive, struct, or array type (`type PT = Point;`, `type Row = [3]int32;`).
- The alias may be used wherever a type is written: variable declarations, function signatures,
  struct field types, and array element types (`[n]MyInt`).

### Restrictions

- Unlike primitive types' `int64(x)` cast syntax, there is no constructor-cast syntax `MyAlias(x)`.

---

## 21. Constant Declarations

Declare a compile-time constant with `const`:

```palan
const MaxLen = 256;

[MaxLen]uint8 buf;
type Msg { [MaxLen]$uint8 body; };
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
- A const may reference another const (`const B = A;`); this chains naturally through inlining.
- Like the literal it names, a const takes its type from where it is used: `const N = 20;` can be
  passed to an `int32` parameter, used in `N + 2` for an `int16` variable, or assigned to `flo64`.

### Restrictions

- There is no name-collision check between a const and a variable — a variable declaration of the
  same name silently shadows a same-named const.
- Only a top-level const can size a borrowed array parameter (`@[N]T`); function signatures are
  registered before any other statement is processed.

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
  stat("/", @!st);
  @!timespec atim = @!st.st_atim;   // pointer to the embedded timespec field
  printf("%ld\n", atim.tv_sec);
  ```
- `@` also takes the address of a primitive-typed array element (`@arr[2]`, `@!arr[2]`),
  including an element of a fixed-size array field (`@!s.data[2]`) and the innermost element of a
  multi-dimensional array (`@!mat[0][1]`). A row (`@!mat[0]`) is borrowed as an array (see
  Borrowing Arrays), which also works where a C function takes a pointer to its elements:

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
(pointer-to-pointer). A struct or array variable is the exception: it is already its own pointer
to its storage, so `@s`/`@!s` and `@arr`/`@!arr` borrow it as it is (see
[Struct types in function signatures](#struct-types-in-function-signatures) and
[Borrowing Arrays](#borrowing-arrays)).

- Not usable on function parameters, except a struct-type parameter.
- Not usable on a borrow: a struct pointer (`@T`/`@!T` where `T` is a struct) or a borrowed array
  (`@[n]T`) is passed by name.
- On a local variable, usable only when the variable is primitive-typed, itself a pointer to a
  primitive (`@T`/`@!T`), a struct, or an array with primitive, struct, or `@T`/`@!T` pointer
  elements.
- On a struct field reached from a local variable, usable only when the leaf field is
  primitive-typed, an embedded struct (`$T`), or an owned struct (`T`) — a pointer-typed field
  (`@T`/`@!T`) or an array field is rejected.
- Not usable on a 2D array row (`@mat[i]`) or a pointer-slot array element (`[n]@T`/`[n]@!T`) —
  only a primitive-typed or struct array element.
- Not usable on a general expression (a call result, a parenthesized tuple, etc.) — only a local
  variable, a field reached from one, or an array element reached from one.

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
  and the 8-bit/16-bit integer types (including `bool`) are rejected.
- The return value is the kernel's raw result in `rax`: a negative value conventionally means
  `-errno` (e.g. `-9` for `EBADF`), which the caller must interpret itself — libc's `errno`
  variable is never touched.
- `syscall` is a reserved word, so a cinclude'd `<unistd.h>`'s `syscall()` wrapper function
  cannot be called from a file that uses this feature.

---
