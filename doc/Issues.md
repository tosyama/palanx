# Palan Issues

Open issues and topics for future consideration.

---

## 6. Borrowed Pointer Lifetimes Are Unchecked

**Summary:** The address-of operators (`@`/`@!`) produce a borrowed pointer that is never allocated and never freed on its own — the object it points into keeps its own ownership and free timing unchanged. There is currently no check that the pointer does not outlive that object. A pointer taken from a struct field or array element inside a block can be assigned to a variable declared outside the block; once the block ends and the owning variable is freed, the outer pointer is left dangling with no diagnostic, e.g.:

```
type Point { int64 x; int64 y; };

@!int64 p;
{
    Point s;
    5 -> s.x;
    @!s.x -> p;
}
printf("%ld\n", p[0]);
```

Reading through `p` after the block exits reads freed memory — undefined behavior, observed to print a garbage value rather than crash. Detecting this requires a borrow-lifetime/escape analysis, which is out of scope for the address-of/dereference work that introduced general `@`/`@!` — no such analysis is implemented today.

---

## 9. Function-Like Macro Export

**Summary:** Only object-like `#define` macros are ever exported as Palan constants (see ASTSpec.md's Constant definition model). A function-like macro such as `S_ISDIR(m)` (`sys/stat.h`) is not exported in any form — referencing its name from Palan is `Undefined function 'S_ISDIR'`, a normal diagnostic, not a compiler abort. `S_IFMT`/`S_IFDIR` etc. are already exported as plain constants, so `(m & S_IFMT) == S_IFDIR` can be written directly with v0.1.29's bitwise operators; a dedicated export mechanism for the macro-as-function form itself would need to translate arbitrary C macro bodies into Palan expressions, which is a new capability, not an extension of the constant-folding added this iteration.

---

## 11. Shift Operators (`<<<`/`>>>`)

**Summary:** v0.1.29 added the bitwise operators `&`/`|`/`^`/`~` but deliberately left out shift operators. `<<` has no token in Palan's lexer at all. `>>` is already `DBL_GRTR`, the move-owner mark on a call argument (`f(x>>)`) and on a parameter (`T >> name`); `->>` is a separate token. Shift operators will be spelled `<<<` (left) and `>>>` (logical right), so they do not collide with it. There is no arithmetic right shift; division by a power of two serves instead.

---

## 12. C Global Variable Write Access

**Summary:** A C global variable brought in via `cinclude` (e.g. `stdout`) is read-only from Palan this version — assigning to it is `E_CGlobalNotAssignable` and taking its address is `E_CGlobalNotAddressable` (see PalanReference.md §9 and SASpec.md's Statement model). Allowing writes would introduce a new category of storage location — one not owned by any Palan scope, with no allocation or free timing of its own — into the ownership and scope rules that today assume every writable location is either a local variable or reached through a pointer with a known owner. Deliberately deferred rather than designed hastily alongside the read-only mechanism.

---

## 13. C Types Diagnosed But Not Represented (`long double`, `va_list`)

**Summary:** Several C type-kinds are recognized by c2ast and explicitly diagnosed by SA as unrepresentable (`E_UnsupportedParamType`/`E_UnsupportedCFuncSignature`/`E_UnsupportedCGlobalType`, see SASpec.md's C-origin signature admission) rather than causing a compiler abort, but none has an actual Palan-side representation: `long double` (c2ast's `flt128` prim name, unresolved to a known Palan primitive) and `va_list`-shaped types. C enums are no longer in this bucket as of v0.1.40: they register as Palan enums (see SASpec.md's Enum types); only an enum whose values c2ast cannot compute (e.g. `= sizeof(int)`) remains unrepresentable. C unions are no longer in this bucket as of v0.1.34: they register as structs whose fields all sit at offset 0 (see SASpec.md's Struct types). Diagnosing cleanly at the point of use was the v0.1.29 goal (IT-2026-09-06-2906); giving any of these a real representation is separate, larger work. C function-pointer parameters are no longer in this bucket as of v0.1.30: a function-pointer parameter can now be passed a Palan function name as a callback argument (see PalanReference.md's "Passing a Palan Function as a Callback"), but that is passing a value into a slot, not representing a function-pointer *type* — there is still no first-class function-pointer variable, no way to declare one, and no way to call through one from Palan code.

---

## 14. Passing a Struct By Value to a C Function Is Unsupported

**Summary:** v0.1.30 implemented a C function *returning* a struct by value as a general System V AMD64 classification (see SASpec.md's call expression `struct-ret` entry), but the reverse direction — a struct-typed **parameter** passed by value (not by pointer) — is still rejected outright: `normalizeCFuncSig` marks any C function with a top-level by-value struct parameter as `_unsupported-sig: "by-value struct parameter"`, so the function itself becomes unusable rather than just that call shape. `stdio.h`'s `fopencookie` (which takes a `cookie_io_functions_t` by value) is the running example. A C union passed by value falls under the same rule, since SA registers unions as structs (v0.1.34); its diagnostic still says "by-value struct parameter". Implementing this would need the same eightbyte classification machinery applied to argument passing (SysV register/stack placement for a classified struct argument) rather than return-value placement. A related, narrower gap: even on the return side, a struct whose size classifies into a fractional eightbyte width other than 1/2/4/8 bytes (e.g. 3, 5, 6, or 7 bytes) is diagnosed (`E_UnsupportedCStructReturn`) rather than implemented, since no function in the v0.1.30 audit needed it.

---

## 15. No CRT Startup Objects Are Linked — `palan-codegen` Supplies ELF/libc Glue Itself

**Summary:** The build manager links a binary with a bare `ld <objs> -lc [-l<lib>...]` (`src/build-mgr/main.cpp`; the `-l<lib>` flags come from `link` clauses on `cinclude`, v0.1.31), never crt1.o/crti.o/crtbegin.o, and `palan-codegen` emits `_start` itself. This works for ordinary programs, but v0.1.30's `atexit` support surfaced two gaps those missing CRT objects would otherwise have closed: `__dso_handle` was undefined (glibc's `atexit` forwards to `__cxa_atexit(func, arg, __dso_handle)`), and `.note.GNU-stack` was never emitted (so linking any note-carrying libc object made `ld` mark the whole binary's stack executable, `PT_GNU_STACK` RWE, with a linker warning). Both are now patched over by `PlnX86CodeGen::emitElfCrtGlue()`, which emits `__dso_handle` on the entry object and `.note.GNU-stack` unconditionally on every object (see SpecAndDesign.md §3.5). This closes the two gaps v0.1.30 actually hit, but a real CRT strategy — linking via crtbegin/crtend, PIE support, or driving the link through `cc` instead of bare `ld` — remains undesigned; the next libc-glue requirement this same root cause produces will need one. To be revisited together with the design for receiving command-line arguments, since switching to a C `main` linked through `cc` changes the program's entry point.

---

## 19. C11 Anonymous Members Without a Member Name Fail to Parse

**Summary:** c2ast's struct/union field parser requires a declarator after every member's type, so a C11 anonymous member (`struct S { int k; union { int a; float b; }; };`) is a parse error ("unexpected token") that fails the whole `cinclude`, for struct and union bodies alike. An anonymous body that has a member name (`union { ... } u;`) works since v0.1.34 (c2ast synthesizes a tag for it). Supporting the unnamed form would need both the parse and promoted field access (`s.a` reaching into the anonymous member), which SA's field resolution has no concept of. None of the headers targeted so far (including glibc's `pthread.h`) uses this form.

---

## 21. Array Literals Are Limited to Positions With a Known Target

**Summary:** An array literal (`[1, 2, 3]`, 2D in either `[1,2][3,4]` or `[[1,2],[3,4]]` form) and a struct value (`[1, 2]`/`{x: 1, y: 2}`) are accepted only where they fill storage of a known type: an array or struct variable's initializer, the source of `->` to an array or a struct, the value returned from a function returning a struct, and an argument for a Palan `T`, `@T`/`@!T` or borrowed array parameter (v0.1.40), for which SA builds a temporary of the parameter's type. Anywhere else, such as an operand (`[1, 2][0]`) or a C function's pointer parameter (`strlen(@[104, 0])`), SA rejects it with `E_ArrLitContext`. Every literal is built at run time with one store per element, so an all-constant literal is not placed in `.rodata`.

---

## 22. C Macro Constants Using Unary `~` Are Not Exported

**Summary:** c2ast's macro-constant folding (ASTSpec.md's Constant definition model) recognizes unary `~` syntactically but does not fold it, so a macro whose body uses it is not exported. For example, ncurses' `A_ATTRIBUTES` (`NCURSES_BITS(~(1U - 1U),0)`) is unavailable, while the other `A_*` attributes are exported. The typed folding added in v0.1.37 already carries each operand's C type, so `~` could fold as a bitwise complement at the promoted operand's width, with the result typed like the operand. An untyped operand has no fixed width, so it needs its own rule, for example taking the C type the literal would have.

---

## 24. gen-ast Nesting Depth Limit

**Summary:** gen-ast reserves its GLR stack at the maximum depth (`YYMAXDEPTH`, 10000 items) and never grows it, so nesting blocks about 3330 levels deep (for example nested `while` loops) fails with "memory exhausted". Bison 3.8.2's `glr2.cc` grows the stack by `memcpy`-ing each `glr_stack_item` and then destroying the original, which frees the semantic value twice; this used to abort gen-ast at 65 nesting levels. Lifting the limit needs a skeleton change or the LALR conversion (#25). Real code does not nest this deeply.

---

## 25. Full LALR(1) Conversion of the Palan Grammar

**Summary:** The Palan grammar still has 14 shift/reduce and 7 reduce/reduce conflicts (v0.1.40), so gen-ast stays on Bison's GLR skeleton `glr2.cc`. Removing them all would allow `lalr1.cc`, where grammar actions get non-`const` `$N` that can be moved from, the parser never splits, and the nesting depth limit of #24 disappears with the GLR stack. The remaining conflicts: (a) `'.'` after `term` or a store location — field access versus member-call `expression '.' ID '(' ... ')'`, needing two tokens of lookahead; after a bare `ID`, also versus a module-qualified type name `M.T`. (b) `'<'` — the reserved, not-implemented generic type syntax `ID<T>` / `interface ID<T>` against the comparison operator; resolving it is a language decision on keeping that syntax. (c) The 7 reduce/reduce conflicts and 2 of the `'='` shift/reduce conflicts — after `ID '=' expression` in a declaration list, whether it is an `inherit_var_decl` or a `var_declaration`. (d) `'='` — a syscall's named-return initializer (`syscall f() -> int32 r = 1 = 60;`) against its syscall number. LALR state merging leaks it into every initialized typed declaration (`int32 x = 1;`), which accounts for 41 of the 53 GLR splits in `samples/tetris.pa`; `%define lr.type ielr` does not split those states. (e) `']'` — an array type `[expr]T` against an array literal `[expr]`.

---

## 26. `cinclude` Processing Time

**Summary:** Now that gen-ast parses in linear time, `cinclude` dominates its run time. In `samples/tetris.pa`, gen-ast took about 1.45s in a Debug build, of which the four `cinclude`s of `ncurses.h`, `unistd.h`, `stdlib.h` and `time.h` took about 1.35s (about 1.0s in the `palan-c2ast` child processes, `ncurses.h` about 0.6s). Building the tools with `-O2` (what `make` does since v0.2.0) brings the whole gen-ast run down to about 0.22s, so this is no longer a practical problem. If it becomes one again, the next steps are profiling c2ast's preprocessing and parsing of system headers, or caching translated headers between builds.

---

## 27. A `cinclude`d Header Cannot See Earlier `cinclude`s' Macros

**Summary:** Each `cinclude` is translated by its own palan-c2ast run, so a header written to follow another header in the same C translation unit does not see that header's macros: an `#ifdef`/`#if` on a macro the earlier header defines takes the other branch, and a macro it uses is not expanded. Typedefs are resolved across `cinclude`s by SA, but macros act during preprocessing, before SA. Supporting this needs a way to pass the macros in effect at the `cinclude` point (for example the earlier headers, or their resulting macro table) to palan-c2ast.
