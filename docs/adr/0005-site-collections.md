# ADR-0005: The Site model and generated pages

> 日本語版: [`docs/ja/adr/0005-site-collections.md`](../ja/adr/0005-site-collections.md)

- Status: Accepted
- Date: 2026-08-29

## Context

Phase 4 adds collections, tags, and pagination. Templates receive a single root object, `Site` (see AGENTS.md), and a `Document` is never mutated after `Site` has been built.

## Decision

- `Site`, `Collection`, `Taxonomy`, and `DraftPolicy` live in `include/kappan/site.hpp`.
- `posts` is the Documents whose permalink starts with `/posts/`. `pages` is the rest, excluding the home page (`/`).
- The order is `date` descending, undated last, and same-day by `slug` ascending.
- `draft: true` documents are not in `Site` by default. They are included only under `DraftPolicy::Include` (CLI `--drafts`).
- The first listing page is `/`. When `content/index.md` exists, that Document is rendered with `layout: index` (the default) and carries the pagination. Otherwise a generated page is written at the same permalink. Later pages are generated at `/page/N/`.
- Tags are at `/tags/{slug}/`. v1 does not paginate tag pages.
- A generated permalink matching an existing Document is `ErrorCode::Path`. `/page/` (from `content/page.md`) and `/page/2/` are different paths, so they can coexist.
- `pagination.posts_per_page` defaults to 10. `0` means a single page with every post. A negative value is a Config error.

## Consequences

- Later stages only read `Site`. Excluding drafts and building listings are the responsibility of Site construction.
