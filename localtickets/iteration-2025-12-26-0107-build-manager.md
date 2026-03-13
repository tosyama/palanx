# Task: Build Manager / Runtime Linking Considerations — 2025-12-26T15:39:23.416Z

ID: IT-2025-12-26-0107

Description:
- ビルドマネージャ（palan）のパイプラインに palan-codegen を組み込む。
- palan-gen-ast -> palan-sa -> palan-codegen -> as -> ld の順で実行し、実行可能バイナリを生成する。

Acceptance Criteria:
- `palan <source.pa>` を実行すると実行可能バイナリが生成される。
- `printf("Hello World!\n")` を含む Palan プログラムをビルドして実行すると期待出力が得られる統合テストが通る。

Owner: build-mgr Maintainer
Status: Closed
Estimate: 0.5d
