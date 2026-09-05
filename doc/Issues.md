# Palan Issues

Open issues and topics for future consideration.

---

## 1. Cast Syntax with Compound Types

**Summary:** The current cast syntax is `type-name(expr)`, but the grammar is undefined when array types (`[]T`) or reference types are involved.

e.g. How should `[]int32(x)` or similar constructs be written?

---

## 2. Move Short-Circuit Optimization Hint to SA

**Summary:** The branch-context detection for `&&` / `||` / `!` (deciding between `lowerBranchCond` and `lowerBranchCondTrue`) is currently implemented as look-ahead logic inside codegen. It may be cleaner to annotate this information in the SA phase to better separate concerns.

---

## 3. Multi-Dimensional Array Syntax `[,]`

**Summary:** It is undecided whether to allow multi-dimensional array syntax such as `[m, n]T` for raw arrays. Currently only 1D arrays are supported.

---

## 4. Top-Level Code and Modules

**Summary:** The behavior is undefined when top-level executable code exists in both the entry file and an imported module. It is unclear how multiple top-level code blocks should be ordered and executed.

---

## 5. `type` Definition Scope

**Summary:** `type` definitions are currently registered globally regardless of where they appear in the source. They should follow the same scope rules as variable declarations — visible only from the point of definition to the end of the enclosing scope.

---

## 6. Borrowed Pointer Lifetimes Are Unchecked

**Summary:** The address-of operators (`@`/`@!`) produce a borrowed pointer that is never allocated and never freed on its own — the object it points into keeps its own ownership and free timing unchanged. There is currently no check that the pointer does not outlive that object. A pointer taken from a struct field or array element inside a block can be assigned to a variable declared outside the block; once the block ends and the owning variable is freed, the outer pointer is left dangling with no diagnostic, e.g.:

```
type Point { int64 x; int64 y; };

@!int64 p;
{
    Point s;
    5 -> s.x;
    p = @!s.x;
}
printf("%ld\n", p[0]);
```

Reading through `p` after the block exits reads freed memory (observed to crash reliably in practice). Detecting this requires a borrow-lifetime/escape analysis, which is out of scope for the address-of/dereference work that introduced general `@`/`@!` — no such analysis is implemented today.

---

## 7. C Struct Pointer Fields Are Always Read-Only

**Summary:** A C struct field whose type is a pointer — either a scalar pointer field (`T *field;`, SA's `raw-ptr`) or a pointer-slot array field (`T *field[n];`, SA's `embed-ptr-arr`) — is always registered with `mutable:false`, regardless of whether the C declaration's pointee was `const`-qualified. `normalizeCType` (the single point that folds C's pointee-`const` into SA's `mutable` vocabulary) is applied only to `cinclude`d function signatures, not to struct field types, so a non-const C pointer field (e.g. `struct Foo *next;`) is conservatively treated the same as a const one. Extending `normalizeCType` (or an equivalent) to struct field registration would let non-const C pointer fields be written through, matching their actual C semantics.
