# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# ビルド + テスト実行
make

# クリーン
make clean

# カバレッジレポート生成
make coverage
```

**依存パッケージ:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

### テスト個別実行

```bash
cd build && bin/c2ast-tester       # C→AST変換のテスト
cd build && bin/gen-ast-tester     # Palanパーサーのテスト
cd build && bin/build-mgr-tester   # ビルドマネージャーのテスト
```

### ツール単体実行

```bash
cd build && bin/palan-c2ast -d ../test/testdata/000_temp_c_header.h   # プリプロセッサ出力
cd build && bin/palan-c2ast -ds stdio.h                                # 標準ヘッダーのAST化
```

## アーキテクチャ概要

Palanは独立した4つのCLIツールが連携するマルチステージコンパイラ。ツール間の通信にはJSON形式のASTを使用する。

### コンパイルパイプライン

```
Palanソース
    ↓
[palan-gen-ast]  PlnLexer.ll + PlnParser.yy → JSON AST生成
    ↓
[palan-c2ast]    Cヘッダー → JSON AST変換（import用）
    ↓
[palan-sa]       意味解析（型チェック、スコープ解決）
    ↓
[palan]          ビルドマネージャー（上記を統括）
```

### ディレクトリ構成

```
src/
├── build-mgr/      palan 本体（ビルド統括）
├── c2ast/          palan-c2ast（Cヘッダー解析）
│   ├── clexer.*        Cレキサー（手書き）
│   ├── cparser.*       Cパーサー（再帰降下）
│   ├── cpreprocessor.* Cプリプロセッサ（マクロ展開・include処理）
│   ├── ctoken.*        トークン定義
│   └── predefined.h    定義済みマクロ群
├── gen-ast/        palan-gen-ast（Palanパーサー）
│   ├── PlnLexer.ll     Flex字句解析ルール
│   └── PlnParser.yy    Bisonパーサールール
├── semantic-anlyzr/ palan-sa（意味解析）
└── common/         共通ユーティリティ
test/
├── c2ast-tester/   c2astのGoogle Testスイート
├── gen-ast-tester/ gen-astのGoogle Testスイート
├── build-mgr-tester/ build-mgrのGoogle Testスイート
├── test-base/      テスト共通基盤（execTestCommand等）
└── testdata/       テスト入力ファイル
doc/
├── SpecAndDesign.md  言語仕様・ツール設計
└── ASTSpec.md        JSON AST形式仕様
lib/json/           nlohmann/json（ヘッダーオンリー）
localtickets/       ローカルタスク管理（Markdown）
```

### AST出力先

ビルド成果物は `~/.palan/work/` 以下にソースパスを反映した構造で格納される:
- Palanソース: `<file>.pa.ast.json`
- Cヘッダー変換: `<file>.pa#<header>.ast.json`
- 意味解析済: `<file>.pa.sa.json`

## テスト方針

- フレームワーク: Google Test (CMake FetchContent で自動取得、v1.12.0)
- カバレッジ目標: 全モジュール95%以上
- 生成ファイル（PlnLexer.cpp, PlnParser.cpp/h）はカバレッジ対象外

## タスク管理

`localtickets/` にMarkdownファイルでローカルタスクを管理する。
