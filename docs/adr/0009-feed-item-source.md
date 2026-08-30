# ADR-0009: The feed lists only articles that were written successfully

> 日本語版: [`docs/ja/adr/0009-feed-item-source.md`](../ja/adr/0009-feed-item-source.md)

- Status: Accepted
- Date: 2026-08-29
- Related: [ADR-0006](0006-output-assets-feeds.md)

## Context

`output::render_feed` walks `site.posts.indices` unconditionally, while `sitemap.xml` is built from `sitemap_urls`, which only accumulates pages that were written successfully. **Two XML files from the same build rest on different sets.**

Measured with `build/dev/kappan`, giving one article a layout that parses but fails at render time:

```
out/posts/broken/index.html    → not created
/posts/broken/ in sitemap.xml  → absent (correct)
/posts/broken/ in feed.xml     → <link>https://example.com/posts/broken/</link> remains
```

Subscribers receive a permanent 404 through the feed. Feed readers remember items once delivered, so fixing it in the next build does not remove the stale entry.

`docs/spec/output.md` defines the sitemap as "every generated HTML page" and the feed as "`item` covers `posts` only" — different words for different sets — and the implementation follows the spec. **The discrepancy originates in the spec.**

## Decision

**Restrict the feed to the same set as the sitemap: pages whose HTML was written successfully.**

- `render_feed` takes the set of written permalinks:

  ```cpp
  [[nodiscard]] std::string render_feed(const Site &site,
                                        const std::set<std::string> &written_permalinks);
  ```

- It still walks `posts.indices`, but drops the `item` for any permalink absent from `written_permalinks`. The order (`date` descending) does not change.
- The number of dropped items is **not** an error. The HTML side has already collected an `ErrorCode`, and the same event must not be reported twice.
- The caller (`content::publish_feeds`) builds the set from `sitemap_urls`. It must do so **before** `std::move`-ing `sitemap_urls` into `render_sitemap`.
- The feed line in `docs/spec/output.md` is rewritten accordingly.

## Rejected alternatives

**Keep the `render_feed` signature and carry a "written" flag on `Site` instead.** This violates ADR-0006's rule that a later stage never mutates an earlier one (`Site` / `RenderedPage`). `Site` is a model fixed before rendering; attaching write results to it would break its meaning in the Phase 6 incremental build.

## Consequences

- `sitemap.xml` and `feed.xml` always describe the same reality.
- An ordinary build where every page succeeds produces identical output, so the existing golden files do not change.
- `render_feed` is no longer determined by `Site` alone. `src/output/` is internal, so the public headers under `include/kappan/` do not change.
- A test in `tests/unit/test_build_site.cpp` pins that an item disappears from the feed when its article fails to render.
