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

**Summary:** A C struct field whose type is a pointer — either a scalar pointer field (`T *field;`, SA's `raw-ptr`) or a pointer-slot array field (`T *field[n];`, SA's `embed-ptr-arr`) — is always registered with `mutable:false`, regardless of whether the C declaration's pointee was `const`-qualified. `normalizeCType` (the single point that folds C's pointee-`const` into SA's `mutable` vocabulary) is applied to `cinclude`d function signatures and, as of v0.1.29, C global variable types, but not to struct field types, so a non-const C pointer field (e.g. `struct Foo *next;`) is conservatively treated the same as a const one. Extending `normalizeCType` (or an equivalent) to struct field registration would let non-const C pointer fields be written through, matching their actual C semantics.

---

## 8. `_IO_FILE` Could Be Completed With `sizeof` Support

**Summary:** `FILE` (`_IO_FILE`) is registered as an incomplete struct (see SASpec.md's Incomplete struct types) because one field's array size expression, `char _unused2[15*sizeof(int) - 4*sizeof(void*) - sizeof(size_t)];`, cannot be evaluated — c2ast's constant-expression evaluator (v0.1.29's macro-constant folding, see ASTSpec.md's Constant definition model) has no `sizeof` operator. Adding `sizeof` evaluation there (knowing each primitive/pointer type's size) would let `_IO_FILE` register as a complete struct, closing the specific gap the incomplete-type mechanism was introduced to paper over. This is genuinely separate work from the fold added in v0.1.29: that fold only handles literals and the arithmetic/bitwise operators, not a type-querying operator.

---

## 9. Function-Like Macro Export

**Summary:** Only object-like `#define` macros are ever exported as Palan constants (see ASTSpec.md's Constant definition model). A function-like macro such as `S_ISDIR(m)` (`sys/stat.h`) is not exported in any form — referencing its name from Palan is `Undefined function 'S_ISDIR'`, a normal diagnostic, not a compiler abort. `S_IFMT`/`S_IFDIR` etc. are already exported as plain constants, so `(m & S_IFMT) == S_IFDIR` can be written directly with v0.1.29's bitwise operators; a dedicated export mechanism for the macro-as-function form itself would need to translate arbitrary C macro bodies into Palan expressions, which is a new capability, not an extension of the constant-folding added this iteration.

---

## 10. Palan `const` Constant-Expression Evaluation

**Summary:** A native Palan `const Name = expr;` declaration only accepts a bare compile-time literal (`lit-int`, `lit-uint`, `lit-flo`, `lit-str`) — `const X = 1 + 2;` is not supported (see PalanReference.md §21 Constant Declarations). This is a different mechanism from the C macro constant-folding introduced in v0.1.29 (ASTSpec.md's Constant definition model), which folds *C preprocessor* macro bodies during header ingestion, not Palan's own `const` syntax during ordinary statement processing. Extending Palan's `const` to accept an arbitrary compile-time expression would be separate work with its own evaluation rules (Palan's own type/overflow semantics, not C's).

---

## 11. Shift Operators (`<<`/`>>`) and the Move-Owner Token Conflict

**Summary:** v0.1.29 added the bitwise operators `&`/`|`/`^`/`~` but deliberately left out shift operators. `<<` has no token in Palan's lexer at all. `>>` is already `DBL_GRTR`, used exclusively as the move-owner operator (`[n]@![]T` array declarations, `->>` ownership-transfer assignment — see `src/gen-ast/PlnLexer.ll`/`PlnParser.yy`) — reusing it as a shift operator would collide with that existing grammar. Introducing shift support needs either an alternative token/syntax for one of the two roles, or a grammar-level disambiguation, decided before implementation.

---

## 12. C Global Variable Write Access

**Summary:** A C global variable brought in via `cinclude` (e.g. `stdout`) is read-only from Palan this version — assigning to it is `E_CGlobalNotAssignable` and taking its address is `E_CGlobalNotAddressable` (see PalanReference.md §9 and SASpec.md's Statement model). Allowing writes would introduce a new category of storage location — one not owned by any Palan scope, with no allocation or free timing of its own — into the ownership and scope rules that today assume every writable location is either a local variable or reached through a pointer with a known owner. Deliberately deferred rather than designed hastily alongside the read-only mechanism.

---

## 13. `uint64` ↔ `flo32`/`flo64` Conversion Unimplemented

**Summary:** v0.1.29's cross-signedness integer conversion fix (`PlnX86CodeGen::emitConvert`) left two direction pairs unimplemented: `uint64` to `flo32`/`flo64` and the reverse. Both require a 2^63 bias correction for values whose top bit is set (a single SSE convert instruction handles only the signed range), which needs a branch — `cvtsi2sd`/`cvttsd2si` alone are insufficient. `emitConvert` operates on straight-line code with no branching support, so the correct fix point is `PlnVCodeGen` (lowering to a `Label`/`CondJmp`/`Jmp` sequence before codegen), not `emitConvert` itself. Currently diagnosed with a dedicated abort message rather than miscompiling silently.

---

## 14. C Types Diagnosed But Not Represented (`long double`, `va_list`, Function Pointers, `union`, `enum`)

**Summary:** Several C type-kinds are recognized by c2ast and explicitly diagnosed by SA as unrepresentable (`E_UnsupportedParamType`/`E_UnsupportedCFuncSignature`/`E_UnsupportedCGlobalType`, see SASpec.md's C-origin signature admission) rather than causing a compiler abort, but none has an actual Palan-side representation: `long double` (c2ast's `flt128` prim name, unresolved to a known Palan primitive), `va_list`-shaped types, C function-pointer types (`func` type-kind), and by-value `union`/`enum` types (whose field/enumerator lists c2ast parses but does not capture — see ASTSpec.md's Variable type `union`/`enum` entries). Diagnosing cleanly at the point of use was the v0.1.29 goal (IT-2026-09-06-2906); giving any of these a real representation is separate, larger work.

---

## 15. `setCCForOp` Uses Signed Condition Codes for Unsigned Comparisons

**Summary:** `setCCForOp` (`src/codegen/PlnX86CodeGen.cpp`) always emits a signed `setCC` mnemonic (`setl`/`setg`/etc.) regardless of the compared type's signedness. For an unsigned comparison this is wrong: `uint8 250 < 10` evaluates to `true` (250 is being read as the signed value -6). The fix needs an unsigned-specific mnemonic table (`setb`/`seta`/etc.), mirroring the signed/unsigned split already done for arithmetic and multiply instructions in `IT-2026-09-07-x86-uint-mnemonic-width`.

---

## 16. `emitInstrDiv`/`emitInstrMod` Always Use 64-Bit `idivq`

**Summary:** `emitInstrDiv`/`emitInstrMod` (`src/codegen/PlnX86CodeGen.cpp`) hard-code the 64-bit signed `idivq` instruction regardless of the operand type's actual width or signedness. Sub-64-bit division/modulo and unsigned division/modulo have no dedicated code path and no test coverage. Needs the same per-type instruction table treatment `IT-2026-09-07-x86-uint-mnemonic-width` gave the other arithmetic instructions.
