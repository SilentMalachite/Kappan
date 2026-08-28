# Front matter

先頭行が `---` のとき、次の `---` までを YAML、残りを Markdown 本文とする。CRLF は読み込み時に LF へ正規化済み。`---` が無いファイルは front matter なし。閉じ `---` が無い・YAML が壊れている場合は落ちずに `ErrorCode::FrontMatter` を集約する。

| キー | 型 | 既定 |
|---|---|---|
| `title` | string | 日付プレフィックス除去後の stem |
| `date` | string | ファイル名の `YYYY-MM-DD-`。無ければ空。ISO 8601 日付または `YYYY-MM-DDTHH:MM:SS` |
| `layout` | string | `content/posts/` 配下なら `post`、それ以外は `page` |
| `slug` | string | stem（日付プレフィックス除去）→ だめなら title から `slugify` |
| `draft` | bool | `false`（既定では出力しない。`build --drafts` で含める） |
| `tags` | string[] | 空 |
| `description` | string | `""` |

`date` は yaml-cpp の日付型に頼らず、スカラー文字列を自分でパースする。不正値の例:

`content/posts/2026-01-01-hello.md:3 front matter の 'date' が日付として解釈できません: '2026-13-01'`

`image` と `sections` は Phase 2 の `FrontMatter` に持たない。
