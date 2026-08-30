# ADR-0004: Permalinks and Japanese slugs

> 日本語版: [`docs/ja/adr/0004-permalinks-and-slug.md`](../ja/adr/0004-permalinks-and-slug.md)

- Status: Accepted
- Date: 2026-08-29

## Context

Phase 2 decides where a `Document` is written. Romanising Japanese filenames and headings would require an extra library, and it would make it harder for readers to guess the content from the URL.

## Decision

- Use pretty URLs: `content/about.md` → `/about/index.html` (permalink `/about/`). Only the top page differs: `content/index.md` → `/index.html` (permalink `/`).
- Posts live in `content/posts/`. The `YYYY-MM-DD-` prefix is optional and acts as a fallback for the date. The permalink is `/posts/{slug}/`.
- The slug is taken, in order, from the front matter `slug:`, then the stem with the date prefix removed, then the title. ASCII is lowercased; whitespace (halfwidth and fullwidth) and characters reserved on Windows become `-`. Kana, kanji, and emoji are preserved.
- After that conversion, trailing dots are removed. If the ASCII-lowercased basename before the first dot is a Windows reserved device name (`con`, `prn`, `aux`, `nul`, `com1` through `com9`, or `lpt1` through `lpt9`), `_` is prefixed. This rule is applied on every OS. OS-specific branching is not used because the same content must produce deterministic URLs while remaining writable on Windows.
- No romanisation library is added. Unicode normalisation (NFC composition) would require ICU, so v1 skips it and uses the input UTF-8 as-is.
- Colliding permalinks are collected as `ErrorCode::Path`; nothing is silently overwritten.

## Consequences

- Japanese URLs can be pinned by tests.
- The `/page/N/` paths of Phase 4 can collide with `content/page.md`. A collision is a Config error (Phase 4).
