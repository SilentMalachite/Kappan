# テンプレート

> English (canonical): [`docs/spec/templates.md`](../../spec/templates.md)

`layout:` の値がテンプレートファイル名になる。`post` → `post.html`。共通骨格は `base.html` で、`{% extends "base.html" %}` と `{% block content %}` を使う。

探索順:

1. `<source>/templates/<layout>.html`（あれば上書き）
2. バイナリに埋め込んだ `themes/default/`

欠けた layout は `ErrorCode::Template`。メッセージにどのファイルが無いかを書く。

inja に渡す JSON のルート:

```json
{
  "site": { "title": "...", "url": "...", "language": "ja", "description": "..." },
  "page": {
    "title": "...",
    "date": "2026-01-01",
    "date_display": "2026年1月1日",
    "permalink": "/posts/hello/",
    "content": "<p>...</p>",
    "tags": [{ "name": "日本語", "slug": "日本語", "permalink": "/tags/日本語/" }],
    "layout": "post",
    "description": "...",
    "image": "/images/og.svg",
    "sections": [],
    "generated_listing": false,
    "og": {
      "title": "こんにちは — 活版ブログ",
      "description": "...",
      "type": "article",
      "url": "https://example.com/posts/hello/",
      "image": "https://example.com/images/og.svg",
      "twitter_card": "summary_large_image"
    }
  },
  "collections": {
    "posts": [{ "title": "...", "permalink": "/posts/hello/", "date": "2026-01-01" }],
    "pages": [{ "title": "概要", "permalink": "/about/" }]
  },
  "pagination": {
    "page": 1,
    "pages": 2,
    "prev": null,
    "next": "/page/2/",
    "posts": []
  },
  "tag": { "name": "日本語", "slug": "日本語", "permalink": "/tags/日本語/" }
}
```

`page.content` は本文 HTML で、エスケープしない。`title` など本文以外はエンジンが HTML エスケープする。自動エスケープはオフ。`pagination` は一覧とタグページに載せる。`tag` はタグページだけ。

LP 用と OGP 用の変数は次のとおり。詳しくは [Landing pages](landing.md)。

| 変数 | 型 | 内容 |
|---|---|---|
| `page.image` | string | front matter の `image`。無ければ空 |
| `page.sections` | object[] | `type` / `eyebrow` / `title` / `text` / `image` / `actions` / `items` を持つ。無ければ空配列 |
| `page.generated_listing` | boolean | 自動生成した一覧ページだけ `true`。document とタグページは `false` |
| `page.og.title` | string | title が空、または自動一覧の 1 ページ目なら `site.title`。それ以外は `{page.title} — {site.title}` |
| `page.og.description` | string | `page.description`。無ければ `site.description` |
| `page.og.type` | string | `page.layout` が `post` なら `article`、それ以外は `website` |
| `page.og.url` | string | `site.url` と `page.permalink` を連結した絶対 URL。`site.url` が空なら空 |
| `page.og.image` | string | 絶対 URL 化した `page.image`。絶対化できなければ空 |
| `page.og.twitter_card` | string | `page.og.image` が空でなければ `summary_large_image`、空なら空 |

`page.og` は landing 専用ではなく、全レイアウト（`post` / `page` / `index` / `tag` / `landing`）に入る。一覧とタグページでは `page.image` は空、`page.sections` は空配列、`page.og.type` は `website` になる。`base.html` は `page.og` を読むだけで、値が空のときは meta 要素ごと出さない。

`content/index.md` が無い場合、自動生成する home は custom template との互換性のため
`page.title` を `site.title` と同じ値に保ち、`page.generated_listing` を `true` にする。
1 ページ目では default theme の `<title>` と `page.og.title` のどちらもサイト名を 1 回だけ出す。
2 ページ目以降は `ページ N — {site.title}` にする。明示的な `content/index.md` は
`page.generated_listing` が `false` の document なので、title にサイト名の suffix を付ける。
通常の document とタグページも、title やタグ名が `site.title` と同じ場合を含めて suffix を維持し、
たとえば `サイト — サイト` とする。

空文字の判定は `{% if page.og.url != "" %}` のように明示的に比較する。inja の `{% if %}` は
文字列も配列も `empty()` なら偽として扱う（`renderer.hpp` の `truthy` が最後に `!empty()` を返す）ため、
`{% if page.og.url %}` でも空文字は省ける。それでも明示比較を採るのは、値が文字列であることを
テンプレート側から読み取れるようにするため。配列は空なら偽になるので、`{% if section.actions %}`
はそのまま書いてよい。

同梱ファイルは `base.html` / `post.html` / `page.html` / `index.html` / `tag.html` / `landing.html`。
