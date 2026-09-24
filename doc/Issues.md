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

## 14. C Types Diagnosed But Not Represented (`long double`, `va_list`, `enum`)

**Summary:** Several C type-kinds are recognized by c2ast and explicitly diagnosed by SA as unrepresentable (`E_UnsupportedParamType`/`E_UnsupportedCFuncSignature`/`E_UnsupportedCGlobalType`, see SASpec.md's C-origin signature admission) rather than causing a compiler abort, but none has an actual Palan-side representation: `long double` (c2ast's `flt128` prim name, unresolved to a known Palan primitive), `va_list`-shaped types, and `enum` types (whose enumerator lists c2ast parses but does not capture — see ASTSpec.md's Variable type `enum` entry). C unions are no longer in this bucket as of v0.1.34: they register as structs whose fields all sit at offset 0 (see SASpec.md's Struct types). Diagnosing cleanly at the point of use was the v0.1.29 goal (IT-2026-09-06-2906); giving any of these a real representation is separate, larger work. C function-pointer parameters are no longer in this bucket as of v0.1.30: a function-pointer parameter can now be passed a Palan function name as a callback argument (see PalanReference.md's "Passing a Palan Function as a Callback"), but that is passing a value into a slot, not representing a function-pointer *type* — there is still no first-class function-pointer variable, no way to declare one, and no way to call through one from Palan code.

---

## 15. `setCCForOp` Uses Signed Condition Codes for Unsigned Comparisons

**Summary:** `setCCForOp` (`src/codegen/PlnX86CodeGen.cpp`) always emits a signed `setCC` mnemonic (`setl`/`setg`/etc.) regardless of the compared type's signedness. For an unsigned comparison this is wrong: `uint8 250 < 10` evaluates to `true` (250 is being read as the signed value -6). The fix needs an unsigned-specific mnemonic table (`setb`/`seta`/etc.), mirroring the signed/unsigned split already done for arithmetic and multiply instructions in `IT-2026-09-07-x86-uint-mnemonic-width`.

---

## 16. `emitInstrDiv`/`emitInstrMod` Always Use 64-Bit `idivq`

**Summary:** `emitInstrDiv`/`emitInstrMod` (`src/codegen/PlnX86CodeGen.cpp`) hard-code the 64-bit signed `idivq` instruction regardless of the operand type's actual width or signedness. Sub-64-bit division/modulo and unsigned division/modulo have no dedicated code path and no test coverage. Needs the same per-type instruction table treatment `IT-2026-09-07-x86-uint-mnemonic-width` gave the other arithmetic instructions.

---

## 17. Passing a Struct By Value to a C Function Is Unsupported

**Summary:** v0.1.30 implemented a C function *returning* a struct by value as a general System V AMD64 classification (see SASpec.md's call expression `struct-ret` entry), but the reverse direction — a struct-typed **parameter** passed by value (not by pointer) — is still rejected outright: `normalizeCFuncSig` marks any C function with a top-level by-value struct parameter as `_unsupported-sig: "by-value struct parameter"`, so the function itself becomes unusable rather than just that call shape. `stdio.h`'s `fopencookie` (which takes a `cookie_io_functions_t` by value) is the running example. A C union passed by value falls under the same rule, since SA registers unions as structs (v0.1.34); its diagnostic still says "by-value struct parameter". Implementing this would need the same eightbyte classification machinery applied to argument passing (SysV register/stack placement for a classified struct argument) rather than return-value placement. A related, narrower gap: even on the return side, a struct whose size classifies into a fractional eightbyte width other than 1/2/4/8 bytes (e.g. 3, 5, 6, or 7 bytes) is diagnosed (`E_UnsupportedCStructReturn`) rather than implemented, since no function in the v0.1.30 audit needed it.

---

## 18. No CRT Startup Objects Are Linked — `palan-codegen` Supplies ELF/libc Glue Itself

**Summary:** The build manager links a binary with a bare `ld <objs> -lc [-l<lib>...]` (`src/build-mgr/main.cpp`; the `-l<lib>` flags come from `link` clauses on `cinclude`, v0.1.31), never crt1.o/crti.o/crtbegin.o, and `palan-codegen` emits `_start` itself. This works for ordinary programs, but v0.1.30's `atexit` support surfaced two gaps those missing CRT objects would otherwise have closed: `__dso_handle` was undefined (glibc's `atexit` forwards to `__cxa_atexit(func, arg, __dso_handle)`), and `.note.GNU-stack` was never emitted (so linking any note-carrying libc object made `ld` mark the whole binary's stack executable, `PT_GNU_STACK` RWE, with a linker warning). Both are now patched over by `PlnX86CodeGen::emitElfCrtGlue()`, which emits `__dso_handle` on the entry object and `.note.GNU-stack` unconditionally on every object (see SpecAndDesign.md §3.5). This closes the two gaps v0.1.30 actually hit, but a real CRT strategy — linking via crtbegin/crtend, PIE support, or driving the link through `cc` instead of bare `ld` — remains undesigned; the next libc-glue requirement this same root cause produces will need one.

---

## 19. `select`/`pselect` Remain Unusable — `fd_set`'s `sizeof`-Sized Array Field

**Summary:** v0.1.30's anonymous-struct-typedef support (c2ast synthesizing a struct tag from a typedef name) makes `fd_set`'s and `__sigset_t`'s typedef names resolve, and `select`/`pselect`'s signatures now type-check. But `fd_set`'s only field is an array sized by a `sizeof`-based constant expression (`__FD_SETSIZE / __NFDBITS`, ultimately involving `sizeof`), which c2ast's constant-expression evaluator still cannot fold — the same root cause as item 8's `_IO_FILE` gap. Without a folded size, the field has no known layout, so `fd_set` registers as an incomplete struct and cannot actually be allocated or written to, leaving `select`/`pselect` callable in name only. Resolved by the same fix item 8 calls for: `sizeof` evaluation in c2ast's constant folder.

---

## 20. Pointer-Bottomed C Typedefs Are Not Registered As Type Names

**Summary:** IT-2026-09-16-3103/3104 (v0.1.31) generalized cincluded typedef registration so that any `prim`- or tagged-`strct`-bottomed typedef a header defines becomes usable as a Palan type name, regardless of whether some C function in the header references it. Pointer-bottomed typedefs (`typedef void *timer_t;`, `typedef int *P;`) were deliberately excluded from the new `ast.typedefs` section (`src/c2ast/CParser.cpp`'s flush filter in `CParser::parse()`) — registering one would force a premature decision about which side of the `@`/`@!` mutability split a bare pointer alias falls on. The per-reference-site path (`registerTypedefAliasInType`'s `pntr` branch, `src/semantic-anlyzr/PlnSemanticAnalyzer.cpp`) doesn't rescue this either: it only recurses into the pointee's own base type, never registers the pointer typedef's own name. So a pointer-bottomed typedef's name is unusable as a Palan type in every path, and referencing it (`P p;`) produces the generic `E_UnknownStructType` message (`"unknown struct type 'P'."`) — accurate in that the name is unresolved, but misleading wording for a pointer alias rather than a struct. `E_UnknownStructType` is a cross-cutting message used from 8 call sites for various "unknown type name" cases, so a wording fix belongs to a separate, broader ticket, not to a pointer-typedef-specific one. Workaround: address-of a variable of the pointee's own type (`@!void t;` in place of a `timer_t` local) — this is a spelling limitation, not a missing capability.

---

## 21. `movq` With a 64-Bit Immediate Outside the 32-Bit Signed Range Fails to Assemble

**Summary:** Initializing an `int64`/`uint64` local (including a typedef'd one like `int64_t`) with a literal outside the sign-extended 32-bit immediate range (e.g. `int64_t c = 3000000000;`) emits `movq $3000000000, -24(%rbp)`, which GNU `as` rejects as an operand type mismatch — `movq` to a memory operand only accepts a 32-bit immediate, sign-extended; a true 64-bit immediate can only be loaded into a register (`movabsq`) and then stored. Codegen has no path that detects an out-of-range immediate and routes it through a register. Discovered incidentally while writing an IT-2026-09-16-3105 stdint.h test; worked around there by choosing in-range literals, since the fix belongs to `PlnX86CodeGen`'s immediate-emission logic, not to a test-only ticket.

## 22. A User-Defined Palan Function With a `flo64` Parameter or Return Type Fails to Assemble

**Summary:** Any Palan (not C) function declared with a `flo64` parameter or `flo64` return type fails at the `as` step with `operand type mismatch for 'movsd'` (and, for arithmetic on the parameter, `mulsd` too), regardless of whether the function is called across an `import` boundary or from the same file, and regardless of whether the argument is a literal or a variable. Reproduced with the minimal case `func show(flo64 x) { printf("%f\n", x); } show(2.0);` — no cinclude, no math library, no import involved. This is unrelated to C-function float parameter/return handling, which already works (`sqrt(2.0)` assigned into a `flo64` local and printed is fine; see `171_link_math.pa`). Discovered while designing the cross-module test for IT-2026-09-16-3108 (`link` clause library propagation) — the originally planned exported-function shape (`flo64` param/return) had to be replaced with an `int32` boundary wrapping internal `flo64` math to route around this bug, since fixing codegen is out of scope for that ticket.

---

## 23. `unistd.h`'s `syscall()` Wrapper Is Shadowed by the `syscall` Reserved Word

**Summary:** v0.1.32 makes `syscall` a globally reserved word to introduce the raw syscall declaration (`PlnParser.yy:874-894`), which shadows `unistd.h`'s own `long syscall(long, ...)` wrapper (`unistd.h:1091`) — a file that cinclude's `<unistd.h>` and uses a native `syscall` declaration cannot also call that libc wrapper by name. This is a deliberate trade-off, not an oversight: `link` (v0.1.31) hit the same collision and was resolved as a contextual keyword recognized only in `cinclude`'s trailing clause, a position (after `import_as`) where no other production can start. `syscall` can't reuse that trick — it appears statement-leading, where `ID ID '(' ...` is ambiguous between a variable declaration, an ordinary call-expression statement, and the new declaration, so disambiguating it contextually would need lookahead machinery this grammar doesn't have. Reserving it globally was judged acceptable because a native syscall declaration makes the libc wrapper largely redundant once the same syscall is declared directly. If a future need for both in the same file arises, the fix is the same contextual-keyword approach `link` used, scoped to the statement-leading position.

---

## 24. C11 Anonymous Members Without a Member Name Fail to Parse

**Summary:** c2ast's struct/union field parser requires a declarator after every member's type, so a C11 anonymous member (`struct S { int k; union { int a; float b; }; };`) is a parse error ("unexpected token") that fails the whole `cinclude`, for struct and union bodies alike. An anonymous body that has a member name (`union { ... } u;`) works since v0.1.34 (c2ast synthesizes a tag for it). Supporting the unnamed form would need both the parse and promoted field access (`s.a` reaching into the anonymous member), which SA's field resolution has no concept of. None of the headers targeted so far (including glibc's `pthread.h`) uses this form.
