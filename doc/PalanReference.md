# Palan Language Reference

**Version:** v0.1.8

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
10. [Block Statements](#10-block-statements)
11. [Modules (import / export)](#11-modules-import--export)
12. [Program Structure](#12-program-structure)

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
| Function call | `name(args)` | `add(3, 4)` |
| Explicit cast | `type(expr)` | `int32(x)` |
| Assignment expression | `expr -> var` | `x + 1 -> x` |

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
import "lib.pa";           // import Palan source file (see §11)
```

---

## 7. Function Definitions

Functions are defined with the `func` keyword. Return values are declared after `->`.
The optional `export` keyword makes the function callable from other Palan files that import this file (see §11).

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

## 10. Block Statements

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

## 11. Modules (import / export)

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

## 12. Program Structure

```palan
cinclude <stdio.h>;

int64 x = 10;
printf("%lld\n", x);
```

- Top-level statements execute as the `_start` entry point.
- User-defined functions can be placed before or after top-level statements.
- `return` is not valid at top-level. To exit early, call `exit()` from `<stdlib.h>`.

