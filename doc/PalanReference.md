# Palan Language Reference

**Version:** v0.1.17

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
| Grouping | `(expr)` | `-(a + b)` |
| Comparison | `expr < expr`, `<=`, `>`, `>=`, `==`, `!=` | `x < 10` |
| Function call | `name(args)` | `add(3, 4)` |
| Explicit cast | `type(expr)` | `int32(x)` |
| Assignment expression | `expr -> var` | `x + 1 -> x` |

Operator precedence (high to low): unary minus, `* / %`, `+ -`, comparisons, `->`.
All binary operators are left-associative.

Comparison operators produce `int32` (1 if true, 0 if false). Both operands are widened to the same type before comparison.

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
cinclude <stdio.h>;      // system header
cinclude "myheader.h";   // local header
printf("%ld\n", x);      // call C function
```

- `cinclude` makes C functions visible from the declaration point to the end of the enclosing scope.

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
- Circular imports (A imports B and B imports A) are supported.

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

When operands have different float widths, the narrower type is implicitly widened:

- `flo32 op flo64` → both become `flo64`; result is `flo64`

When one operand is an integer and the other is a float, the integer is implicitly widened to the float type:

- `int op flo32` → int widened to `flo32`; result is `flo32`
- `int op flo64` → int widened to `flo64`; result is `flo64`

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
The semantic analyzer resolves them to pointer types (`pntr(T)` and `pntr(pntr(T))`) with no
ownership tracking. The caller is responsible for managing the lifetime of the returned pointer.

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

### Limitations (current version)

- Top-level (global) array variables are not freed at scope exit (the OS reclaims memory at process exit).
- Multi-dimensional array declarations (`[m][n]T mat`) and chained element access (`mat[i][j]`) are not yet implemented.
- Boundary checking is not performed.
