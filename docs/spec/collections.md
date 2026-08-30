# Collections, tags, and pagination

> 日本語版: [`docs/ja/spec/collections.md`](../ja/spec/collections.md)

`Site` is the root object handed to templates. A `Document` is never mutated after it is built.

## Collections

| Name | Members | Order |
|---|---|---|
| `posts` | Permalink starts with `/posts/` | `date` descending, undated last, same-day by slug ascending |
| `pages` | Non-posts other than the home page | slug ascending |

`draft: true` documents are in neither collection by default. `kappan build --drafts` includes them.

## Home page and listings

- First page: `/` (`index.html`)
- Subsequent pages: `/page/2/`, and so on
- `content/index.md` defaults to the `index` layout. Its body appears above the listing.
- Each page holds `pagination.posts_per_page` items (10 by default). `0` means a single page with every post.

## Tags

`/tags/{slug}/` is generated from each post's `tags:`. The slug uses the same `slugify` as documents, so kana, kanji, and emoji are preserved.
