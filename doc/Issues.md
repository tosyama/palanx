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
