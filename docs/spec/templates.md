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
    "permalink": "/posts/hello/",
    "content": "<p>...</p>",
    "tags": ["日本語"],
    "layout": "post",
    "description": "..."
  }
}
```

`page.content` は本文 HTML で、エスケープしない。`title` など本文以外はエンジンが HTML エスケープする。自動エスケープはオフ。

Phase 3 の同梱ファイルは `base.html` / `post.html` / `page.html`。`landing.html` は Phase 7。
