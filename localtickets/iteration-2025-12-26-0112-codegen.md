# Task: Code Generator — x86 Assembly Output (palan-codegen) — 2026-03-11T00:00:00.000Z

ID: IT-2025-12-26-0112

Description:
- palan-sa の出力（.sa.json）を受け取り、x86-64 アセンブリ（AT&T 記法）を生成する新ツール palan-codegen を実装する。
- 生成したアセンブリを `as` でオブジェクトファイルに変換し、`ld` でリンクして実行可能バイナリを生成する。
- 初期スコープ: `printf` による文字列出力（Hello World 相当）が動作すること。

Acceptance Criteria:
- `palan-codegen <sa.json>` を実行すると x86-64 アセンブリファイルが生成される。
- `as` / `ld` によるコンパイル・リンクが成功し、実行可能バイナリが得られる。
- `printf("Hello World!\n")` を含む Palan プログラムをコンパイルして実行すると
  `Hello World!` が標準出力に出力される統合テストが通る。

Owner: codegen Maintainer
Status: Closed
Estimate: 2.0d
