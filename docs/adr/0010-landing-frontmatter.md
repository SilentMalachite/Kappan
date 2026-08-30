# ADR-0010: landing の front matter は型付きで持ち、OGP は全レイアウトに出す

- Status: Accepted
- Date: 2026-08-30
- 関連: [ADR-0005](0005-site-collections.md), [ADR-0006](0006-output-assets-feeds.md)

## 文脈

Phase 7 で `layout: landing`、front matter の `sections`、OGP を足す。

`layout` は既にテンプレート選択の汎用経路に乗っている。`src/render/engine.cpp:146` が `front_matter.layout + ".html"` を引くだけなので、`landing.html` を用意すれば LP 用の出力経路を新設せずに済む。AGENTS.md §4「ブログと LP の統一」のとおり、LP は `layout` が違うだけの通常ページとして扱える。サイト側 `templates/landing.html` による上書きも既存の仕組みで効く。

未解決なのは 2 つ。

1. `sections` は YAML の「マップの配列」で、Phase 2 の `FrontMatter`（`include/kappan/document.hpp:11`）に受け皿が無い。テンプレートへ渡すには、どの層でどこまで型を確定させるかを決める必要がある。
2. OGP をどの範囲に出すか。`themes/default/base.html` は `post` / `page` / `index` / `tag` すべての親なので、ここへ meta を足すと LP 以外にも波及する。

## 決定

### 1. `FrontMatter` を型付きで拡張する

```cpp
struct LandingAction {
  std::string label;
  std::string href;
};

struct LandingItem {
  std::string title;
  std::string text;
  std::string icon;
};

struct LandingSection {
  std::string type;
  std::string eyebrow;
  std::string title;
  std::string text;
  std::string image;
  std::vector<LandingAction> actions;
  std::vector<LandingItem> items;
};
```

- `FrontMatter` に `std::string image` と `std::vector<LandingSection> sections` を足す。
- 公開ヘッダ `include/kappan/document.hpp` に `YAML::Node` も `nlohmann::json` も出さない。現在の include は `<chrono>` `<filesystem>` `<optional>` `<string>` `<vector>` だけで、これを保つ。
- 型の確定は解析段（`src/content/parse.cpp`）で終える。既知キーの型が違う場合は行番号付き `ErrorCode::FrontMatter` を返す。
- 未知キーは無視する。`sections[].type` の値は列挙で縛らず、拒否しない。同梱 `landing.html` も `type` で分岐しない。テンプレート側が自由に分岐できるよう素通しする。
- v1 のフィールドは上記だけにする。足りなければ `LandingSection` を拡張する。

### 2. OGP は `render/context.cpp` が組み立て、全レイアウトに入れる

- `page.og` は `title` / `description` / `type` / `url` / `image` / `twitter_card` の 6 フィールド。決定規則は `docs/spec/landing.md` に書く。
- `og.type` は `page.layout` が `post` なら `article`、それ以外は `website`。
- `page_json` だけでなく `make_listing_context` と `make_tag_context` にも入れる。一覧とタグでは `image` は空、`sections` は空配列になる。
- `base.html` は `page.og` を読むだけで、判断を持たない。レンダリング後に `Site` や `Document` を書き換えない（ADR-0005 の一方向を保つ）。
- `site.url` が空なら `og:url` と相対 `og:image` は出さない。ADR-0006 が sitemap / feed に定めた「`url` が空なら絶対 URL を作らない」と同じ扱いにする。壊れた相対 URL を meta に出すより、meta 要素ごと省くほうが安全。

### 3. `join_url` を `src/util/url.hpp` へ移す

絶対 URL 化は `output::join_url`（`src/output/xml.cpp:42`）が既に持っている。`page.og.url` と `og.image` で同じ処理が要るが、render は output より前段なので、後段のヘッダを include して解決したくない。

`util::escape_markup` を `render::html_escape` と `output::xml_escape` が共有しているのと同じ形にし、`src/util/url.{hpp,cpp}` へ移して両方から使う。振る舞いは変えない（末尾の `/` を畳み、permalink が空なら `/` を付ける、日本語をパーセントエンコードしない）。

## 却下した案

**`FrontMatter` に `nlohmann::json sections` を持たせる。** 実装は最短で、inja へもそのまま渡せる。しかし公開ヘッダ `include/kappan/document.hpp` に nlohmann-json が漏れる。`Document` は AGENTS.md §4 の中心となる型で、利用者から見える形を最小に保ちたい。型不正の検出もレンダリング時まで遅れ、「どのファイルの、何が、どうダメか」を行番号付きで出す §6 の要求を満たしにくい。

**`YAML::Node` を `Document` に持ち回る。** 同じ理由に加え、yaml-cpp のノードは解析元の寿命に縛られる。差分ビルド（Phase 6）で `Document` を保持する経路と相性が悪い。

**LP 専用のパイプラインを作る。** AGENTS.md §4「LP を特別扱いしない」と Phase 7 の非目標に反する。出力先の規則・sitemap・feed・static コピーをすべて二重に持つことになる。

**OGP を `layout: landing` のときだけ出す。** `blog-ja` golden の差分は小さくなる。しかしブログ記事を共有したときにタイトルも画像も出ないまま残り、Phase 5 で「実サイトとして公開できる」ようにした目的を欠く。OGP は LP 固有の機能ではない。

## 結果

- LP も通常ページと同じ `Document` と `Engine::render` で出る。新しい出力経路は増えない。
- `base.html` の変更は全レイアウトに波及するので、`tests/golden/blog-ja/expected/**` の HTML 9 件を再生成する。`examples/blog` の front matter に `description` が無いため、差分は `og:` の追加行だけになる。
- `sections` の HTML escaping は `render/context.cpp` で一元化する。`page.content` だけは既存どおり Markdown 由来の HTML として生で渡す。
- 依存は増えない。`vcpkg.json` は変えない。
