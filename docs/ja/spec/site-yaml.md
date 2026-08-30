# site.yaml

> English (canonical): [`docs/spec/site-yaml.md`](../../spec/site-yaml.md)

サイト根にある YAML。Phase 2 の必須キーは `title` だけ。

```yaml
title: 活版ブログ
url: https://example.com
language: ja
description: 日本語と絵文字 🐙 を含むサイト
```

| キー | 型 | 既定 | 備考 |
|---|---|---|---|
| `title` | string | （必須） | 欠落・非スカラーは行番号付き `ErrorCode::Config` |
| `url` | string | `""` | 空でなければ `sitemap.xml` と `feed.xml` の絶対 URL に使う。空ならどちらも出さない。`http://` / `https://` で始まりホストを持つ絶対 URL のみ受け付け、それ以外は行番号付き `ErrorCode::Config` |
| `language` | string | `ja` | |
| `description` | string | `""` | |
| `pagination.posts_per_page` | int | `10` | `0` は全件 1 ページ。負は行番号付き `ErrorCode::Config` |

未知キーは無視する。型不正は「どのキーが、どうダメか」を日本語で書く。行番号は yaml-cpp の `Mark`（0 始まり）に 1 を足す。

コンテンツは `<source>/content/` 以下の `.md`。静的ファイルは `<source>/static/` を出力根へコピーする。どちらも `_` で始まるディレクトリは走査しない。
