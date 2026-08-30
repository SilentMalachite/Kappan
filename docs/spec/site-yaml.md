# site.yaml

> 日本語版: [`docs/ja/spec/site-yaml.md`](../ja/spec/site-yaml.md)

The YAML file at the site root. As of Phase 2, `title` is the only required key.

```yaml
title: 活版ブログ
url: https://example.com
language: ja
description: 日本語と絵文字 🐙 を含むサイト
```

| Key | Type | Default | Notes |
|---|---|---|---|
| `title` | string | (required) | Missing or non-scalar values produce `ErrorCode::Config` with a line number |
| `url` | string | `""` | When non-empty, used to build the absolute URLs in `sitemap.xml` and `feed.xml`. When empty, neither file is written. Only absolute URLs starting with `http://` or `https://` and carrying a host are accepted; anything else produces `ErrorCode::Config` with a line number |
| `language` | string | `ja` | |
| `description` | string | `""` | |
| `pagination.posts_per_page` | int | `10` | `0` means a single page with every post. A negative value produces `ErrorCode::Config` with a line number |

Unknown keys are ignored. Type errors state which key is wrong and why, in Japanese. Line numbers are the yaml-cpp `Mark` (zero-based) plus one.

Content is the `.md` files under `<source>/content/`. Static files under `<source>/static/` are copied to the output root. In both trees, directories whose name starts with `_` are not traversed.
