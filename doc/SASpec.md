Palan Semantic Analyzer JSON Specification
==========================================

ver. 0.1.9

Output of palan-sa. Extends the AST JSON format (see ASTSpec.md) with resolved
type information and pre-collected literal tables.

\* - Required

Root
----
- original\* - original source file path
- str-literals\* - String literal table (collected by SA, used by codegen for .rodata)
- functions\* - Processed Palan function list (empty array when no functions defined)
- statements\* - Top-level statement list

String literal table entry
--------------------------
- label\* - Assembly label string (e.g. ".str0")
- value\* - String value

Function model (sa.json)
------------------------
Same structure as the Palan function definition in ASTSpec.md, with `body` replaced by
SA-annotated statements.

- name\* - Function name string
- func-type\* - "palan"
- parameters\* - Parameter list (Palan parameter: name, var-type; empty array when no parameters)
- ret-type - Return variable type (single-return functions only)
- rets - Return value list (multi-return functions only)
- body\* - SA-annotated statement list (same rules as top-level statements below)

Note: cinclude statements are not present in function bodies (they are top-level only).

Statement model
---------------
Same structure as AST statements (see ASTSpec.md) with the following differences:

- cinclude statements are consumed by SA and not emitted
- import statements are consumed by SA and not emitted; imported functions are
  registered in the current scope and become callable from the point of import
- var-type resolved on id expressions (see Expression model below)
- assign and return statements are emitted as-is with SA-annotated expressions

- **block** - standalone block statement
  - stmt-type\*: "block"
  - body\*: statement list (cinclude consumed; func-def hoisted to sa["functions"])

  Scope rules:
  - Variables and Palan functions declared inside a block are not visible outside.
  - Shadowing of variables or Palan function names across any scope boundary is
    a compile error (BOOST_ASSERT).
  - C functions from cinclude inside a block are visible only until the end of that
    block. Shadowing of C function names is permitted.
  - Palan function definitions inside a block are pre-registered (pass 1) to
    support forward references within the same block.

- **if** - if / if-else statement
  - stmt-type\*: "if"
  - cond\*: SA-annotated condition expression (value-type present; integer expected)
  - then\*: then-block (same structure as block stmt body in ASTSpec.md)
  - else: else-block or nested if statement (omitted when absent)

Additional statement kinds emitted by SA:

- **assign** - assignment statement
  - stmt-type\*: "assign"
  - name\*: target variable name string
  - value\*: SA-annotated source expression (may be wrapped in convert node)

- **return** - return statement
  - stmt-type\*: "return"
  - values: SA-annotated return expression list (omitted for bare `return;`)

- **tapple-decl** - tuple-style multiple return value declaration
  - stmt-type\*: "tapple-decl"
  - vars\*: variable list (name, var-type per entry; types resolved by SA from function rets)
  - value\*: SA-annotated call expression (func-type: "palan"; carries value-types field)

Expression model
----------------
Same structure as AST expressions (see ASTSpec.md) with the following additions:

- value-type - Resolved Variable type object (same structure as var-type in ASTSpec.md).
  Present on all expression kinds except bare call with no return type.
  - lit-int: the expected type when used in a typed context (e.g. `int32 x = 10;` → int32);
    defaults to int64 when no expected type is available
  - lit-str: {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "uint8"}}
  - id: same object as var-type
  - add: promoted type of left and right operands (see Promotion rules);
    the narrower operand is wrapped in a convert node if types differ
  - sub: same promotion rules as add
  - cmp: always `{"type-kind": "prim", "type-name": "int32"}` (result is 0 or 1);
    left and right operands are promoted by the same rules as add
  - call: present when the function has a return type (ret-type in its definition)

SA-only expression kinds (not present in AST JSON):

- convert - Type conversion inserted by SA. Wraps an expression whose value-type
  differs from the required type. Used for both implicit widening and explicit casts
  (narrowing, signed↔unsigned). The `cast` AST node is consumed by SA and replaced
  by `convert` (or removed if Identical); it does not appear in sa.json.
  - expr-type\*: "convert"
  - value-type\*: target Variable type object
  - from-type\*: source Variable type object
  - src\*: inner expression being converted

  Example: `int64 y = x;` where x is int32 (implicit widening)
  ```json
  {
    "expr-type":  "convert",
    "value-type": {"type-kind": "prim", "type-name": "int64"},
    "from-type":  {"type-kind": "prim", "type-name": "int32"},
    "src": {"expr-type": "id", "name": "x", ...}
  }
  ```

  Example: `int32(x)` where x is int64 (explicit narrowing cast)
  ```json
  {
    "expr-type":  "convert",
    "value-type": {"type-kind": "prim", "type-name": "int32"},
    "from-type":  {"type-kind": "prim", "type-name": "int64"},
    "src": {"expr-type": "id", "name": "x", ...}
  }
  ```

Additional fields per expression kind:

- lit-str expression: value replaced by label (assembly label string, e.g. ".str0")
- id expression: var-type added (Variable type object from ASTSpec.md)
- call expression:
  - func-type\* added: "c" for C functions, "palan" for Palan user-defined functions
  - value-type added when the function has a single return type (ret-type in its definition)
  - value-types added when the function has multiple return values (rets in its definition);
    array of Variable type objects in declaration order

Promotion rules
---------------
Integer types are ranked by bit width. The higher-ranked type wins.

- Rank 1: int8, uint8
- Rank 2: int16, uint16
- Rank 3: int32, uint32
- Rank 4: int64, uint64

If one operand type is not in the table, the other operand's type is used as-is.

typeCompat rules
----------------
`typeCompat(from, to)` returns one of: `Identical`, `ImplicitWiden`, `ExplicitCast`, `Incompatible`.

| from \ to            | same type  | wider, same group | narrower or diff group (prim) | pointer | other |
|----------------------|------------|-------------------|-------------------------------|---------|-------|
| same prim type       | Identical  | —                 | —                             | —       | —     |
| signed int           | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| unsigned int         | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| float                | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| pointer (same base)  | —          | —                 | —                             | Identical | Incompat|
| pointer (diff base)  | —          | —                 | —                             | Incompatible | — |

Notes:
- Signed and unsigned are different groups; `int32 → uint32` requires `ExplicitCast`.
- `ExplicitCast` is only permitted at a `cast` expression site (`type-name(expr)`).
  Using it implicitly (e.g. assigning int64 to int32 directly) is a compile error.
- Variadic arguments undergo caller promotion: int8/int16 → int32, uint8/uint16 → uint32.

(TBD)
