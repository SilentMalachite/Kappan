# ADR-0006: How static assets and feeds are laid out

> 日本語版: [`docs/ja/adr/0006-output-assets-feeds.md`](../ja/adr/0006-output-assets-feeds.md)

- Status: Accepted
- Date: 2026-08-29

## Context

Phase 5 makes the output directly publishable. HTML writing is concentrated in `content/build.cpp`, and there is no asset copying, `sitemap.xml`, or RSS yet. We want to add them as the last stage of the pipeline without growing the public headers.

## Decision

- Writing, copying, and XML generation live in `src/output/`. `content::build_site` stays an orchestrator that merely calls the later stages.
- Static files are copied verbatim from `<source>/static/` to the output root, the way Hugo does it. Non-Markdown files inside `content/` are not copied. Images are not transformed.
- The feed is a single RSS 2.0 file at `/feed.xml`. No Atom.
- When `url` in `site.yaml` is empty, no sitemap and no RSS are written. This is not an error, so the default produced by `kappan new` keeps working.
- `<loc>` and `<link>` are joined as UTF-8. Japanese URLs are not percent-encoded.
- `--out` is emptied before writing. If `--out` equals the source root, or the source lives inside `--out`, the command fails with `ErrorCode::Cli`.
- The public headers (`Config` and friends) do not change. `source_root / "static"` is enough to locate `static/`.

## Consequences

- For example, `static/images/🐙.svg` → `_site/images/🐙.svg`.
- `examples/blog` (which sets `url`) gets `sitemap.xml` and `feed.xml` in its golden files.
- Collisions are `ErrorCode::Path`, never an overwrite.
