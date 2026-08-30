# ADR-0005: Site モデルと生成ページ

> English (canonical): [`docs/adr/0005-site-collections.md`](../../adr/0005-site-collections.md)

- Status: Accepted
- Date: 2026-08-29

## 文脈

Phase 4 でコレクション・タグ・ページネーションを足す。テンプレートに渡す根は `Site` 一つにする（AGENTS.md）。`Document` を Site 構築後に書き換えない。

## 決定

- `include/kappan/site.hpp` に `Site` / `Collection` / `Taxonomy` / `DraftPolicy` を置く。
- `posts` は permalink が `/posts/` で始まる Document。`pages` はそれ以外のうちホーム（`/`）を除く。
- 並びは `date` 降順、date なしは末尾、同日は `slug` 昇順。
- `draft: true` は既定で Site に入れない。`DraftPolicy::Include`（CLI `--drafts`）のときだけ入れる。
- 一覧 1 ページ目は `/`。`content/index.md` があればその Document を `layout: index`（既定）で描き、pagination を載せる。無ければ同じ permalink の生成ページを出す。2 ページ目以降は `/page/N/` の生成ページ。
- タグは `/tags/{slug}/`。v1 はタグページのページネーションをしない。
- 生成 permalink が既存 Document と一致したら `ErrorCode::Path`。`/page/`（`content/page.md`）と `/page/2/` は別パスなので共存できる。
- `pagination.posts_per_page` の既定は 10。`0` は全件 1 ページ。負は Config エラー。

## 結果

- 後段は `Site` を読むだけ。下書き除外と一覧は Site 構築の責務。
