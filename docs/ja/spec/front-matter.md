# Front matter

> English (canonical): [`docs/spec/front-matter.md`](../../spec/front-matter.md)

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
| `image` | string | `""` |
| `sections` | map[] | 空 |

`date` は yaml-cpp の日付型に頼らず、スカラー文字列を自分でパースする。不正値の例:

`content/posts/2026-01-01-hello.md:3 front matter の 'date' が日付として解釈できません: '2026-13-01'`

`image` と `sections` は LP 用（[Landing pages](landing.md)）。`image` は OGP の `og:image` 候補になり、`sections` はテンプレートへ渡す構造化データになる。ブログ記事で使ってもよい。

`sections` は「マップの配列」。各要素の既知キーは `type` / `eyebrow` / `title` / `text` / `image` / `actions` / `items`。`actions` は `{ label, href }` の配列、`items` は `{ title, text, icon }` の配列で、値はすべて文字列。省略した文字列は空、省略した配列は空になる。未知キーは無視する。`type` の値は拒否しない。

```yaml
sections:
  - type: hero
    eyebrow: Kappan
    title: 日本語LPを静的生成
    text: Markdown と YAML だけで公開できます 🐙
    actions:
      - label: 機能を見る
        href: "#features"
  - type: features
    items:
      - title: UTF-8
        text: かな・漢字・絵文字をそのまま扱います。
        icon: 文
```

既知キーの型が違う場合は行番号付き `ErrorCode::FrontMatter`。不正値の例:

`content/index.md:6 front matter の 'sections.actions' はマップの配列である必要があります`
