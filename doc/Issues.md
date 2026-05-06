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

## 5. Unsigned Integer Literal Suffix (`u`) Not Implemented

**Summary:** The lexer defines `UDIGIT = [0-9]+"u"` and the parser has a `UINT` token and `lit-uint` expression type, but no Flex action rule returns `UINT`. As a result, `123u` syntax is not recognized — `u` is tokenized as a separate identifier and causes a syntax error.

The `uint64` type itself is supported for variable declarations and arithmetic, but there is no way to write a `uint64` literal that exceeds `int64` max (`9223372036854775807`) in source code.

**To fix:** Add `<*>{UDIGIT} { lval.build<string>() = string(yytext, yyleng-1); return UINT; }` to `PlnLexer.ll` (stripping the `u` suffix from the stored value).
