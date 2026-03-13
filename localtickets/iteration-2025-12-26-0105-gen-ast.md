# Task: AST Generator Import Handling (palan-gen-ast) — 2025-12-26T15:39:23.416Z

ID: IT-2025-12-26-0105

Description:
- Add/confirm logic in palan-gen-ast to detect imports of C headers, invoke palan-c2ast, and embed C AST nodes into the Palan AST output.

Acceptance Criteria:
- palan-gen-ast invokes palan-c2ast for C imports and embeds resulting AST nodes.
- Unit tests assert that generated AST for a sample source contains the printf declaration node.

Owner: gen-ast Maintainer
Status: Closed
Estimate: 0.5d
