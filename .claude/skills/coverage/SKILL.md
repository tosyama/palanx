---
name: coverage
description: Use this skill when the user mentions coverage, カバレッジ, uncovered lines, wants to check or improve test coverage for a specific file or component, or asks to run coverage-related make targets.
---

## Coverage Build Directory

Coverage builds use `build-cov/` (separate from `build/`). `build-cov/` persists between runs — only `.gcda` files are cleared before each run, so incremental builds are fast after the first.

## Make Targets

| Target | Tests run | lcov filter | When to use |
|--------|-----------|-------------|-------------|
| `make coverage` | all testers | `src/*` | Full report; PR review, overall health check |
| `make coverage-codegen` | codegen-tester + build-mgr-tester | `src/codegen/*` | Working on PlnX86CodeGen, PlnVCodeGen, PlnRegAlloc |
| `make coverage-sa` | sa-tester + sa-unit-tester | `src/semantic-anlyzr/*` | Working on PlnSemanticAnalyzer |
| `make coverage-reset` | — | — | Force full rebuild of build-cov/ (e.g., after CMakeLists change) |

Run **foreground only** — do not use `run_in_background: true` (prevents duplicate builds).

## Checking Coverage Results

**Overall summary** (totals only — reliable):
```bash
lcov --summary build-cov/lcov.info --rc branch_coverage=1
```

**Per-file line coverage** (parse lcov.info directly — `lcov --list` is unreliable with lcov 2.0):
```bash
awk '/^SF:/{f=$0} /^LF:/{lf=$0} /^LH:/{lh=$0} /^end_of_record/{
  sub(/^SF:/,"",f); sub(/^LF:/,"",lf); sub(/^LH:/,"",lh);
  printf "%-60s %s/%s\n", f, lh, lf}' build-cov/lcov.info
```

> **Note:** `lcov --list` in lcov 2.0 displays incorrect per-file percentages (e.g., 5.4% when actual is 90%). Use `lcov --summary` for totals and parse `lcov.info` directly for per-file stats.

## Viewing Uncovered Lines in a Specific File

Use `gcov` from inside `build-cov/` to avoid creating `.gcov` files in the repo root:

```bash
# Example: PlnX86CodeGen.cpp
cd build-cov && gcov -o src/codegen/CMakeFiles/palan-codegen.dir/ \
     ../src/codegen/PlnX86CodeGen.cpp.gcno
```

This writes `PlnX86CodeGen.cpp.gcov` inside `build-cov/`.
Read it with the Read tool — lines prefixed with `#####:` are uncovered.

**gcov object directory by component (paths relative to build-cov/):**

| Source file | -o path |
|-------------|---------|
| `src/codegen/*.cpp` | `src/codegen/CMakeFiles/palan-codegen.dir/` |
| `src/semantic-anlyzr/*.cpp` | `src/semantic-anlyzr/CMakeFiles/palan-sa.dir/` |
| `src/c2ast/*.cpp` | `src/c2ast/CMakeFiles/palan-c2ast.dir/` |
| `src/gen-ast/*.cpp` | `src/gen-ast/CMakeFiles/palan-gen-ast.dir/` |
| `src/build-mgr/*.cpp` | `src/build-mgr/CMakeFiles/palan.dir/` |

## Workflow

1. Run the appropriate `make coverage-*` target
2. Check per-file % by parsing `lcov.info` directly (see above)
3. For files with low coverage, run `gcov` from inside `build-cov/` and read the `.gcov` file to identify uncovered lines
4. Write tests targeting those lines, then re-run the coverage target
