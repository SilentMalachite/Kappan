# ADR-0009: feed に載せるのは書き出しに成功した記事だけにする

> English (canonical): [`docs/adr/0009-feed-item-source.md`](../../adr/0009-feed-item-source.md)

- Status: Accepted
- Date: 2026-08-29
- 関連: [ADR-0006](0006-output-assets-feeds.md)

## 文脈

`output::render_feed` は `site.posts.indices` を無条件に走査する。一方 `sitemap.xml` は、書き出しに成功したページだけを積んだ `sitemap_urls` から作られる。**同じビルドの 2 つの XML が、違う集合を根拠にしている。**

実測（`build/dev/kappan`）。パースは通るがレンダリング時に落ちるレイアウトを 1 記事に付けた場合:

```
out/posts/broken/index.html   → 作られない
sitemap.xml の /posts/broken/ → 0 件（正しい）
feed.xml     の /posts/broken/ → <link>https://example.com/posts/broken/</link> が残る
```

購読者は恒久的な 404 をフィードで受け取る。フィードリーダーは一度配信した item を覚えるので、次のビルドで直しても古い項目は消えない。

`docs/spec/output.md` は sitemap を「対象は生成した HTML すべて」、feed を「`item` は `posts` のみ」と別々の言葉で定義しており、実装は仕様どおり。**食い違いの原因は仕様側にある。**

## 決定

**feed も sitemap と同じく「HTML の書き出しに成功したページ」に限る。**

- `render_feed` が書き出し済み permalink の集合を受け取るようにする:

  ```cpp
  [[nodiscard]] std::string render_feed(const Site &site,
                                        const std::set<std::string> &written_permalinks);
  ```

- `posts.indices` を走査したうえで、`written_permalinks` に無い permalink の `item` を落とす。並び順（`date` 降順）は変えない。
- 落とした件数は**エラーにしない**。HTML 側で既に `ErrorCode` が積まれており、同じ事象を二重に報告しない。
- 呼び出し側（`content::publish_feeds`）は `sitemap_urls` から集合を作る。`sitemap_urls` を `render_sitemap` へ `std::move` する**前**に作ること。
- `docs/spec/output.md` の feed の行を書き換える:

  > `item` は `posts` のうち **HTML の書き出しに成功したもの**。並びはコレクションと同じ（`date` 降順、無しは末尾）。件数上限は無い。対象が空なら `item` 無しの channel を出す。

## 却下した案

**`render_feed` の引数を増やさず、`Site` 側に「書き出し済み」フラグを持たせる。** ADR-0006 の「後段は前段（`Site` / `RenderedPage`）を書き換えない」に反する。`Site` はレンダリング前に確定するモデルであり、書き出し結果を持たせると Phase 6 の差分ビルドで意味が壊れる。

## 結果

- `sitemap.xml` と `feed.xml` が常に同じ現実を指す。
- 全ページが成功する通常のビルドでは出力が変わらないので、既存のゴールデンは変わらない。
- `render_feed` が `Site` だけでは決まらなくなる。`src/output/` は内部なので公開ヘッダ `include/kappan/` は変わらない。
- レンダリングに失敗した記事があるビルドで feed から item が消えることを固定するテストを `tests/unit/test_build_site.cpp` に足す。
