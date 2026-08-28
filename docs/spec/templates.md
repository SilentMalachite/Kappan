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
    "description": "..."
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

同梱ファイルは `base.html` / `post.html` / `page.html` / `index.html` / `tag.html`。`landing.html` は Phase 7。
