# Landing pages

Phase 7。LP は `layout: landing` を持つ通常の Markdown ページとして扱う。専用の CLI もパイプラインも無い（[ADR-0010](../adr/0010-landing-frontmatter.md)）。

- 出力先は既存の pretty URL 規則に従う。`content/index.md` なら `/`、`content/lp.md` なら `/lp/`。
- テンプレートは `landing.html`。サイト側に `<source>/templates/landing.html` があればそちらが優先される（[テンプレート](templates.md)）。
- `sections` はテンプレート用の構造化データ。本文 `page.content` と併用でき、同梱テンプレートでは `sections` の後ろに本文が出る。
- `sections[].type` は同梱 `landing.html` では分岐に使わない。値を拒否しないので、サイト側テンプレートが自由に分岐できる。
- `image` は OGP の `og:image` 候補になる。
- `examples/landing` が `kappan build --source examples/landing --out <dir>` で生成できる。

front matter の書式は [Front matter](front-matter.md) を見ること。

## OGP

`page.og` は `src/render/context.cpp` が組み立てる。全レイアウト（`post` / `page` / `index` / `tag` / `landing`）に入り、`base.html` はこれを読むだけで判断を持たない。

| フィールド | 規則 |
|---|---|
| `og.title` | `page.title` が空でなければ `{page.title} — {site.title}`、空なら `site.title`。同梱テンプレートの `<title>` と同じ式 |
| `og.description` | `page.description` が空でなければそれ、空なら `site.description` |
| `og.type` | `page.layout` が `post` なら `article`、それ以外は `website` |
| `og.url` | `site.url` が空でなければ `site.url` と `page.permalink` を連結した絶対 URL、空なら空 |
| `og.image` | `page.image` が `http://` / `https://` 始まりならそのまま。`/` 始まりで `site.url` があれば絶対 URL 化。それ以外（相対、空）は空 |
| `og.twitter_card` | `og.image` が空でなければ `summary_large_image`、空なら空 |

絶対 URL 化は sitemap / feed と同じ規則で行う。末尾の `/` を畳んでから permalink を連結し、日本語をパーセントエンコードしない（[ADR-0006](../adr/0006-output-assets-feeds.md)）。

`site.url` が空のとき、`og:url` と相対 `og:image` は出さない。空文字のまま `meta` を書かず、`base.html` 側の `{% if %}` で meta 要素ごと省く。壊れた相対 URL を共有先に渡さないため。

## 出力例

```html
<meta property="og:title" content="日本語LPを静的生成 — 活版ランディング">
<meta property="og:description" content="Markdown と YAML から、日本語に強いランディングページを生成します。">
<meta property="og:type" content="website">
<meta property="og:url" content="https://example.com/">
<meta property="og:image" content="https://example.com/images/og.svg">
<meta name="twitter:card" content="summary_large_image">
```

## v1 でやらないこと

- OG 画像の自動生成、画像のリサイズ・変換。
- `sections[].type` の固定列挙とバリデーション。
- LP 専用の CLI サブコマンドや出力経路。
