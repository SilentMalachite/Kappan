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
| `url` | string | `""` | 空でなければ `sitemap.xml`、`feed.xml`、OGP メタデータの絶対 URL に使う。空なら sitemap と feed は出力せず、URL に依存する OGP フィールドも省略する。不正値は行番号付き `ErrorCode::Config` |
| `language` | string | `ja` | |
| `description` | string | `""` | |
| `pagination.posts_per_page` | int | `10` | `0` は全件 1 ページ。負は行番号付き `ErrorCode::Config` |

未知キーは無視する。型不正は「どのキーが、どうダメか」を日本語で書く。行番号は yaml-cpp の `Mark`（0 始まり）に 1 を足す。

`url` を指定する場合は、正確に `http://` または `https://` で始まり、空でない authority
を持つ絶対 URL でなければならない。userinfo は受け付けない。host は ASCII の DNS 名、
`127.0.0.1` のような IPv4 アドレス、または `[2001:db8::1]` のように角括弧で囲んだ IPv6
リテラルに限る。DNS の各ラベルは63文字以内、末尾のdot 1個を除いた名前全体は253文字以内
とする。`[fe80::1%eth0]` のような IPv6 zone identifier は v1 では未対応。port を指定する
場合は10進数字だけで構成し、1から65535の範囲にする。ASCII の空白、control byte、DEL は
URL 内のどの位置でも拒否する。

有効な例は `https://example.com/blog?q=日本語#先頭`、`http://127.0.0.1:8080`、
`https://[2001:db8::1]:443/`。`https://user@example.com`、`https://example.com:0`、
`https://256.1.1.1`、`https://[fe80::1%eth0]` は無効。検証時にhostの表記を正規化したり、
末尾slashを追加・削除したり、path・query・fragmentを変更したり、percent decodeしたりしない。
検証済みのscalar値をbyte-for-byteそのまま `Config::url` に保持する。

コンテンツは `<source>/content/` 以下の `.md`。静的ファイルは `<source>/static/` を出力根へコピーする。どちらも `_` で始まるディレクトリは走査しない。
