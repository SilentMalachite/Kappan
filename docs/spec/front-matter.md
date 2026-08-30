# Front matter

> 日本語版: [`docs/ja/spec/front-matter.md`](../ja/spec/front-matter.md)

When the first line is `---`, everything up to the next `---` is parsed as YAML and the remainder is the Markdown body. CRLF has already been normalised to LF at read time. A file without `---` has no front matter. A missing closing `---`, or malformed YAML, does not throw: it is collected as `ErrorCode::FrontMatter`.

Empty content between the delimiters is treated the same as no front matter. Any non-empty YAML must have a mapping at the document root. A scalar, sequence, or explicit null (`~`) root produces an `ErrorCode::FrontMatter` validation error with the source file and the first front matter line (normally line 2). Validation errors from multiple Markdown files are collected while valid sibling files continue to be rendered, then reported together when the build finishes.

For example, this sequence is not a valid front matter root:

```yaml
- title
- 配列
```

`content/bad-sequence.md:2 front matter はマップである必要があります`

| Key | Type | Default |
|---|---|---|
| `title` | string | The file stem with the date prefix removed |
| `date` | string | The `YYYY-MM-DD-` prefix of the filename, or empty. An ISO 8601 date, or `YYYY-MM-DDTHH:MM:SS` |
| `layout` | string | `post` under `content/posts/`, otherwise `page` |
| `slug` | string | The stem with the date prefix removed; failing that, `slugify` applied to the title |
| `draft` | bool | `false` (not written by default; include with `build --drafts`) |
| `tags` | string[] | empty |
| `description` | string | `""` |
| `image` | string | `""` |
| `sections` | map[] | empty |

Explicit front matter `slug:` values use the same normalisation as stem and title fallbacks. Trailing dots are removed, and a Windows reserved device basename is prefixed with `_` on every OS: `CON` → `_con`, `con.txt` → `_con.txt`, and `LPT9.` → `_lpt9`.

`date` is parsed from the scalar string by hand rather than relying on the yaml-cpp date type. An example of an invalid value:

`content/posts/2026-01-01-hello.md:3 front matter の 'date' が日付として解釈できません: '2026-13-01'`

`image` and `sections` are for landing pages (see [Landing pages](landing.md)). `image` becomes a candidate for the OGP `og:image`, and `sections` is structured data handed to the template. Both may also be used in blog posts.

`sections` is an array of maps. The known keys of each element are `type`, `eyebrow`, `title`, `text`, `image`, `actions`, and `items`. `actions` is an array of `{ label, href }`, `items` is an array of `{ title, text, icon }`, and every value is a string. Omitted strings become empty, omitted arrays become empty. Unknown keys are ignored. The value of `type` is never rejected.

```yaml
sections:
  - type: hero
    eyebrow: Kappan
    title: 日本語LPを静的生成
    text: Markdown と YAML だけで公開できます 🐙
    actions:
      - label: 機能を見る
        href: "#features"
  - type: features
    items:
      - title: UTF-8
        text: かな・漢字・絵文字をそのまま扱います。
        icon: 文
```

A known key with the wrong type produces `ErrorCode::FrontMatter` with a line number. An example of an invalid value:

`content/index.md:6 front matter の 'sections.actions' はマップの配列である必要があります`
