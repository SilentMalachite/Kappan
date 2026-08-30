# ADR-0004: permalink と日本語 slug

> English (canonical): [`docs/adr/0004-permalinks-and-slug.md`](../../adr/0004-permalinks-and-slug.md)

- Status: Accepted
- Date: 2026-08-29

## 文脈

Phase 2 で `Document` の出力先を決める。日本語のファイル名・見出しをローマ字化すると、追加ライブラリが要るうえ、読者が URL を見て内容を推測しにくくなる。

## 決定

- pretty URL にする。`content/about.md` → `/about/index.html`（permalink `/about/`）。トップだけ `content/index.md` → `/index.html`（permalink `/`）。
- 記事は `content/posts/`。`YYYY-MM-DD-` プレフィックスは任意で date のフォールバック。permalink は `/posts/{slug}/`。
- slug の優先順は front matter `slug:` → 日付プレフィックスを除いた stem → title。ASCII は小文字、空白（半角/全角）と Windows 予約文字は `-`。かな・漢字・絵文字は残す。
- その変換後、末尾のdotを取り除く。最初のdotより前のASCII小文字basenameがWindows予約デバイス名（`con`, `prn`, `aux`, `nul`, `com1`〜`com9`, `lpt1`〜`lpt9`）なら、先頭に `_` を付ける。この規則は全OSで適用する。同じコンテンツから決定的なURLを生成しつつWindowsでも書き出せるように、OS別の分岐は行わない。
- ローマ字化ライブラリは入れない。Unicode 正規化（NFC 合成）も ICU が要るため v1 では行わず、入力 UTF-8 をそのまま使う。
- 同一 permalink が衝突したら `ErrorCode::Path` で集約する（黙って上書きしない）。

## 結果

- 日本語 URL がテストで固定できる。
- Phase 4 の `/page/N/` は `content/page.md` と衝突し得る。衝突時は Config エラーにする（Phase 4）。
