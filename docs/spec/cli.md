# CLI

`serve` は後のフェーズで足す。

## 共通

```
kappan --help
kappan --version
kappan --verbose <subcommand>
```

`--version` の値は CMake の `project(kappan VERSION ...)` と一致する。`--verbose` は spdlog を debug にする。

## `kappan build`

サイト根ディレクトリを読み、`content/` 以下の Markdown を HTML にする。

```
kappan build --source <site-root> --out <dir> [--drafts]
```

- `--source` は `site.yaml` のあるディレクトリ。ファイルを渡すと使い方を示して `ErrorCode::Cli` で終了する。
- `site.yaml` が無い・壊れている場合は行番号付き `ErrorCode::Config`。
- 1 ファイルの失敗で止めない。エラーを集約して最後に報告し、1 件でもあれば終了コードは非 0。
- 出力は pretty URL（`about.md` → `about/index.html`）。`layout` に対応するテンプレートで体裁を付ける。
- `--drafts` が無いとき `draft: true` の記事は出力しない。
- 入力の UTF-8 BOM は読み捨て、出力には付けない。CRLF は LF に正規化する。

## `kappan new`

```
kappan new <dir>
```

空のディレクトリに `site.yaml`、日本語サンプル記事、同梱テーマの `templates/` を書き出す。既に空でないディレクトリなら `ErrorCode::Cli`。

## 終了コード

想定内エラーは「どのファイルの、何が、どうダメか」を書いて非 0。
