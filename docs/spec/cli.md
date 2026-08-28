# CLI

Phase 1 時点のコマンド。`serve` と `new` は後のフェーズで足す。

## 共通

```
kappan --help
kappan --version
kappan --verbose <subcommand>
```

`--version` の値は CMake の `project(kappan VERSION ...)` と一致する。`--verbose` は spdlog を debug にする。

## `kappan build`

Markdown 1 ファイルを HTML に変換する。`site.yaml` は不要。

```
kappan build --source <file.md> --out <dir>
```

- `--source` は通常のファイルであること。ディレクトリを渡すと `ErrorCode::Cli` で終了する。
- `--out` は出力ディレクトリ。無ければ作成する。成果物は `<dir>/<stem>.html`。
- 入力の UTF-8 BOM は読み捨て、出力には付けない。
- CRLF は LF に正規化してから変換する。
- かな・漢字・絵文字・日本語ファイル名をそのまま扱う。

Phase 2 以降、`--source` はサイト根ディレクトリ（`site.yaml` 必須）に変わる。ファイル直指定はエラーで使い方を示す。

## 終了コード

想定内エラー（読めないファイル、不正 UTF-8 など）はメッセージを出して非 0。メッセージは「どのファイルの、何が、どうダメか」を含む。
