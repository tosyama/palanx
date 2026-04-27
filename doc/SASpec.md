Palan Semantic Analyzer JSON Specification
==========================================

ver. 0.1.18

Output of palan-sa. Extends the AST JSON format (see ASTSpec.md) with resolved
type information and pre-collected literal tables.

\* - Required

Root
----
- original\* - original source file path
- str-literals\* - String literal table (collected by SA, used by codegen for .rodata)
- functions\* - Processed Palan function list (empty array when no functions defined)
- statements\* - Top-level statement list
- alloc-shapes\* - List of unique array shape descriptors collected from var-decls.
  Empty array when no multi-dimensional arrays are declared.
  Each entry:
  - shape-key\* - Shape key string (e.g. "arr\_arr\_int32")
  - leaf-type\* - Innermost element type name (e.g. "int32")
  - depth\* - Nesting depth integer (currently always 2 for `[m][n]T`)

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

Note: `[]T` (unsized array type) in `ret-type` or parameter `var-type` is resolved by SA to
`pntr(T)` with no ownership tracking. `[][]T` becomes `pntr(pntr(T))`.
These types are valid only in function signatures; using `[]T` in a `var-decl` is a compile error.

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

- **var-decl** (array transformation) - Array variable declarations are transformed by SA:

  **`[n]T` (raw array):** A `var-decl` with `arr` type-kind (`specifier: "raw"`, non-null `size-expr`,
  prim `base-type`) is rewritten as a `var-decl` with `pntr` type and a synthesized `malloc` call as init:
  - `var-type`: changed from `arr` to `pntr` (pointer to the element type)
  - `init`: `{"expr-type":"call","name":"malloc","func-type":"c","args":[<size_bytes>],"value-type":<pntr_type>}`
    where `size_bytes` = `size-expr` (when elem_size == 1) or `mul(size-expr, lit-uint(elem_size))` otherwise
  - The variable is registered in scope with `pntr` type
  - The `size-expr` must evaluate to an integer type; a float size is a compile error

  **`[n]@![]T` (array of pointer slots):** A `var-decl` with `arr` type-kind where `base-type` is
  `pntr(mutable=true, base=arr(T))` is also transformed:
  - `var-type`: changed to `pntr(pntr(T))` (pointer to pointer)
  - `init`: `malloc(size-expr * 8)` (each slot is a pointer; elem-size is always 8)
  - The outer array is freed at scope exit. Inner arrays (stored in slots) must be freed
    explicitly or transferred via `->>` before scope exit.

  **`[m][n]T` (2D array):** A `var-decl` with `arr` type-kind where `base-type` is itself an
  `arr(prim T)` is transformed to a 2D allocation:
  - A temp var `__<name>_d0` (uint64) is prepended, capturing the outer dimension expression.
  - `var-type`: changed to `pntr(pntr(T))`
  - `init`: palan call to `__pln_alloc_arr_arr_<leaf>(__<name>_d0, <inner-dim-expr>)`
  - A shape entry `{"shape-key": "arr\_arr\_<leaf>", "leaf-type": "<leaf>", "depth": 2}` is added
    to the root `alloc-shapes` array (deduplicated by shape-key across all var-decls).
  - Scope-exit free: palan call stmt `__pln_free_arr_arr_<leaf>(<name>, __<name>_d0)`.
  - build-mgr auto-generates the allocator/free Palan source from `alloc-shapes` and links it.

  Scope-exit cleanup:
  - At the end of each block/while/function body containing array var-decls, SA appends
    `free()` expression statements in reverse declaration order for that scope's arrays.
  - Top-level (global scope) arrays are NOT freed (OS reclaims at process exit).

  Early-exit cleanup:
  - `return`: free calls are prepended for all array vars in all active function-level scopes
    (innermost-first, reverse declaration order).
  - `break`/`continue`: free calls are prepended for arrays in all scopes within the current
    while loop body (including any nested blocks active at that point).
  - Prepended frees before `break`/`continue`/`return` may leave unreachable free calls at the
    normal scope-exit point; this is harmless (dead code, never executed at runtime).

- **while** - while loop statement
  - stmt-type\*: "while"
  - cond\*: SA-annotated condition expression (value-type present; integer expected)
  - body\*: SA-annotated statement list (raw array; variables declared inside are scoped to the loop body)

- **break** - exit the innermost while loop
  - stmt-type\*: "break"
  - (no additional fields; SA validates that break appears inside a while loop)

- **continue** - skip to next iteration of innermost while loop
  - stmt-type\*: "continue"
  - (no additional fields; SA validates that continue appears inside a while loop)

Additional statement kinds emitted by SA:

- **assign** - assignment statement
  - stmt-type\*: "assign"
  - name\*: target variable name string
  - value\*: SA-annotated source expression (may be wrapped in convert node)

- **arr-assign** - array element assignment (`val -> arr[i]` or `val ->> arr[i]`)
  - stmt-type\*: "arr-assign"
  - target\*: SA-annotated arr-index expression (see Expression model below)
  - value\*: SA-annotated source expression (may be wrapped in convert node to match elem type)

  When `ownership-transfer: true` (`->>` syntax): SA emits an additional `assign` statement
  immediately after the arr-assign that sets the source variable to NULL. The variable remains
  in `arrayScopeVars_` and receives `free(NULL)` at scope exit (C standard guarantees no-op).

- **return** - return statement
  - stmt-type\*: "return"
  - values: SA-annotated return expression list (omitted for bare `return;`)

  Ownership transfer on return: if the returned expression is an `id` whose variable is in
  `arrayScopeVars_`, SA removes it from the free-tracking set before emitting scope-exit
  cleanup. The caller receives ownership of the returned pointer.

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
  - lit-flo: adopts flo32 or flo64 when used in a float-typed context (e.g. `flo32 y = 1.5;` → flo32);
    defaults to flo64 when no expected float type is available
  - lit-str: {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "uint8"}}
  - id: same object as var-type
  - add: promoted type of left and right operands (see Promotion rules);
    the narrower operand is wrapped in a convert node if types differ
  - sub: same promotion rules as add
  - neg: same type as operand (no promotion)
  - cmp: always `{"type-kind": "prim", "type-name": "int32"}` (result is 0 or 1);
    left and right operands are promoted by the same rules as add
  - call: present when the function has a return type (ret-type in its definition).
    When `ret-type` is `pntr(T)` derived from a `[]T` signature, the caller is responsible
    for freeing the returned pointer (expiring ownership).

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

SA-only expression kinds (added to AST nodes):

- **arr-index** - Array element access (`arr[i]`). The AST node gains three SA-added fields:
  - value-type\*: element type (the `base-type` of the array's `pntr` value-type)
  - elem-size\*: `lit-uint` node whose `value` is the element byte size (1, 2, 4, or 8)
  - array\*: SA-annotated array expression (value-type must be `pntr`)
  - index\*: SA-annotated index expression (must not be float; integer types only)

  Validation:
  - If the array operand's value-type is not `pntr` → compile error (E_NotArrayType)
  - If the index expression resolves to flo32 or flo64 → compile error (E_ArrayIndexNotInteger)

  Example: `a[i]` where `[5]int32 a`:
  ```json
  {
    "expr-type": "arr-index",
    "value-type": {"type-kind": "prim", "type-name": "int32"},
    "array": {"expr-type": "id", "name": "a",
              "var-type":   {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "int32"}},
              "value-type": {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "int32"}}},
    "index":    {"expr-type": "id", "name": "i", "value-type": {"type-kind": "prim", "type-name": "int64"}, ...},
    "elem-size": {"expr-type": "lit-uint", "value": "4",
                  "value-type": {"type-kind": "prim", "type-name": "uint64"}}
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

Float promotion rules:

- `flo32 op flo64` → flo32 is implicitly widened to flo64; result is flo64
- `int op flo32` → int is implicitly widened to flo32; result is flo32
- `int op flo64` → int is implicitly widened to flo64; result is flo64
- `flo32 op int` / `flo64 op int` → symmetric with the above

The `%` (mod) operator on float operands is a compile error (SA emits an error and aborts).

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
| pointer (same base, mutable diff) | — | —            | —                             | Identical | — |
| pointer (diff base)  | —          | —                 | —                             | Incompatible | — |

Notes:
- Signed and unsigned are different groups; `int32 → uint32` requires `ExplicitCast`.
- `ExplicitCast` is only permitted at a `cast` expression site (`type-name(expr)`).
  Using it implicitly (e.g. assigning int64 to int32 directly) is a compile error.
- Variadic arguments undergo caller promotion: int8/int16 → int32, uint8/uint16 → uint32.
- `pntr(T)` and `pntr(T, mutable=true)` are treated as `Identical`; base-type match is sufficient
  for arr-assign target type checking.

(TBD)
