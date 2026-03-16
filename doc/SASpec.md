Palan SA JSON Specification
============================

ver. 0.1.2

Output of palan-sa. Extends the AST JSON format (see ASTSpec.md) with resolved
type information and pre-collected literal tables.

\* - Required

Root
----
- original\* - original source file path
- str-literals\* - String literal table (collected by SA, used by codegen for .rodata)
- statements\* - Statement list

String literal table entry
--------------------------
- label\* - Assembly label string (e.g. ".str0")
- value\* - String value

Statement model
---------------
Same structure as AST statements (see ASTSpec.md) with the following differences:

- cinclude statements are consumed by SA and not emitted
- var-type resolved on id expressions (see Expression model below)

Expression model
----------------
Same structure as AST expressions (see ASTSpec.md) with the following additions:

- value-type - Resolved Variable type object (same structure as var-type in ASTSpec.md).
  Present on all expression kinds except call.
  - lit-int: the expected type when used in a typed context (e.g. `int32 x = 10;` → int32);
    defaults to int64 when no expected type is available
  - lit-str: {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "uint8"}}
  - id: same object as var-type
  - add: promoted type of left and right operands (see Promotion rules);
    the narrower operand is wrapped in a convert node if types differ

SA-only expression kinds (not present in AST JSON):

- convert - Implicit type widening inserted by SA. Wraps an expression whose
  value-type is narrower than the required type.
  - expr-type\*: "convert"
  - value-type\*: target Variable type object (the wider type)
  - from-type\*: source Variable type object (the narrower type)
  - src\*: inner expression being converted

  Example: `int64 y = x;` where x is int32
  ```json
  {
    "expr-type":  "convert",
    "value-type": {"type-kind": "prim", "type-name": "int64"},
    "from-type":  {"type-kind": "prim", "type-name": "int32"},
    "src": {"expr-type": "id", "name": "x", ...}
  }
  ```

  Narrowing (wider → narrower) is a compile-time error except when the source
  is an integer literal (lit-int / lit-uint).

Additional fields per expression kind:

- lit-str expression: value replaced by label (assembly label string, e.g. ".str0")
- id expression: var-type added (Variable type object from ASTSpec.md)
- call expression: func-type\* added ("c" for C functions)

Promotion rules
---------------
Integer types are ranked by bit width. The higher-ranked type wins.

- Rank 1: int8, uint8
- Rank 2: int16, uint16
- Rank 3: int32, uint32
- Rank 4: int64, uint64

If one operand type is not in the table, the other operand's type is used as-is.

(TBD)
