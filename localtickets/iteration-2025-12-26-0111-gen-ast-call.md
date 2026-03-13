# Task: gen-ast Function Call AST Node — 2026-03-11T00:00:00.000Z

ID: IT-2025-12-26-0111

Description:
- palan-gen-ast で関数呼び出し文を `not-impl` ではなく `call` ステートメントとして AST に出力する。
- 関数名と引数リスト（文字列リテラル、識別子等）を JSON ノードとして表現する。

Acceptance Criteria:
- `printf("Hello World!")` を含む Palan ソースを palan-gen-ast に通すと、
  `stmt-type: "call"` ノードが `name: "printf"` および `args` を持って出力される。
- 対応するユニットテストが通る。

Owner: gen-ast Maintainer
Status: Closed
Estimate: 0.5d
