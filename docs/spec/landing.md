# Landing pages

> 日本語版: [`docs/ja/spec/landing.md`](../ja/spec/landing.md)

Phase 7. A landing page is an ordinary Markdown page that carries `layout: landing`. There is no dedicated CLI subcommand and no dedicated pipeline (see [ADR-0010](../adr/0010-landing-frontmatter.md)).

- The output path follows the existing pretty-URL rules: `content/index.md` becomes `/`, `content/lp.md` becomes `/lp/`.
- The template is `landing.html`. A site-local `<source>/templates/landing.html` takes precedence (see [Templates](templates.md)).
- `sections` is structured data for the template. It can be combined with the `page.content` body; in the bundled template the body appears after the sections.
- The bundled `landing.html` does not branch on `sections[].type`. Since the value is never rejected, a site-local template is free to branch on it.
- `image` becomes a candidate for the OGP `og:image`.
- `examples/landing` builds with `kappan build --source examples/landing --out <dir>`.

For the front matter format, see [Front matter](front-matter.md).

## OGP

`page.og` is assembled by `src/render/context.cpp`. It is present in every layout (`post`, `page`, `index`, `tag`, `landing`), and `base.html` only reads it — it makes no decisions of its own.

| Field | Rule |
|---|---|
| `og.title` | `{page.title} — {site.title}` when `page.title` is non-empty, otherwise `site.title`. The same expression as `<title>` in the bundled template |
| `og.description` | `page.description` when non-empty, otherwise `site.description` |
| `og.type` | `article` when `page.layout` is `post`, otherwise `website` |
| `og.url` | The absolute URL formed from `site.url` and `page.permalink` when `site.url` is non-empty, otherwise empty |
| `og.image` | `page.image` as-is when it starts with `http://` or `https://`. Made absolute when it starts with `/` and `site.url` is set. Otherwise (relative or empty) empty |
| `og.twitter_card` | `summary_large_image` when `og.image` is non-empty, otherwise empty |

Absolute URLs are built with the same rule as sitemap and feed: trailing slashes are collapsed before the permalink is appended, and Japanese characters are not percent-encoded (see [ADR-0006](../adr/0006-output-assets-feeds.md)).

When `site.url` is empty, `og:url` and relative `og:image` are omitted. Rather than writing a `meta` with an empty value, `base.html` drops the whole element with an `{% if %}` — a broken relative URL must never be handed to whoever the page is shared with.

## Example output

```html
<meta property="og:title" content="日本語LPを静的生成 — 活版ランディング">
<meta property="og:description" content="Markdown と YAML から、日本語に強いランディングページを生成します。">
<meta property="og:type" content="website">
<meta property="og:url" content="https://example.com/">
<meta property="og:image" content="https://example.com/images/og.svg">
<meta name="twitter:card" content="summary_large_image">
```

## Out of scope for v1

- Generating OG images, and resizing or converting images.
- A fixed enumeration of, and validation for, `sections[].type`.
- A landing-specific CLI subcommand or output path.
