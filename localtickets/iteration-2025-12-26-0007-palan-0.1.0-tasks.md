# Implementation Plan: Palan v0.1.0 — 2025-12-26T15:35:35.358Z

ID: IT-2025-12-26-0007

Objective:
- Ship Palan v0.1.0 which enables importing the C header <stdio.h> and calling printf from Palan programs for improved debugging and deterministic unit-test output.

Ordered tasks (minimal, executable order):
1. [DONE] Project kickoff & criteria (IT-2025-12-26-0101, 0.25d)
   - Confirm scope: only <stdio.h> import and printf call semantics; define acceptance tests.

2. [DONE] Predefined C macros & config (IT-2025-12-26-0102, 0.25d)
   - Ensure src/c2ast/predefined.h and c2ast search-path logic include necessary macros and std include paths.

3. [DONE] C-to-AST translator enhancements (IT-2025-12-26-0103/0104, palan-c2ast, 0.5d)
   - Verify palan-c2ast parses relevant parts of stdio.h (declarations for printf) and serializes compatible AST nodes.

4. [DONE] AST generator: cinclude handling (IT-2025-12-26-0105, palan-gen-ast, 0.5d)
   - invoke palan-c2ast on cinclude and embed C function prototypes into the cinclude stmt node.

5. gen-ast: function call AST node (IT-2025-12-26-0111, palan-gen-ast, 0.5d)
   - Output call stmts (name + args) for function call expressions instead of not-impl.

6. Semantic analyzer adjustments (IT-2025-12-26-0106, palan-sa, 0.5d)
   - Register C functions from cinclude into scope; annotate resolved call stmts with func-type: "c".

7. Code generator (IT-2025-12-26-0112, palan-codegen, 2.0d)
   - Generate x86-64 assembly from sa.json; assemble with as and link with ld.

8. Build manager: pipeline integration (IT-2025-12-26-0107, 0.5d)
   - Wire palan-gen-ast -> palan-sa -> palan-codegen -> as -> ld in the build manager.

9. Tests: unit and integration (IT-2025-12-26-0108, 0.5d)
   - Unit tests for all modules; integration test running Hello World binary.

10. Documentation & examples (IT-2025-12-26-0109, 0.25d)
    - Update README and doc/SpecAndDesign.md.

11. Release prep (IT-2025-12-26-0110, 0.25d)
    - Tag v0.1.0, draft release notes.

Acceptance Criteria:
- palan-gen-ast can import <stdio.h>-derived AST nodes via palan-c2ast.
- palan-sa accepts and type-checks calls to printf originating from C header imports.
- An integration test compiles and runs a Palan program that calls printf and produces expected output (or a captured equivalent) on CI.

Risks & mitigations:
- Real system stdio.h may be large/complex: mitigate by using a minimal stdio.h stub for tests and providing a way to specify sysinclude paths.
- Name/type mismatches between C declarations and Palan types: provide clear mapping rules and fallbacks to treat externs conservatively.

Owners: Compiler team; suggested smaller owners per task in commit message.
Total estimate: ~3.5d

Next step: create per-task tickets and wire CI integration if you want granular tracking.