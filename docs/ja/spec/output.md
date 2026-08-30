# 出力（静的アセット・sitemap・RSS）

> English (canonical): [`docs/spec/output.md`](../../spec/output.md)

Phase 5。レンダリング結果を `--out` に書き、公開に必要なファイルを添える。後段は前段（`Site` / `RenderedPage`）を書き換えない。

## 出力先の準備

設定の読み込みと走査・解析が終わったあと、HTML を書く直前に `--out` を空にする（無ければ作る）。設定エラーの時点では消さない。次は `ErrorCode::Cli` で拒否する。ソースを消さないため。

- `--out` が `--source` と同じ
- `--source` が `--out` の内側にある

`--out` が `--source` の内側（`examples/blog/_site`）はよい。

消す前に、その `--out` が kappan の出力先かを確かめる。出力根に `.kappan-out` を書いておき、次の判断に使う。

| `--out` の状態 | 動作 |
|---|---|
| 無い | 作る |
| 空 | そのまま使う |
| 非空で有効な `.kappan-out` がある | 消して作り直す |
| 非空で `.kappan-out` が無い、または不正と確認できた | **何も消さず** `ErrorCode::Cli`。`--force` でのみ消す |
| 出力先の空判定、マーカーの status、またはマーカーの内容を検査できない | **何も消さず** `ErrorCode::Io` |

有効なマーカーは、symlink ではない通常ファイルで、raw bytes が `kappan output directory\n`（24 bytes）と完全一致するものだけ。UTF-8 BOM、CRLF、余分な byte が 1 つでもあれば不一致であり、BOM 除去や改行正規化はしない。マーカーの status または内容を検査できないときは何も消さず `ErrorCode::Io`。`--force` は互換性のためマーカー検証を迂回するが、上のソース保護判定は迂回しない。

`.kappan-out` は `create_directories` の直後に書く。ビルドが途中で失敗しても次回が拒否されないようにするため。詳細は [ADR-0007](../adr/0007-out-dir-deletion-policy.md)。

## 静的アセット

`<source>/static/` を出力根へ、相対パスを保ったままコピーする。

| 入力 | 出力 |
|---|---|
| `static/css/site.css` | `_site/css/site.css` |
| `static/images/🐙.svg` | `_site/images/🐙.svg` |

- `static/` が無い・空でもエラーにしない。同梱テーマから静的ファイルは出さない。
- `_` で始まるディレクトリは走査しない（`content/` と同じ）。
- `.` で始まるファイル・ディレクトリはコピーしない（`.DS_Store` や `.git/` を出力に混ぜない）。
- 中身はバイト列のまま。UTF-8 検査も改行正規化もしない（画像・フォントを壊さない）。
- 日本語ファイル名はそのまま。

## sitemap.xml

`site.yaml` の `url` が空でなければ、出力根に `sitemap.xml` を書く。空なら書かない（エラーにしない）。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>https://example.com/posts/こんにちは/</loc>
    <lastmod>2026-01-01</lastmod>
  </url>
</urlset>
```

- 対象は生成した HTML すべて（記事・固定ページ・ホーム・`/page/N/`・タグ）。同じ permalink は 1 度だけ。下書きは `Site` に入っていないので出ない。
- `<loc>` は `url` の末尾 `/` を除いたものと permalink を連結する。UTF-8 のまま（パーセントエンコードしない）。XML 特殊文字はエスケープする。
- `<lastmod>` は Document に `date` があるときだけ。sitemaps.org が要求する **W3C Datetime** とし、日付のみ（`YYYY-MM-DD`）か、UTC の `Z` 付き日時（`YYYY-MM-DDThh:mm:ssZ`）を出す。TZD の無い日時は不正なので出さない。一覧・タグなど Document でないページは要素ごと省略する。
- `<url>` の並びは permalink の昇順（ゴールデンを安定させる）。
- BOM は付けない。

## feed.xml（RSS 2.0）

`url` が空でなければ、出力根に `feed.xml` を書く。空なら書かない。

```xml
<?xml version="1.0" encoding="UTF-8"?>
<rss version="2.0">
  <channel>
    <title>活版ブログ</title>
    <link>https://example.com/</link>
    <description>日本語と絵文字 🐙 を含むサイト</description>
    <language>ja</language>
    <item>
      <title>こんにちは</title>
      <link>https://example.com/posts/こんにちは/</link>
      <guid isPermaLink="true">https://example.com/posts/こんにちは/</guid>
      <pubDate>Thu, 01 Jan 2026 00:00:00 +0000</pubDate>
      <description>最初の記事です。</description>
    </item>
  </channel>
</rss>
```

- `item` は `posts` のみ。並びはコレクションと同じ（`date` 降順、無しは末尾）。件数上限は無い。posts が空なら `item` 無しの channel を出す。
- `channel/link` は `url` + `/`。`channel/description` が空なら `title` を使う（RSS 2.0 で必須のため）。
- `item/description` は front matter の `description`。空なら本文 HTML。どちらも XML エスケープする（CDATA は使わない）。
- `pubDate` は RFC 822。時刻は `date` の UTC、オフセットは常に `+0000`。曜日と月の名前は英語で固定する（ロケールに依存しない）。`date` が無ければ要素ごと省略する。
- ページ・タグ・下書きは入れない。

## 衝突

生成 HTML・`sitemap.xml`・`feed.xml`・`static/` が同じ出力パスになるときは黙って上書きしない。`ErrorCode::Path` を集約する。

`url` が空のとき、利用者が `static/sitemap.xml` や `static/feed.xml` を置くのはよい。

## 数え方

`pages_written` は HTML だけ。アセットと XML は含めない。
