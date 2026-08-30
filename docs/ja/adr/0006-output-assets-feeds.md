# ADR-0006: 静的アセットとフィードの置き方

> English (canonical): [`docs/adr/0006-output-assets-feeds.md`](../../adr/0006-output-assets-feeds.md)

- Status: Accepted
- Date: 2026-08-29

## 文脈

Phase 5 で生成物をそのまま公開できるようにする。HTML の書き出しは `content/build.cpp` に寄っている。アセットコピー・`sitemap.xml`・RSS がまだ無い。公開ヘッダを増やさずに、パイプラインの最終段として足したい。

## 決定

- 書き出し・コピー・XML 生成は `src/output/` に置く。`content::build_site` はオーケストレーションのまま、後段を呼ぶだけにする。
- 静的ファイルは `<source>/static/` を出力根へそのままコピーする（Hugo 流）。`content/` 内の非 Markdown はコピーしない。画像の変換はしない。
- フィードは RSS 2.0 を `/feed.xml` に 1 本だけ出す。Atom は出さない。
- `site.yaml` の `url` が空なら sitemap / RSS を出さない。エラーにはしない（`kappan new` の既定を壊さない）。
- `<loc>` と `<link>` は UTF-8 のまま連結する。日本語 URL をパーセントエンコードしない。
- `--out` は書き出し前に空にする。`--out` がソース根と同じ、またはソースが `--out` の内側なら `ErrorCode::Cli`。
- 公開ヘッダ（`Config` など）は変えない。`static/` の位置は `source_root / "static"` で足りる。

## 結果

- 例: `static/images/🐙.svg` → `_site/images/🐙.svg`。
- `examples/blog`（`url` あり）は `sitemap.xml` と `feed.xml` がゴールデンに入る。
- 衝突は上書きせず `ErrorCode::Path`。
