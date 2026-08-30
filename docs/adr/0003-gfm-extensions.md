# ADR-0003: cmark-gfm extensions and raw HTML

> 日本語版: [`docs/ja/adr/0003-gfm-extensions.md`](../ja/adr/0003-gfm-extensions.md)

- Status: Accepted
- Date: 2026-08-28

## Context

Phase 1 converts a single Markdown file to HTML. Blog authors use tables and strikethrough, and occasionally write raw HTML in the body. Emitting dangerous tags verbatim would hand an attacker a foothold for XSS.

## Decision

- The parser is **cmark-gfm**. Of the core extensions we enable `table`, `strikethrough`, `autolink`, `tasklist`, and `tagfilter`.
- The options are `CMARK_OPT_UNSAFE` (keeps raw HTML and some dangerous URLs), `CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE` (`~~`), and `CMARK_OPT_GITHUB_PRE_LANG`.
- `CMARK_OPT_FOOTNOTES` is not a core extension, so it is out of scope for v1.
- Invalid UTF-8 never reaches cmark: it becomes `ErrorCode::Utf8` at read time, rather than relying on the replacement characters of `CMARK_OPT_VALIDATE_UTF8`.
- `tagfilter` neutralises dangerous HTML tags. XSS through the raw HTML that remains is the site author's responsibility, and the README and spec say so.

## Consequences

- GFM tables and `~~strikethrough~~` are pinned by tests.
- We cover the core of GitHub Flavored Markdown without adding a dependency.
