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
| `url` | string | `""` | When non-empty, used to build absolute URLs in `sitemap.xml`, `feed.xml`, and OGP metadata. When empty, neither sitemap nor feed is written and URL-dependent OGP fields are omitted. Invalid values produce `ErrorCode::Config` with a line number |
| `language` | string | `ja` | |
| `description` | string | `""` | |
| `pagination.posts_per_page` | int | `10` | `0` means a single page with every post. A negative value produces `ErrorCode::Config` with a line number |

Unknown keys are ignored. Type errors state which key is wrong and why, in Japanese. Line numbers are the yaml-cpp `Mark` (zero-based) plus one.

When set, `url` must be an absolute URL with the exact `http://` or `https://` prefix and a
non-empty authority. User information is not accepted. The host must be an ASCII DNS name, an
IPv4 address such as `127.0.0.1`, or a bracketed IPv6 literal such as `[2001:db8::1]`. DNS labels
are limited to 63 characters and the name, excluding one optional trailing dot, is limited to 253
characters. IPv6 zone identifiers such as `[fe80::1%eth0]` are not supported in v1. A port, when
present, must contain only decimal digits and be in the range 1 through 65535. ASCII whitespace,
control bytes, and DEL are rejected anywhere in the URL.

Valid examples include `https://example.com/blog?q=日本語#先頭`,
`http://127.0.0.1:8080`, and `https://[2001:db8::1]:443/`. Values such as
`https://user@example.com`, `https://example.com:0`, `https://256.1.1.1`, and
`https://[fe80::1%eth0]` are invalid. Validation does not canonicalize the host, add or remove a
trailing slash, change the path, query, or fragment, or percent-decode the value. The validated
scalar is stored in `Config::url` byte-for-byte.

When Kappan forms an absolute page URL for `page.og.url`, `sitemap.xml`, or `feed.xml`, it removes
trailing slashes from the base URL path, appends the document permalink, and then restores the
configured query and fragment unchanged. For example, the permalink `/posts/hello/` and the base
URL `https://example.com/blog?q=日本語#先頭` produce
`https://example.com/blog/posts/hello/?q=日本語#先頭`.

Content is the `.md` files under `<source>/content/`. Static files under `<source>/static/` are copied to the output root. In both trees, directories whose name starts with `_` are not traversed.
