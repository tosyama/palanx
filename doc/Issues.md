# Palan Issues

Open issues and topics for future consideration.

---

## 1. Borrowed Pointer Lifetimes Are Unchecked

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

Reading through `p` after the block exits reads freed memory — undefined behavior, observed to print a garbage value rather than crash. Detecting this requires a borrow-lifetime/escape analysis, which is out of scope for the address-of/dereference work that introduced general `@`/`@!` — no such analysis is implemented today. A stack variable (`$Point s;`) widens the reach of the same gap: a pointer into it that is returned from the function points into a frame that no longer exists.

A variable-length array (v0.2.1) adds a shorter-lived case: `push` and `resize` can move its elements, so a borrow of an element (`@!v[i]`) or the elements given to a `[]T` parameter dangles after the length changes, within the same block.

---

## 2. Function-Like Macro Export

**Summary:** Only object-like `#define` macros are ever exported as Palan constants (see ASTSpec.md's Constant definition model). A function-like macro such as `S_ISDIR(m)` (`sys/stat.h`) is not exported in any form — referencing its name from Palan reports that `S_ISDIR` is a function-like C macro Palan cannot use. `S_IFMT`/`S_IFDIR` etc. are already exported as plain constants, so `(m & S_IFMT) == S_IFDIR` can be written directly with v0.1.29's bitwise operators; a dedicated export mechanism for the macro-as-function form itself would need to translate arbitrary C macro bodies into Palan expressions, which is a new capability, not an extension of the constant-folding added this iteration.

---

## 3. C Global Variable Write Access

**Summary:** A C global variable brought in via `cinclude` (e.g. `stdout`) is read-only from Palan this version — assigning to it is `E_CGlobalNotAssignable` and taking its address is `E_CGlobalNotAddressable` (see PalanReference.md §12 and SASpec.md's Statement model). Allowing writes would introduce a new category of storage location — one not owned by any Palan scope, with no allocation or free timing of its own — into the ownership and scope rules that today assume every writable location is either a local variable or reached through a pointer with a known owner. Deliberately deferred rather than designed hastily alongside the read-only mechanism.

---

## 4. C Types Diagnosed But Not Represented (`long double`, `va_list`)

**Summary:** Several C type-kinds are recognized by c2ast and explicitly diagnosed by SA as unrepresentable (`E_UnsupportedParamType`/`E_UnsupportedCFuncSignature`/`E_UnsupportedCGlobalType`, see SASpec.md's C-origin signature admission) rather than causing a compiler abort, but none has an actual Palan-side representation: `long double` (c2ast's `flt128` prim name, unresolved to a known Palan primitive) and `va_list`-shaped types. C enums are no longer in this bucket as of v0.1.40: they register as Palan enums (see SASpec.md's Enum types); only an enum whose values c2ast cannot compute (e.g. `= sizeof(int)`) remains unrepresentable. C unions are no longer in this bucket as of v0.1.34: they register as structs whose fields all sit at offset 0 (see SASpec.md's Struct types). Diagnosing cleanly at the point of use was the v0.1.29 goal (IT-2026-09-06-2906); giving any of these a real representation is separate, larger work. A C function pointer is representable as `@func(...)` (see PalanReference.md §7) unless its own signature uses one of these types or is variadic.

---

## 5. No CRT Startup Objects Are Linked — `palan-codegen` Supplies ELF/libc Glue Itself

**Summary:** The build manager links a binary with a bare `ld <objs> -lc [-l<lib>...]` (`src/build-mgr/main.cpp`; the `-l<lib>` flags come from `link` clauses on `cinclude`, v0.1.31), never crt1.o/crti.o/crtbegin.o, and `palan-codegen --entry` emits `_start` itself. This works for ordinary programs, but v0.1.30's `atexit` support surfaced two gaps those missing CRT objects would otherwise have closed: `__dso_handle` was undefined (glibc's `atexit` forwards to `__cxa_atexit(func, arg, __dso_handle)`), and `.note.GNU-stack` was never emitted (so linking any note-carrying libc object made `ld` mark the whole binary's stack executable, `PT_GNU_STACK` RWE, with a linker warning). Both are now patched over by `PlnX86CodeGen`, which emits `__dso_handle` on the entry object and `.note.GNU-stack` unconditionally on every object (see SpecAndDesign.md §3.5). This closes the two gaps v0.1.30 actually hit, but a real CRT strategy — linking via crtbegin/crtend, PIE support, or driving the link through `cc` instead of bare `ld` — remains undesigned; the next libc-glue requirement this same root cause produces will need one. Since v0.2.2 the entry object holds only `_start`, which reads argc/argv from the initial stack and calls the entry module's top-level function `<module>..main(argc, argv)`; switching to a C `main` linked through `cc` would replace just that object with a `main` making the same call.

---

## 6. Array Literals Are Limited to Positions With a Known Target

**Summary:** An array literal (`[1, 2, 3]`, 2D in either `[1,2][3,4]` or `[[1,2],[3,4]]` form) and a struct value (`[1, 2]`/`{x: 1, y: 2}`) are accepted only where they fill storage of a known type: an array or struct variable's initializer, the source of `->` to an array or a struct, the value returned from a function returning a struct, and an argument for a Palan `T`, `@T`/`@!T` or borrowed array parameter (v0.1.40) or a C function's by-value struct parameter, for which SA builds a temporary of the parameter's type. Anywhere else, such as an operand (`[1, 2][0]`) or a C function's pointer parameter (`strlen(@[104, 0])`), SA rejects it: as a call argument the error names the parameter and its type, or the C function; elsewhere it lists the positions above. Every literal is built at run time with one store per element, so an all-constant literal is not placed in `.rodata`.

---

## 7. gen-ast Nesting Depth Limit

**Summary:** gen-ast reserves its GLR stack at the maximum depth (`YYMAXDEPTH`, 10000 items) and never grows it, so nesting blocks about 3330 levels deep (for example nested `while` loops) fails with "memory exhausted". Bison 3.8.2's `glr2.cc` grows the stack by `memcpy`-ing each `glr_stack_item` and then destroying the original, which frees the semantic value twice; this used to abort gen-ast at 65 nesting levels. Lifting the limit needs a skeleton change or the LALR conversion (#8). Real code does not nest this deeply.

---

## 8. Full LALR(1) Conversion of the Palan Grammar

**Summary:** The Palan grammar still has 5 shift/reduce and 6 reduce/reduce conflicts (v0.2.0; `samples/tetris.pa` takes 23 GLR splits), so gen-ast stays on Bison's GLR skeleton `glr2.cc`. Removing them all would allow `lalr1.cc`, where grammar actions get non-`const` `$N` that can be moved from, the parser never splits, and the nesting depth limit of #7 disappears with the GLR stack. The remaining conflicts: (a) `'.'` after `term` or a store location — field access versus member-call `term '.' ID '(' ... ')'`, needing two tokens of lookahead; after a bare `ID`, also versus a module-qualified type name `M.T`. (b) The 6 reduce/reduce conflicts — after `ID '=' expression` in a declaration list, whether it is an `inherit_var_decl` or a `var_declaration`. (c) `']'` — an array type `[expr]T` against an array literal `[expr]`.

---

## 9. `cinclude` Processing Time

**Summary:** Now that gen-ast parses in linear time, `cinclude` dominates its run time. In `samples/tetris.pa`, gen-ast took about 1.45s in a Debug build, of which the four `cinclude`s of `ncurses.h`, `unistd.h`, `stdlib.h` and `time.h` took about 1.35s (about 1.0s in the `palan-c2ast` child processes, `ncurses.h` about 0.6s). Building the tools with `-O2` (what `make` does since v0.2.0) brings the whole gen-ast run down to about 0.22s, so this is no longer a practical problem. If it becomes one again, the next steps are profiling c2ast's preprocessing and parsing of system headers, or caching translated headers between builds (the cache key would have to include the macro state passed in from the earlier `cinclude`s, see #10).

---

## 10. A Header Read Inside a Block Is Skipped by a Later `cinclude` Outside It

**Summary:** Each `cinclude` is preprocessed with the macros every earlier `cinclude` in the file left defined, in source order, so a header an earlier `cinclude` already read is skipped by its include guard. Its declarations come from that earlier `cinclude`, but C functions are visible only in the `cinclude`'s scope. When the earlier one is inside a block, the functions are not visible after the block: `func f() { cinclude <stdio.h>; ... }` followed by a top-level `cinclude <stdio.h>; printf("x\n");` reports `Undefined function 'printf'`. Fixing this needs either the macro state to follow block scope or SA to register the skipped declarations for the later `cinclude` as well.


---

## 11. `register_t` Is 32 Bits

**Summary:** c2ast does not define `__GNUC__`, so glibc headers take the path meant for non-GNU compilers. That path keeps the ABI everywhere except `sys/types.h`'s `register_t`, which becomes `int` (32 bits) instead of a word-sized (64-bit) integer. No other glibc header uses `register_t`, so only Palan code that names it directly is affected. Defining `__GNUC__` would instead pull in GNU-only constructs such as `_Float128`.

---

## 12. glibc Headers c2ast Cannot Read

**Summary:** Of the 242 public headers in libc6-dev (excluding `bits/` and `gnu/`), c2ast reads 200 (v0.2.0). 4 of the rest are not meant to be read (`regexp.h`, `sys/elf.h` and `sys/vm86.h` stop with `#error`; `finclude/math-vector-fortran.h` is not C). The other 38 fail on these constructs, each also failing every header that includes the one named:
- (a) Bit-fields — `a.out.h`, `arpa/nameser_compat.h` (`arpa/nameser.h`, `resolv.h`), `fenv.h`, `ieee754.h`, `netinet/ip.h` (`netinet/ip_icmp.h`), `netinet/tcp.h`, `obstack.h`, `printf.h`, `regex.h` (`re_comp.h`), `scsi/sg.h`, `sys/mtio.h`, `sys/timex.h`.
- (b) `restrict` in an unnamed parameter or inside array brackets (`const char *restrict)`, `p[restrict]`) — `glob.h`, `aio.h`, `spawn.h`. The non-GNU path defines `__restrict` as `restrict`; c2ast accepts `__restrict` in the same positions.
- (c) `__signed__` — `asm-generic/int-ll64.h`, reached from `net/ethernet.h`, `net/if_ppp.h`, `net/ppp_defs.h`, `netatalk/at.h`, `netinet/ether.h`, `netinet/if_ether.h`, `netinet/if_fddi.h`, `nfs/nfs.h`, `sys/fanotify.h`, `sys/prctl.h`, `sys/quota.h`, `sys/sysinfo.h`. Linux kernel headers use GNU keywords whether or not `__GNUC__` is defined.
- (d) `__has_include` in `#if` — `sys/mount.h`, `sys/rseq.h`.
- (e) `__extension__` before a struct member — `sys/user.h`.
- (f) `_Complex` — `complex.h`.
- (g) `#include` inside an enum body — `stab.h`.
- (h) A `static __inline__` function definition — `sys/platform/x86.h`.
- (i) `#error` for non-GNU compilers — `tgmath.h`. Its type-generic macros are function-like and would not be usable from Palan anyway (#2).

---

## 13. Returned Structs Are Always Heap-Allocated

**Summary:** A function returning a struct allocates the return value on the heap on every call, and the caller frees it when done; this holds also when the value is only a temporary (an argument such as `@sub(@a, @b)`, or the middle of `v -> f() -> g()`) and for named struct returns. In `samples/raytracer` (v0.2.0), rendering a 256×192 image makes about 6.7 million `calloc`/`free` pairs, about 137 per pixel (mostly from `sub`, `scene_intersect`'s named returns, `mul`, `normalized` and `add`), and `malloc`/`calloc`/`free` take about 66% of the instructions executed (callgrind). The 1024×768 render takes about 7.7s. A fix is to have the caller pass the storage for the return value, as C's struct return does: frame storage for a temporary or a `$T` variable, heap storage only for a variable that owns it. Receiving into `$T v = f();` copies the heap value into the frame and frees it, which is correct but does not avoid the allocation.

---

## 14. Float Values Are Never Kept in Registers

**Summary:** `PlnRegAlloc` assigns no XMM registers, so every `flo32`/`flo64` value lives in a stack slot and each arithmetic operation stores its result to memory and reloads it. `samples/raytracer/vecmath.pa`'s `dot` compiles to 32 instructions, most of them `movss` to and from the frame. Allocating XMM registers needs to save values that live across a call, since SysV makes every XMM register caller-saved. Together with #13, this keeps the raytracer far slower than C.

---

## 15. Exported Palan Functions Cannot Be Called from C by Name

**Summary:** Every Palan function links by a symbol made from its module, name and parameter types (`util..show.int32`, v0.2.1), which is no C identifier. C code linked in with `cinclude ... link "x.o"` can call a Palan function only through a function pointer it is given (a callback). Calling one by name would need a way to give an exported function its plain name, like C++'s `extern "C"`.

---

## 16. Same-Named Source Files in Different Directories Share a Module Name

**Summary:** The module part of a symbol is the source file's name without its directory, so a symbol does not depend on where the source is checked out. Two modules `a/util.pa` and `b/util.pa` that export a function of the same name and parameter types therefore give it the same symbol, and `ld` reports it as defined twice. Escaping `-` and `.` as `_` plus hex digits can likewise make two file names meet (`my-lib.pa` and `my_2dlib.pa`).

---

## 17. `->>` Into a Call Argument

**Summary:** A parameter that takes an owned array or struct (`[+]T`, `[n]T`, `T`) receives a copy of the argument. Handing a variable's storage over instead, as `v ->> f()` would, is not supported ("this expression is not supported"), so a function that consumes a large array always costs a copy. It needs a parameter form that takes ownership, and the caller's variable to be left NULL as `->>` into a variable does.

---

## 18. Using a Value After `->>` Is Not Detected

**Summary:** `a ->> b` leaves `a` NULL, and nothing stops `a` from being used afterwards: `len(@a)`, `a[0]` or `a.x` dereferences NULL and the program crashes. This holds for structs, `[+]T` and rows alike, and for a field or element given away (`p ->> q; s -> p;` copies into a NULL struct). Detecting it needs flow analysis of moved-from variables.

---

## 19. Run-Time Failures of Array Allocation Are Unchecked

**Summary:** A size or capacity known only at run time is not checked: a negative `n` in `[n]T` or `[+n]T` becomes a huge `uint64`. Nor is the result of `malloc`/`calloc`/`realloc` for any heap allocation, so running out of memory dereferences NULL later. `pop` on an empty `[+]T` and a negative `resize` do abort; an option to leave those checks out of a release build has been requested.

---

## 20. `len` Error Messages

**Summary:** `len` reports a missing length only for an array it recognizes (an array whose size is known only at run time, or a `[]T`). A target containing a call, such as `len(@c[f()])`, is not folded, and a value that is not an array, such as `len(@x)` for an `int32 x`, both report "Undefined function 'len'", which does not say what was given. `len` of a run-time-sized array would need the array to keep its length, which it does not.

---

## 21. Variable-Length Arrays of Pointer Slots

**Summary:** `[+]@T`/`[+]@!T` (a variable-length array of borrowed pointers) and `[+][n]@T` (rows of them) are not supported. Neither is a `[][+]T` parameter, which would need each element's header to be passed rather than its data; `@[n][+]T` or `@[+][+]T` borrows such an array instead.

---

## 22. A Row Is Pushed with `@`, a Struct Without

**Summary:** `push` on `[+]S` takes its element as a struct-type parameter, which borrows a struct given by name (`ps.push({x: 5, y: 6})`). On `[+][n]T` it takes a row as `@[n]T`, and a borrowed array must be written with `@` (`vp.push(@[7, 8])`; `vp.push([7, 8])` is an error), because arrays have no parameter form that borrows implicitly: a `[n]T` parameter takes a copy. Taking the row as `[n]T` would allow the plain literal but copy it twice, unless the parameter's storage could be handed over (see #17). Separately, the message for a struct given to `@T` without `@` names only `@[..]` for a literal, though `@{..}` works too.
