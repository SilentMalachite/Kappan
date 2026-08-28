# site.yaml

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
| `url` | string | `""` | |
| `language` | string | `ja` | |
| `description` | string | `""` | |

未知キーは無視する。型不正は「どのキーが、どうダメか」を日本語で書く。行番号は yaml-cpp の `Mark`（0 始まり）に 1 を足す。

コンテンツは `<source>/content/` 以下の `.md`。`_` で始まるディレクトリは走査しない。
