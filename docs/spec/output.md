# Output (static assets, sitemap, RSS)

> 日本語版: [`docs/ja/spec/output.md`](../ja/spec/output.md)

Phase 5. Writes the rendered result to `--out` and adds the files needed for publishing. A later stage never mutates an earlier one (`Site` / `RenderedPage`).

## Preparing the output directory

`--out` is emptied (or created) after configuration, scanning, and parsing have finished, immediately before the HTML is written — never while a configuration error is still possible. The following are rejected with `ErrorCode::Cli`, so that the source is never deleted:

- `--out` is the same as `--source`
- `--source` lives inside `--out`

`--out` inside `--source` (`examples/blog/_site`) is fine.

Before deleting, kappan checks that `--out` really is one of its own output directories. It writes `.kappan-out` (a fixed single line) at the output root and uses it to decide:

| State of `--out` | Behaviour |
|---|---|
| Missing | Create it |
| Empty | Use it as-is |
| Non-empty with `.kappan-out` | Delete and recreate |
| Non-empty without `.kappan-out` | **Delete nothing** and fail with `ErrorCode::Cli`. Only `--force` deletes |

`.kappan-out` is written immediately after `create_directories`, so that a build failing part-way through does not cause the next run to be refused. See [ADR-0007](../adr/0007-out-dir-deletion-policy.md).

## Static assets

`<source>/static/` is copied to the output root, preserving relative paths.

| Input | Output |
|---|---|
| `static/css/site.css` | `_site/css/site.css` |
| `static/images/🐙.svg` | `_site/images/🐙.svg` |

- A missing or empty `static/` is not an error. The bundled theme contributes no static files.
- Directories starting with `_` are not traversed (same as `content/`).
- Files and directories starting with `.` are not copied, so `.DS_Store` and `.git/` never reach the output.
- Contents are copied as raw bytes: no UTF-8 validation and no newline normalisation, so images and fonts stay intact.
- Japanese filenames are preserved as-is.

## sitemap.xml

When `url` in `site.yaml` is non-empty, `sitemap.xml` is written at the output root. When it is empty, nothing is written and this is not an error.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>https://example.com/posts/こんにちは/</loc>
    <lastmod>2026-01-01</lastmod>
  </url>
</urlset>
```

- Every generated HTML page is included: posts, pages, home, `/page/N/`, and tags. A permalink appears only once. Drafts are not in `Site`, so they never appear.
- `<loc>` joins `url` (with its trailing `/` removed) and the permalink. It stays UTF-8 and is not percent-encoded. XML special characters are escaped.
- `<lastmod>` is emitted only when the Document has a `date`. It uses the **W3C Datetime** format sitemaps.org requires: either a date (`YYYY-MM-DD`) or a UTC timestamp with `Z` (`YYYY-MM-DDThh:mm:ssZ`). A timestamp without a TZD is invalid and is not emitted. Pages that are not Documents, such as listings and tags, omit the element entirely.
- `<url>` entries are sorted by permalink ascending, which keeps the golden files stable.
- No BOM.

## feed.xml (RSS 2.0)

When `url` is non-empty, `feed.xml` is written at the output root. When it is empty, nothing is written.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0">
  <channel>
    <title>活版ブログ</title>
    <link>https://example.com/</link>
    <description>日本語と絵文字 🐙 を含むサイト</description>
    <language>ja</language>
    <item>
      <title>こんにちは</title>
      <link>https://example.com/posts/こんにちは/</link>
      <guid isPermaLink="true">https://example.com/posts/こんにちは/</guid>
      <pubDate>Thu, 01 Jan 2026 00:00:00 +0000</pubDate>
      <description>最初の記事です。</description>
    </item>
  </channel>
</rss>
```

- `item` covers `posts` only, in the same order as the collection (`date` descending, undated last). There is no item limit. When there are no posts, the channel is written without any `item`.
- `channel/link` is `url` + `/`. When `channel/description` is empty, `title` is used, because RSS 2.0 requires it.
- `item/description` is the front matter `description`, falling back to the body HTML. Both are XML-escaped; CDATA is not used.
- `pubDate` is RFC 822. The time is the UTC of `date` and the offset is always `+0000`. Weekday and month names are fixed to English so they do not depend on the locale. When `date` is absent, the element is omitted.
- Pages, tags, and drafts are excluded.

## Collisions

When generated HTML, `sitemap.xml`, `feed.xml`, and `static/` resolve to the same output path, nothing is silently overwritten: `ErrorCode::Path` is collected instead.

When `url` is empty, a user is free to ship their own `static/sitemap.xml` or `static/feed.xml`.

## Counting

`pages_written` counts HTML only. Assets and XML are not included.
