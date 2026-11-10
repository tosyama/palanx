# Palan Standard Library Reference

**Version:** v0.2.4

The standard library is the library `std`, found with the compiler: `import <std/utf8>;` needs no
`libpath` (see [Using a Library](PalanReference.md#using-a-library) in the
[Palan Language Reference](PalanReference.md)). Its modules are ordinary Palan; nothing in it is
known to the compiler.

---

## Modules

- [std/utf8](#stdutf8) — UTF-8 text

---

## std/utf8

```palan
cinclude <stdio.h>;
import <std/utf8>;

utf8 s = toUtf8("h\xC3\xA9llo", utf8mode.KEEP_INVALID);
s.append(", ");
s.appendChar(8364);                         // U+20AC
printf("%s %ld %ld\n", s -> cstr(), s -> byteLen(), s -> charCount());   // héllo, € 11 8

int64 pos = 0;
while pos < s -> byteLen() {
    int32 c = s -> nextChar(@!pos);         // -1 for an invalid part
    printf("U+%04X ", c);
}
```

`utf8` is UTF-8 text held with a NUL at the end, so `cstr` gives it to C as it is. A declared
`utf8` is empty. Its `mode` says what it does with bytes that are not valid UTF-8 when it is made
or added to, and `hadInvalid` tells that such a part was met:

| `utf8mode` | Invalid part |
|------------|--------------|
| `KEEP_INVALID` (a declared `utf8`'s) | kept as it is |
| `EMPTY_ON_INVALID` | the text becomes empty, and stays empty whatever is added |
| `REPLACE_INVALID` | replaced with U+FFFD |

An invalid part is the longest start of a valid sequence, or else one byte, as Unicode
recommends: `"A\xE2\x82B"` has one, which `REPLACE_INVALID` makes `"A\xEF\xBF\xBDB"`. Only a
`KEEP_INVALID` text holds invalid bytes; `hadInvalid` is cleared only when adding to one
completes a sequence cut at its end.

| Function | Meaning |
|----------|---------|
| `toUtf8(@[]uint8 cs, utf8mode m) -> utf8` | the NUL-terminated bytes `cs`, made by the rules of `m` |
| `cstr(@utf8 s) -> @[]uint8` | the NUL-terminated bytes, until `s` is added to |
| `byteLen(@utf8 s) -> int64` | the number of bytes, without the NUL |
| `nextChar(@utf8 s, @!int64 pos) -> int32` | the code point at byte `pos`, moving `pos` past it; `-1` for an invalid part |
| `charCount(@utf8 s) -> int64` | the number of code points, an invalid part counting as one |
| `append(@!utf8 s, @utf8 t)` | adds `t`, by the rules of `s`'s mode |
| `append(@!utf8 s, @[]uint8 cs)` | adds the NUL-terminated bytes `cs`, by the rules of `s`'s mode |
| `appendChar(@!utf8 s, int32 c)` | adds the code point `c`; a surrogate, a value beyond U+10FFFF or a negative one is an invalid part, which `KEEP_INVALID` keeps as nothing |
| `replaced(@utf8 s) -> utf8` | a copy in `REPLACE_INVALID` mode with each invalid part replaced, keeping `hadInvalid` |

A function that only reads is called as `s -> f()`, and one that changes `s` as `s.f()`.
`bytes`, `hadInvalid` and `mode` are fields that can be read; writing them directly can break
the NUL at the end or the meaning of `hadInvalid` (see doc/Issues.md).
