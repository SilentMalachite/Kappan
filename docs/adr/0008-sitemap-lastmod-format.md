# ADR-0008: sitemap の `<lastmod>` は W3C Datetime にする

- Status: Accepted
- Date: 2026-08-29
- 関連: [ADR-0006](0006-output-assets-feeds.md)

## 文脈

`docs/spec/output.md` は `<lastmod>` を「Document に `date` があるときだけ ISO 日付（`YYYY-MM-DD` または日時）」とだけ定義しており、実装は `util::format_iso_datetime` の戻り値をそのまま入れている。実装は仕様どおりで、逸脱ではない。**仕様の側が外部要求を満たしていない。**

実測（`build/dev/kappan`）:

```
date: 2026-01-01T09:30:00Z の記事
→ <lastmod>2026-01-01T09:30:00</lastmod>
（同じビルドの <pubDate> は Thu, 01 Jan 2026 09:30:00 +0000 で正しい）
```

sitemaps.org は `<lastmod>` に W3C Datetime を要求する。W3C Datetime の complete-date-plus-hours-minutes-seconds 形式は TZD（`Z` または `±hh:mm`）が必須で、TZD の無い値はバリデータが拒否する。パースする側のクローラも未定義のゾーンとして扱う。

`YYYY-MM-DD`（complete date 形式）は TZD 不要で正しい。`examples/blog` の記事はすべて日付のみなのでこの分岐に入り、ゴールデンはこの欠陥を素通ししている。

`format_iso_datetime` はテンプレート変数 `date` と共用しているため、そこを直すとテンプレート出力と既存ゴールデンまで動く。

## 決定

**sitemap 専用のフォーマッタを新設し、`format_iso_datetime` は変えない。**

- `util::format_w3c_datetime(std::chrono::sys_seconds)` を `src/util/datetime.hpp` に足す。
  - 深夜 0 時ちょうど → `YYYY-MM-DD`（complete date 形式。TZD 不要）
  - それ以外 → `YYYY-MM-DDThh:mm:ssZ`
- 値は `sys_seconds`＝UTC なので、オフセットは常に `Z`。`±hh:mm` は出さない（`format_rfc822` が `+0000` 固定なのと同じ方針）。
- `render_sitemap` の `<lastmod>` はこの新関数を使う。
- `format_iso_datetime` は現状のまま。テンプレート変数 `date` の意味と既存ゴールデンを動かさない。
- `docs/spec/output.md` の `<lastmod>` の行を書き換える:

  > `<lastmod>` は Document に `date` があるときだけ。W3C Datetime とし、日付のみ（`YYYY-MM-DD`）か、UTC の `Z` 付き日時（`YYYY-MM-DDThh:mm:ssZ`）を出す。一覧・タグなど Document でないページは要素ごと省略する。

## 結果

- `examples/blog` は全記事が日付のみなので、既存のゴールデンは 1 バイトも変わらない。
- 日時付き `date` のケースは既存ゴールデンに無いので、`tests/unit/test_datetime.cpp` と `tests/unit/test_output.cpp` にテストを足す必要がある。
- `format_iso_datetime` と `format_w3c_datetime` が並ぶことになるが、用途が違う（テンプレート表示用と外部仕様準拠用）ので統合しない。片方を変えたときにもう片方が壊れないことが利点。
