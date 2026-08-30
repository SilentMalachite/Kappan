# ADR-0010: Landing front matter is typed, and OGP goes into every layout

> 日本語版: [`docs/ja/adr/0010-landing-frontmatter.md`](../ja/adr/0010-landing-frontmatter.md)

- Status: Accepted
- Date: 2026-08-30
- Related: [ADR-0005](0005-site-collections.md), [ADR-0006](0006-output-assets-feeds.md)

## Context

Phase 7 adds `layout: landing`, the `sections` front matter, and OGP.

`layout` already rides the generic template-selection path: `src/render/engine.cpp:146` merely looks up `front_matter.layout + ".html"`, so providing `landing.html` avoids adding a landing-specific output path. As AGENTS.md §4 ("blogs and landing pages are unified") requires, a landing page is an ordinary page that differs only in its `layout`. Overriding it with a site-local `templates/landing.html` works through the existing mechanism.

Two things are unresolved.

1. `sections` is a YAML array of maps, and the Phase 2 `FrontMatter` (`include/kappan/document.hpp:11`) has nowhere to put it. Handing it to templates means deciding which layer fixes the types, and how far.
2. How widely to emit OGP. `themes/default/base.html` is the parent of `post`, `page`, `index`, and `tag`, so adding meta there reaches far beyond landing pages.

## Decision

### 1. Extend `FrontMatter` with typed fields

```cpp
struct LandingAction {
  std::string label;
  std::string href;
};

struct LandingItem {
  std::string title;
  std::string text;
  std::string icon;
};

struct LandingSection {
  std::string type;
  std::string eyebrow;
  std::string title;
  std::string text;
  std::string image;
  std::vector<LandingAction> actions;
  std::vector<LandingItem> items;
};
```

- `FrontMatter` gains `std::string image` and `std::vector<LandingSection> sections`.
- Neither `YAML::Node` nor `nlohmann::json` appears in the public header `include/kappan/document.hpp`. Its includes today are only `<chrono>`, `<filesystem>`, `<optional>`, `<string>`, and `<vector>`, and that stays true.
- Types are fixed during parsing (`src/content/parse.cpp`). A known key with the wrong type returns `ErrorCode::FrontMatter` with a line number.
- Unknown keys are ignored. The value of `sections[].type` is not constrained to an enumeration and is never rejected. The bundled `landing.html` does not branch on `type` either: it is passed straight through so that a site-local template can branch freely.
- v1 ships only the fields above. If they are not enough, `LandingSection` is extended.

### 2. `render/context.cpp` assembles OGP, and it goes into every layout

- `page.og` has six fields: `title`, `description`, `type`, `url`, `image`, `twitter_card`. The rules are written in `docs/spec/landing.md`.
- `og.type` is `article` when `page.layout` is `post`, otherwise `website`.
- It is added not only to `page_json` but also to `make_listing_context` and `make_tag_context`. On listings and tags, `image` is empty and `sections` is an empty array.
- `base.html` only reads `page.og` and makes no decisions. Nothing mutates `Site` or `Document` after rendering, preserving the one-way flow of ADR-0005.
- When `site.url` is empty, `og:url` and a relative `og:image` are not emitted, matching the rule ADR-0006 set for sitemap and feed ("no absolute URLs when `url` is empty"). Dropping the whole meta element is safer than emitting a broken relative URL.

### 3. Move `join_url` to `src/util/url.hpp`

Building absolute URLs already lives in `output::join_url` (`src/output/xml.cpp:42`). `page.og.url` and `og.image` need the same logic, but render runs before output, and we do not want to include a later stage's header to get it.

We follow the shape `util::escape_markup` already has, shared by `render::html_escape` and `output::xml_escape`: move it to `src/util/url.{hpp,cpp}` and use it from both. Behaviour does not change — collapse the trailing `/`, append `/` when the permalink is empty, and never percent-encode Japanese.

## Rejected alternatives

**Store `nlohmann::json sections` on `FrontMatter`.** This is the shortest implementation and feeds inja directly, but it leaks nlohmann-json into the public header `include/kappan/document.hpp`. `Document` is the central type of AGENTS.md §4, and we want the shape users see to stay minimal. It would also delay type errors until render time, making it hard to meet §6's requirement to report which file, what is wrong, and why, with a line number.

**Carry `YAML::Node` on `Document`.** Same reason, plus yaml-cpp nodes are tied to the lifetime of what they were parsed from, which fits poorly with the Phase 6 path that keeps `Document` alive across incremental builds.

**Build a landing-specific pipeline.** This contradicts AGENTS.md §4 ("landing pages are not special-cased") and the Phase 7 non-goals. It would duplicate the output path rules, sitemap, feed, and static copying.

**Emit OGP only for `layout: landing`.** The diff to the `blog-ja` golden files would be smaller, but sharing a blog post would still show neither title nor image, which undercuts the Phase 5 goal of being publishable as a real site. OGP is not a landing-specific feature.

## Consequences

- Landing pages come out of the same `Document` and `Engine::render` as ordinary pages. No new output path is added.
- Changes to `base.html` reach every layout, so the 9 HTML files under `tests/golden/blog-ja/expected/**` are regenerated. Since the `examples/blog` front matter has no `description`, the diff is only the added `og:` lines.
- HTML escaping of `sections` is centralised in `render/context.cpp`. Only `page.content` keeps being passed through raw, as Markdown-derived HTML.
- No dependency is added; `vcpkg.json` does not change.
