# ADR-0003: cmark-gfm の拡張と生 HTML

> English (canonical): [`docs/adr/0003-gfm-extensions.md`](../../adr/0003-gfm-extensions.md)

- Status: Accepted
- Date: 2026-08-28

## 文脈

Phase 1 は Markdown 1 ファイルを HTML に変換する。ブログ著者はテーブルや打ち消し線を使い、ときどき生 HTML を本文に書く。危険なタグをそのまま出すと XSS の足がかりになる。

## 決定

- パーサは **cmark-gfm**。コア拡張のうち `table` / `strikethrough` / `autolink` / `tasklist` / `tagfilter` を有効化する。
- オプションは `CMARK_OPT_UNSAFE`（生 HTML と一部の危険な URL を残す）、`CMARK_OPT_STRIKETHROUGH_DOUBLE_TILDE`（`~~`）、`CMARK_OPT_GITHUB_PRE_LANG`。
- `CMARK_OPT_FOOTNOTES` はコア拡張外なので v1 では使わない。
- 不正 UTF-8 は cmark に渡さず、読み込み段階で `ErrorCode::Utf8` にする（`CMARK_OPT_VALIDATE_UTF8` の置換文字に頼らない）。
- `tagfilter` は危険な HTML タグを無効化する。残った生 HTML の XSS は静的サイトの著者責任として README / spec に書く。

## 結果

- GFM テーブルと `~~打ち消し~~` がテストで固定される。
- 依存を増やさずに GitHub Flavored Markdown の中核をカバーできる。
