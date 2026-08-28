# コレクション・タグ・ページネーション

`Site` がテンプレートに渡す根。`Document` は構築後に書き換えない。

## コレクション

| 名前 | 対象 | 並び |
|---|---|---|
| `posts` | permalink が `/posts/` で始まる | `date` 降順、無しは末尾、同日は slug 昇順 |
| `pages` | ホーム以外の非 posts | slug 昇順 |

`draft: true` は既定でどちらにも入らない。`kappan build --drafts` で含める。

## ホームと一覧

- 1 ページ目: `/`（`index.html`）
- 2 ページ目以降: `/page/2/` …
- `content/index.md` の layout 既定は `index`。本文は一覧の上に出る。
- 1 ページあたり `pagination.posts_per_page`（既定 10）。`0` なら全件 1 ページ。

## タグ

記事の `tags:` から `/tags/{slug}/` を作る。slug は記事と同じ `slugify`。かな・漢字・絵文字は残す。
