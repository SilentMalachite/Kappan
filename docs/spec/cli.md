# CLI

## 共通

```
kappan --help
kappan --version
kappan --verbose <subcommand>
```

`--version` の値は CMake の `project(kappan VERSION ...)` と一致する。`--verbose` は spdlog を debug にする。

## `kappan build`

サイト根ディレクトリを読み、`content/` 以下の Markdown を HTML にする。`static/` をコピーし、`url` があれば `sitemap.xml` と `feed.xml` を書く。

```
kappan build --source <site-root> --out <dir> [--drafts] [--force]
```

- `--source` は `site.yaml` のあるディレクトリ。ファイルを渡すと使い方を示して `ErrorCode::Cli` で終了する。
- `site.yaml` が無い・壊れている場合は行番号付き `ErrorCode::Config`。
- 1 ファイルの失敗で止めない。エラーを集約して最後に報告し、1 件でもあれば終了コードは非 0。
- 出力は pretty URL（`about.md` → `about/index.html`）。`layout` に対応するテンプレートで体裁を付ける。
- `--drafts` が無いとき `draft: true` の記事は出力しない。
- 入力の UTF-8 BOM は読み捨て、出力には付けない。CRLF は LF に正規化する。
- 書き出し前に `--out` を空にする。`--out` がソース根と同じ、またはソースが `--out` の内側なら `ErrorCode::Cli`。
- `--out` が空でなく、kappan の出力先である印（`.kappan-out`）も無い場合は、**何も消さずに** `ErrorCode::Cli` で拒否する。`--force` を付けたときだけ消す。`--force` は上の 2 つの判定には効かない（ソースは常に守る）。
- 詳細は [output.md](output.md)。

## `kappan serve`

生成結果を loopback HTTP で配信する。`--watch` は後のフェーズで足す。

```
kappan serve --source <site-root> [--host 127.0.0.1] [--port 8080] [--drafts]
```

- `--source` は必須。`site.yaml` のあるディレクトリ。
- `--host` の既定値は `127.0.0.1`。明示指定なしに LAN へ公開しない。
- `--port` の既定値は 8080。CLI 上の範囲は `1..65535`。
- `--drafts` が無いとき `draft: true` の記事は出さない。
- 初回のサイト生成に失敗したら待ち受けせず、エラーを報告して非 0 で終了する。
- bind に失敗したら host と port を含む `ErrorCode::Io` で非 0 終了する。
- `Ctrl-C`（SIGINT、および SIGTERM）で待ち受けを止め、HTTP thread を join し、一時 workspace を回収する。signal handler は停止フラグを立てるだけであり、stop・ログ・filesystem・mutex は呼ばない。
- `serve` は `<source>/_site` を作らない・変更しない。生成物は OS の一時ディレクトリへ書く。

## `kappan new`

```
kappan new <dir>
```

空のディレクトリに `site.yaml`、日本語サンプル記事、同梱テーマの `templates/` を書き出す。既に空でないディレクトリなら `ErrorCode::Cli`。

## 終了コード

想定内エラーは「どのファイルの、何が、どうダメか」を書いて非 0。
