Palan Abstract Syntax Tree Json Specification
============================================

ver. 0.1.18

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
- type-kind\* - Type kind string: "prim" "pntr" "arr" "strct" "union" "enum" "func" "user"
- const - Boolean, true if const qualified (omitted when false)
  1. prim - Primitive type
    - type-name\* - Type name string
      - Integer: "int8" "int16" "int32" "int64" "uint8" "uint16" "uint32" "uint64"
      - Float: "flo32" "flo64"
      - Other: "void"
  2. pntr - Pointer type
    - base-type\* - Base variable type
    - mutable - Boolean, true if writable array pointer slot (`@!` syntax); omitted when false
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
    Note: For `[m][n]T` (2D array), the outer arr's `base-type` is itself an `arr` type
    (`specifier: "raw"`, leaf `base-type` is a prim type). SA transforms this nested arr
    into a `pntr(pntr(T))` var-decl with auto-generated allocator calls (see SASpec.md).
  4. embed - Directly embedded memory type (`$T`)
    - base-type\* - Base variable type
  5. strct - Struct type
    - name - Struct name string
    - fields - Field list
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

Block object
------------
Used in `func-def` bodies and standalone block statements.

- functions\* - Palan function definition list local to this block (may be empty array)
- body\* - Statement model list (does not contain func-def entries)

Statement model
---------------
- stmt-type\* - Statement type: "import" "cinclude" "expr" "var-decl" "assign" "arr-assign" "return" "tapple-decl" "block" "if" "while" "break" "continue"
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
  7. return - return statement
    - values - Return expression list (omitted for bare `return;`)
  8. tapple-decl - tuple-style multiple return value declaration (`(type name, ...) = call(...)`)
    - vars\* - Variable declaration list (name, var-type per entry)
    - value\* - Call expression model (must be a call to a multi-return Palan function)
  9. block - standalone block statement (`{ ... }`)
    - functions\* - Palan function definition list local to this block (may be empty array)
    - body\* - Statement model list (does not contain func-def entries)
  10. if - if / if-else statement
    - cond\* - Condition expression model
    - then\* - Then-block object (block statement body)
    - else - Else-block object or nested if statement (omitted when absent)
  11. while - while loop statement
    - cond\* - Condition expression model
    - body\* - Statement model list (raw array, no block wrapper)
  12. break - exit the innermost while loop (no additional fields)
  13. continue - skip to next iteration of innermost while loop (no additional fields)

Expression model
----------------
- expr-type\* - Expression type string: "lit-str" "lit-int" "lit-uint" "lit-flo" "id" "add" "sub" "cmp" "call" "cast" "arr-index"
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

Note: Negative integer literals (e.g. `-42`) are represented as a `neg` expression wrapping a positive literal.
Note: sa.json extends this format with additional fields and expression kinds. See SASpec.md.

Location Array
-----------------
- 0\* - Begin line integer
- 1\* - Begin column integer
- 2\* - End line integer
- 3\* - End column integer

