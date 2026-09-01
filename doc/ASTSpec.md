Palan Abstract Syntax Tree Json Specification
============================================

ver. 0.1.27

\* - Required

Root
----
- import - import file list
- export - export declaration list
- original - original file path
- ast\* - AST model

Import file list
----------------
- path-type\* - Path type string: "src" "inc"
- path\* - Path string

Export declaration list
-------------------------
Each entry is a Function definition model (see below) for an `export func` declaration,
with the `block` field omitted (signature only).

- name\* - Function name string
- func-type\* - Function type string: "palan"
- parameters\* - Parameter list
- ret-type - Return variable type (single-return; omitted for void and multi-return)
- rets - Return value list (multi-return; omitted for single-return and void)

AST model
---------
- functions - Function definition model list (Palan user-defined functions)
- statements - Statement model list
- constants - Constant definition model list (from `cinclude`; object-like `#define` macros recognized as a compile-time constant)

Function definition model
-------------------------
- name\* - Function name string
- func-type\* - Function type string: "palan" "c"
- loc\* - Location Array (palan only)

  1. **palan** - Palan user-defined function
     - export - Boolean, true if declared with `export` keyword (omitted when false)
     - parameters\* - Parameter list (Palan parameter, see below; empty array when no parameters)
     - ret-type - Return variable type (single-return functions only; omitted for void and multi-return)
     - rets - Return value list (multi-return functions only; omitted for single-return and void)
     - block\* - Block object (function body; see Block object below)

  2. **c** - C function prototype (from `cinclude`)
     - parameters\* - Parameter list (C parameter, see below; empty array when no parameters)
     - ret-type\* - Return variable type

Constant definition model
--------------------------
An object-like `#define` macro whose body, after fully expanding any references to other
object-like macros (e.g. `#define S_IFDIR __S_IFDIR`), resolves to one of two forms: a bare
integer literal (e.g. `#define MAGIC 42`), or a pointer-cast of a bare integer literal
(e.g. `#define NULL ((void *)0)`). Macros whose expanded body doesn't match either form
(function-like macros referenced without a call, string literals, arithmetic/other expression
forms, etc.) are not exported here.

- name\* - Macro name string
- value\* - Decimal string (e.g. "10")
- value-type\* - Variable type (see below)

Struct definition model
------------------------
Captured from `struct Name { field_decl... }` in a `cinclude`d C header (a forward
declaration with no body, e.g. `struct Missing;`, is not captured — there is no field
list to register). Attached to the `cinclude` statement model's `structs` field (see
Statement model below); same field-list shape as the native `struct-def` statement.

- name\* - Struct tag name string
- fields\* - Field list
  - name\* - Field name string
  - var-type\* - Field type (same Variable type object format)

Palan Parameter
---------------
Used in Palan function `parameters` and `rets`.

- name\* - Variable name string
- var-type\* - Variable type

C Parameter
-----------
Used in C function `parameters`.

- name\* - Parameter name string (or "..." for variadic sentinel)
- var-type - Variable type (omitted for variadic sentinel "...")

Return value (rets entry)
-------------------------
Same structure as Palan Parameter.

- name\* - Return value name string
- var-type\* - Variable type

Variable type
-------------
- type-kind\* - Type kind string: "prim" "pntr" "arr" "embed" "strct" "union" "enum" "func" "user"
- const - Boolean, true if const qualified (omitted when false)
  1. prim - Primitive type
    - type-name\* - Type name string
      - Integer: "int8" "int16" "int32" "int64" "uint8" "uint16" "uint32" "uint64"
      - Float: "flo32" "flo64"
      - Other: "void"
  2. pntr - Pointer type
    - base-type\* - Base variable type
    - mutable\* - Boolean, true for a writable pointer (`@!` syntax), false for a
      read-only pointer (`@` syntax)
  3. arr - Array type
    - base-type\* - Base variable type
    - specifier\* - Array kind string:
      - "raw"      — `[expr]type` or `[]type`    raw heap array (C-array equivalent, no count stored)
      - "fixed"    — `[#expr]type` or `[#]type`  heap array with element count in memory header
      - "variable" — `[+expr]type` or `[+]type`  variable-size array (size-expr = initial capacity)
    - size-expr - Array size expression model
      - non-null: size/capacity determined by this expression
      - null semantics differ by specifier:
        - "raw"      (`[]type`)  — initialization required; element count determined by SA from initializer
        - "fixed"    (`[#]type`) — initialization required; element count determined by SA from initializer
        - "variable" (`[+]type`) — no initial capacity; allocation deferred (no initialization required)
    - embedded - Boolean, true for contiguous 2D array (`[n]$[m]T` syntax); omitted when false
    Note: For `[m][n]T` (2D array via pointer-of-pointers), the outer arr's `base-type` is itself an
    `arr` type (`specifier: "raw"`, leaf `base-type` is a prim type). SA transforms this nested arr
    into a `pntr(pntr(T))` var-decl with auto-generated allocator calls (see SASpec.md).
    Note: For `[n]$[m]T` (contiguous 2D array), the outer arr has `embedded: true` and its `base-type`
    is an inner `arr`. SA allocates the entire grid as a single malloc of n*m elements.
    Note: The `$` token in `[n]$[m]T` produces an `arr` with `embedded:true`; `$T` standalone (outside
    an array context) produces the `embed` type-kind below.
  4. embed - Inline struct embedding (`$T` syntax in struct field declarations)
    - base-type\* - Base variable type of the embedded struct (type-kind "prim" with the struct name)
    Note: Valid only inside `struct-def` field lists. SA resolves the sub-struct layout and folds the
    embedded fields' offsets into the parent struct (no separate pointer).
  5. strct - Struct type reference (by name), from a C struct-typed parameter/return in a
     `cinclude`d function signature
    - type-name - Struct name string; omitted for a reference to a forward-declared-only tag
      (no captured field list — see Struct definition model above — so the reference cannot
      be resolved into a registered struct type)
  6. union - Union type (TBD)
    - name - Union name string
    - fields - Field list
  7. enum - Enum type (TBD)
    - name - Enum name string
    - enumerators - Enumerator list
  8. func - Function type
    - parameters - Parameter list
    - ret-type\* - Return variable type
  9. user - User defined type
    - type-name\* - Type name string
    - base-type\* - Base variable type

Note: C `restrict` qualifier is not represented in the AST (optimization hint only).

Note: A var-type node originating from a C typedef that resolves to a known type (type-kind "prim",
or "pntr" for a pointer-bottomed typedef chain, e.g. `typedef void *timer_t;`) may carry an additional
`typedef-name` field — the original C typedef identifier (e.g. `"size_t"`). This is a
c2ast/cinclude-only annotation. For type-kind "prim", SA registers it as a native type alias (see
PalanReference.md §20 Type Aliases) and strips the field before emitting to sa.json (see SASpec.md).
For type-kind "pntr", the field is left in place; it is not registered as an explicit Palan alias
type name and never reaches sa.json (C function signatures themselves aren't serialized there).

Block object
------------
Used in `func-def` bodies and standalone block statements.

- functions\* - Palan function definition list local to this block (may be empty array)
- body\* - Statement model list (does not contain func-def entries)

Statement model
---------------
- stmt-type\* - Statement type: "import" "cinclude" "expr" "var-decl" "assign" "arr-assign" "struct-def" "type-alias" "const-decl" "field-assign" "return" "tapple-decl" "block" "if" "while" "break" "continue"
- loc\* - Location Array (omitted for "not-impl")
  1. import - import module statement
    - path-type\* - Path type string: "src" "inc"
    - path\* - Path string
    - targets - Import target name string list, null - all
    - alias - Alias name string
  2. cinclude - C header include statement (expands in place, scope-aware)
    - path-type\* - Path type string: "src" "inc"
    - path\* - Path string
    - functions - Function definition model list (C prototypes from the header)
    - structs - Struct definition model list (see Struct definition model above); omitted
      when the header defines no capturable structs
  3. expr - expression statement
    - body\* - Expression model
  4. var-decl - variable declaration statement
    - vars\* - Variable declaration list
      - name\* - Variable name string
      - var-type\* - Variable type
      - init - Initializer expression model (omitted if no initializer)
  5. assign - assignment statement (`expr -> var`)
    - name\* - Target variable name string
    - value\* - Source expression model
  6. arr-assign - array element assignment statement (`expr -> arr[i]` or `expr ->> arr[i]`)
    - target\* - arr-index expression model (see Expression model §12)
    - value\* - Source expression model
    - ownership-transfer - Boolean, true if `->>` ownership-transfer syntax; omitted when false
  7. struct-def - struct type definition (`type Name { field_decl... }`; consumed by SA, not emitted to sa.json)
    - name\* - Struct name string
    - fields\* - Field list (each entry: `name`, `var-type`)
      - name\* - Field name string
      - var-type\* - Field type (same Variable type object format)
  8. type-alias - native type alias declaration (`type Name = type_expr;`; consumed by SA, not emitted to sa.json)
    - name\* - Alias name string
    - type\* - Aliased type (same Variable type object format)
  9. const-decl - native constant declaration (`const Name = <literal>;`; consumed by SA, not emitted to sa.json)
    - name\* - Constant name string
    - value\* - Literal expression model (lit-int, lit-uint, lit-flo, or lit-str; see Expression model)
  10. field-assign - struct field assignment (`value -> obj.field`)
    - object\* - Base store_loc (kind: "var")
    - field\*  - Field name string
    - value\*  - Source expression model
  11. return - return statement
    - values - Return expression list (omitted for bare `return;`)
  12. tapple-decl - tuple-style multiple return value declaration (`(type name, ...) = call(...)`)
    - vars\* - Variable declaration list (name, var-type per entry)
    - value\* - Call expression model (must be a call to a multi-return Palan function)
  13. block - standalone block statement (`{ ... }`)
    - functions\* - Palan function definition list local to this block (may be empty array)
    - body\* - Statement model list (does not contain func-def entries)
  14. if - if / if-else statement
    - cond\* - Condition expression model
    - then\* - Then-block object (block statement body)
    - else - Else-block object or nested if statement (omitted when absent)
  15. while - while loop statement
    - cond\* - Condition expression model
    - body\* - Statement model list (raw array, no block wrapper)
  16. break - exit the innermost while loop (no additional fields)
  17. continue - skip to next iteration of innermost while loop (no additional fields)

Expression model
----------------
- expr-type\* - Expression type string: "lit-str" "lit-int" "lit-uint" "lit-flo" "id" "add" "sub" "cmp" "call" "cast" "arr-index" "field-access" "logical-and" "logical-or" "logical-not" "addr-of"
- loc\* - Location Array (omitted for "not-impl" and "assign-expr")
  1. lit-str - String literal
    - value\* - String value
  2. lit-int - Signed integer literal (corresponds to INT token)
    - value\* - Decimal string (e.g. "10")
  3. lit-uint - Unsigned integer literal (corresponds to UINT token)
    - value\* - Decimal string (e.g. "10")
  4. lit-flo - Floating-point literal (corresponds to FLO token; format: digits.digits)
    - value\* - Decimal string (e.g. "3.14")
  5. id - Identifier (variable reference)
    - name\* - Identifier name string
  6. add - Binary addition
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  7. sub - Binary subtraction
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  8. neg - Unary negation
    - operand\* - Operand expression model
  9. cmp - Comparison expression (result: 0 or 1 as int32)
    - op\*    - Operator string: "<" "<=" ">" ">=" "==" "!="
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  10. call - Function call expression
    - name\* - Function name string
    - args - Argument expression list
  11. cast - Explicit type cast expression (`type-name(expr)` syntax)
    - target-type\* - Target Variable type object
    - src\* - Source expression model
  12. arr-index - Array element access (`arr[i]`)
    - array\* - Array expression model
    - index\* - Index expression model
    (elem-size is added by SA; not present in AST)
  13. logical-and - Short-circuit logical AND (`a && b`; result: int32, 0 or 1)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  14. logical-or - Short-circuit logical OR (`a || b`; result: int32, 0 or 1)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  15. logical-not - Logical NOT (`!a`; result: int32, 0 or 1)
    - operand\* - Operand expression model
  16. field-access - Struct field read (`obj.field` rvalue)
    - object\* - Object expression model (typically `id`)
    - field\*  - Field name string
  17. member-call - Qualified function call (`L.f(args)` syntax; consumed by SA, not emitted to sa.json)
    - object\* - Object expression (typically `id` for module alias; SA rejects non-`id` in v0.1.22)
    - method\* - Method/function name string
    - args - Argument expression list
    Note: SA resolves `member-call` and emits a regular `call` node in sa.json.
  18. addr-of - Address-of a local variable or a struct field reached from one (`@` read-only,
      `@!`/`AT_EXCL` mutable). The operand grammar is `store_loc` (the same vocabulary the
      left side of `->` accepts: a bare identifier, or a `.`-chain of field accesses rooted
      in one), so `@s.x` and `@!s.in.v` parse; SA decides in sa.json whether the target is
      addressable (see SASpec.md).
    - object\* - Operand expression: `id` for a plain variable, `field-access` for a struct
      field (nested `.` chains produce nested `field-access` objects), or an arbitrary
      expression node for anything else (SA rejects non-addressable operands)
    - mutable\* - `true` for `@!`, `false` for `@`

Note: Negative integer literals (e.g. `-42`) are represented as a `neg` expression wrapping a positive literal.
Note: sa.json extends this format with additional fields and expression kinds. See SASpec.md.

Location Array
-----------------
- 0\* - Begin line integer
- 1\* - Begin column integer
- 2\* - End line integer
- 3\* - End column integer

