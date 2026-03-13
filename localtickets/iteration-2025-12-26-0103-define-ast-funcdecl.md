# Task: Define AST Node — Function definition — 2025-12-26T16:07:29.120Z

ID: IT-2025-12-26-0103

Description:
- Define and document an AST node to represent C function declarations (e.g., printf) so palan-c2ast can emit, palan-gen-ast can embed, and palan-sa can type-check them.

Acceptance Criteria:
- (Done) doc/ASTSpec.md updated with Function node specification and examples.
- palan-c2ast emits Function nodes for parsed C declarations (printf example).
- palan-gen-ast supports embedding Function nodes when merging C AST into Palan AST output.
- palan-sa accepts Function nodes and can mark them as extern symbols callable from Palan.
- Unit tests added: c2ast outputs Function nodes for stdio.h stub; gen-ast/sa tests validate presence and basic typing.

Owner: gen-ast / c2ast / sa Maintainers
Status: Closed
ClosedAt: 2026-01-04T15:45:00.194Z
ClosedReason: Superseded by later tickets (0104-0110) which implement parsing, embedding, and semantic handling of C function declarations; closing 103 to avoid duplication.
Estimate: 0.5d
