Palan Abstract Syntax Tree Json Specification
============================================

ver. 0.1.4

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
(TBD)

AST model
---------
- functions - Function definition model list (Palan user-defined functions)
- statements - Statement model list

Function definition model
-------------------------
- name\* - Function name string
- func-type\* - Function type string: "palan" "c"
- loc - Location Array

  1. **palan** - Palan user-defined function
     - parameters - Parameter list (Palan parameter, see below)
     - ret-type - Return variable type (single-return functions only; omitted for void and multi-return)
     - rets - Return value list (multi-return functions only; omitted for single-return and void)
     - body\* - Statement model list (function body)

  2. **c** - C function prototype (from `cinclude`)
     - parameters - Parameter list (C parameter, see below)
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
      - Float: "flt32" "flt64"
      - Other: "void"
  2. pntr - Pointer type
    - base-type\* - Base variable type
  3. arr - Array type
    - base-type\* - Base variable type
    - size - Array size integer, null - unspecified
  4. strct - Struct type
    - name - Struct name string
    - fields - Field list
  5. union - Union type (TBD)
    - name - Union name string
    - fields - Field list
  6. enum - Enum type (TBD)
    - name - Enum name string
    - enumerators - Enumerator list
  7. func - Function type
    - parameters - Parameter list
    - ret-type\* - Return variable type
  8. user - User defined type
    - type-name\* - Type name string
    - base-type\* - Base variable type

Note: C `restrict` qualifier is not represented in the AST (optimization hint only).

Statement model
---------------
- stmt-type\* - Statement type: "import" "cinclude" "expr" "var-decl" "assign" "return" "tapple-decl"
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
  6. return - return statement
    - values - Return expression list (omitted for bare `return;`)
  7. tapple-decl - tuple-style multiple return value declaration (`(type name, ...) = call(...)`)
    - vars\* - Variable declaration list (name, var-type per entry)
    - value\* - Call expression model (must be a call to a multi-return Palan function)

Expression model
----------------
- expr-type\* - Expression type string: "lit-str" "lit-int" "lit-uint" "id" "add" "call" "cast"
  1. lit-str - String literal
    - value\* - String value
  2. lit-int - Signed integer literal (corresponds to INT token)
    - value\* - Decimal string (e.g. "10")
  3. lit-uint - Unsigned integer literal (corresponds to UINT token)
    - value\* - Decimal string (e.g. "10")
  4. id - Identifier (variable reference)
    - name\* - Identifier name string
  5. add - Binary addition
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  6. call - Function call expression
    - name\* - Function name string
    - args - Argument expression list
  7. cast - Explicit type cast expression (`type-name(expr)` syntax)
    - target-type\* - Target Variable type object
    - src\* - Source expression model

Note: Negative integer literals are represented as a unary minus expression (TBD).
Note: sa.json extends this format with additional fields and expression kinds. See SASpec.md.

Location Array
-----------------
- 0\* - Source file ID integer
- 1\* - Begin line integer
- 2\* - Begin column integer
- 3\* - End line integer
- 4\* - End column integer

(TBD)

