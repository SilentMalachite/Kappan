# ADR-0008: `<lastmod>` in the sitemap uses W3C Datetime

> 日本語版: [`docs/ja/adr/0008-sitemap-lastmod-format.md`](../ja/adr/0008-sitemap-lastmod-format.md)

- Status: Accepted
- Date: 2026-08-29
- Related: [ADR-0006](0006-output-assets-feeds.md)

## Context

`docs/spec/output.md` defined `<lastmod>` only as "an ISO date (`YYYY-MM-DD` or a timestamp), emitted when the Document has a `date`", and the implementation inserts the return value of `util::format_iso_datetime` verbatim. The implementation matches the spec; it is not a deviation. **The spec itself fails to meet an external requirement.**

Measured with `build/dev/kappan`:

```
An article with date: 2026-01-01T09:30:00Z
→ <lastmod>2026-01-01T09:30:00</lastmod>
(the <pubDate> of the same build is correct: Thu, 01 Jan 2026 09:30:00 +0000)
```

sitemaps.org requires W3C Datetime for `<lastmod>`. The complete-date-plus-hours-minutes-seconds form of W3C Datetime requires a TZD (`Z` or `±hh:mm`); validators reject a value without one, and crawlers that parse it treat the zone as undefined.

`YYYY-MM-DD` (the complete date form) needs no TZD and is correct. Every article in `examples/blog` is date-only and therefore takes that branch, so the golden files walked straight past this defect.

`format_iso_datetime` is shared with the `date` template variable, so fixing it there would also move the template output and the existing golden files.

## Decision

**Add a sitemap-specific formatter and leave `format_iso_datetime` alone.**

- Add `util::format_w3c_datetime(std::chrono::sys_seconds)` to `src/util/datetime.hpp`.
  - Exactly midnight → `YYYY-MM-DD` (the complete date form; no TZD needed)
  - Otherwise → `YYYY-MM-DDThh:mm:ssZ`
- The value is `sys_seconds`, i.e. UTC, so the offset is always `Z`. `±hh:mm` is never emitted, matching the way `format_rfc822` fixes `+0000`.
- `<lastmod>` in `render_sitemap` uses the new function.
- `format_iso_datetime` stays as it is, so the meaning of the `date` template variable and the existing golden files do not move.
- The `<lastmod>` line in `docs/spec/output.md` is rewritten accordingly.

## Consequences

- Every article in `examples/blog` is date-only, so not a single byte of the existing golden files changes.
- The timestamped `date` case is absent from the golden files, so tests are added to `tests/unit/test_datetime.cpp` and `tests/unit/test_output.cpp`.
- `format_iso_datetime` and `format_w3c_datetime` now sit side by side. They are not merged because their purposes differ — template display versus compliance with an external spec — and keeping them apart means changing one cannot break the other.
