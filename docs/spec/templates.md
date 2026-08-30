# テンプレート

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
| `page.og.title` | string | `page.title` があれば `{page.title} — {site.title}`、無ければ `site.title` |
| `page.og.description` | string | `page.description`。無ければ `site.description` |
| `page.og.type` | string | `page.layout` が `post` なら `article`、それ以外は `website` |
| `page.og.url` | string | `site.url` と `page.permalink` を連結した絶対 URL。`site.url` が空なら空 |
| `page.og.image` | string | 絶対 URL 化した `page.image`。絶対化できなければ空 |
| `page.og.twitter_card` | string | `page.og.image` が空でなければ `summary_large_image`、空なら空 |

`page.og` は landing 専用ではなく、全レイアウト（`post` / `page` / `index` / `tag` / `landing`）に入る。一覧とタグページでは `page.image` は空、`page.sections` は空配列、`page.og.type` は `website` になる。`base.html` は `page.og` を読むだけで、値が空のときは meta 要素ごと出さない。

空文字の判定は `{% if page.og.url != "" %}` のように明示的に比較する。inja の `{% if %}` は
文字列を常に真として扱うため、`{% if page.og.url %}` では空文字を省けない。配列は空なら偽になるので、
`{% if section.actions %}` はそのまま書いてよい。

同梱ファイルは `base.html` / `post.html` / `page.html` / `index.html` / `tag.html` / `landing.html`。
