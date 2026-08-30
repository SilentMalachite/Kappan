# Templates

> 日本語版: [`docs/ja/spec/templates.md`](../ja/spec/templates.md)

The value of `layout:` becomes the template filename: `post` → `post.html`. The shared skeleton is `base.html`, used through `{% extends "base.html" %}` and `{% block content %}`.

Lookup order:

1. `<source>/templates/<layout>.html` (overrides when present)
2. `themes/default/`, embedded in the binary

A missing layout produces `ErrorCode::Template`, with the missing filename in the message.

The root of the JSON handed to inja:

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

`page.content` is body HTML and is not escaped. Everything else, `title` included, is HTML-escaped by the engine. Auto-escaping is off. `pagination` is present on listing and tag pages; `tag` only on tag pages.

The landing and OGP variables are below. See [Landing pages](landing.md) for details.

| Variable | Type | Contents |
|---|---|---|
| `page.image` | string | The front matter `image`, or empty |
| `page.sections` | object[] | Holds `type`, `eyebrow`, `title`, `text`, `image`, `actions`, `items`. Empty array when absent |
| `page.generated_listing` | boolean | `true` only for an automatically generated listing page; `false` for documents and tag pages |
| `page.og.title` | string | `site.title` for an empty title or generated listing page 1; otherwise `{page.title} — {site.title}` |
| `page.og.description` | string | `page.description`, or `site.description` |
| `page.og.type` | string | `article` when `page.layout` is `post`, otherwise `website` |
| `page.og.url` | string | The absolute URL formed from `site.url` and `page.permalink`. Empty when `site.url` is empty |
| `page.og.image` | string | `page.image` made absolute, or empty when it cannot be |
| `page.og.twitter_card` | string | `summary_large_image` when `page.og.image` is non-empty, otherwise empty |

`page.og` is not landing-specific: it is present in every layout (`post`, `page`, `index`, `tag`, `landing`). On listing and tag pages, `page.image` is empty, `page.sections` is an empty array, and `page.og.type` is `website`. `base.html` only reads `page.og`, and drops the whole `meta` element when a value is empty.

When `content/index.md` is absent, the generated home keeps `page.title` equal to `site.title`
for compatibility with custom templates and sets `page.generated_listing` to `true`. On page 1,
the default theme emits the site title once in both `<title>` and `page.og.title`. Page 2 and later
use `Page N — {site.title}` (the localized generated title is `ページ N`). An explicit
`content/index.md` is a document with `page.generated_listing` set to `false`, so its title keeps
the site suffix. Normal documents and tag pages also keep the suffix even when their title or tag
name equals `site.title`, for example `Site — Site`.

Emptiness is tested with an explicit comparison such as `{% if page.og.url != "" %}`. inja's `{% if %}` already treats an `empty()` string or array as false (the `truthy` helper in `renderer.hpp` returns `!empty()` as its last resort), so `{% if page.og.url %}` would also skip an empty string. The explicit comparison is preferred so the template itself shows that the value is a string. Arrays are false when empty, so `{% if section.actions %}` can be written directly.

The bundled files are `base.html`, `post.html`, `page.html`, `index.html`, `tag.html`, and `landing.html`.
