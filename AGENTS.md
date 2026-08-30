# AGENTS.md — Kappan（活版）/ C++20 静的サイトジェネレーター

> このファイルは**すべてのコーディングエージェント共通の規約**です。
> Claude Code 固有の運用は `CLAUDE.md` に書きますが、本ファイルを上書きしません（追加のみ）。
> 規約と実装が食い違った場合は、実装を直す前に**本ファイルを直すか、直さない理由を述べる**こと。

---

## 1. プロジェクト概要

**Kappan** は Markdown からブログ記事とランディングページ（LP）の両方を生成する、単一バイナリの静的サイトジェネレーター。

### 目標
- Markdown + YAML front matter → HTML の静的生成
- テンプレートによるレイアウト分離（ブログ用・LP 用の両方を同じ仕組みで扱う）
- 依存ゼロで配布できる単一実行ファイル（macOS / Windows / Linux）
- 日本語コンテンツを一級市民として扱う（UTF-8、CJK、ファイル名）

### 非目標（v1 では作らない）
- プラグイン機構、動的サーバー、CMS、WYSIWYG エディタ
- 画像の自動変換・最適化（将来の別ツールに委ねる）
- 任意 JavaScript の実行を伴うテンプレート

---

## 2. ビルドと実行

```bash
# 初回のみ：vcpkg の取得
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
../vcpkg/bootstrap-vcpkg.sh          # Windows は bootstrap-vcpkg.bat

# 構成 → ビルド → テスト
cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure

# リリースビルド
cmake --preset release
cmake --build --preset release

# 配布用の自己完結ビルド（Windows は dist-windows）
cmake --preset dist
cmake --build --preset dist
ctest --preset dist --output-on-failure

# 実行
./build/dev/kappan build   --source examples/blog --out _site
./build/dev/kappan serve   --source examples/blog --port 8080 --watch
./build/dev/kappan new     my-site
```

- ビルドシステム: **CMake 3.28+ / Ninja / CMakePresets.json**
- 依存管理: **vcpkg manifest モード**（`vcpkg.json`）
- コンパイラ: `std::format` と `std::jthread` を使うため下限が高い
  - macOS: Xcode 26 以降（Xcode 16.4 の libc++ には `std::jthread` が無い）
  - Linux: GCC 13 以降（GCC 12 には `<format>` が無い）
  - Windows: MSVC のみ。MinGW / Clang は `x64-windows` triplet と ABI が合わず、構成時に止める
- **プリセット外のビルドコマンドを新設しない**。必要なら `CMakePresets.json` に追加する。
- 配布関連は `KAPPAN_DIST` の下にまとめる（[ADR-0011](docs/adr/0011-release-distribution.md)）。

---

## 3. ディレクトリ構成

```
kappan/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── AGENTS.md / CLAUDE.md
├── README.md / CHANGELOG.md / CONTRIBUTING.md / SECURITY.md / CODE_OF_CONDUCT.md
│                       # 英語が正本。日本語版は *.ja.md
├── .gitattributes      # ワークツリーの改行を LF に固定する（Windows の CRLF 変換対策）
├── .github/
│   ├── workflows/      # ci.yml（3 OS）/ release.yml（配布 4 環境 + タグ公開）
│   ├── ISSUE_TEMPLATE/
│   └── PULL_REQUEST_TEMPLATE.md
├── cmake/
│   ├── embed_theme.cmake       # themes/default をヘッダへ埋め込む
│   ├── dist_smoke.cmake        # 実行ファイル実体で golden を突き合わせる
│   └── dist_selfcontained.cmake # 配布バイナリの依存を許可リストで検証する
├── docs/
│   ├── spec/           # 機能仕様（英語・正本）
│   ├── adr/            # 設計判断の記録（英語・正本。ADR-0001.md ...）
│   └── ja/             # spec と adr の日本語版
├── include/kappan/     # 公開ヘッダのみ
├── src/
│   ├── main.cpp        # CLI 入口。ここだけが例外を捕捉する
│   ├── config/         # site.yaml の読み込みと検証
│   ├── content/        # ファイル走査、front matter 分離、Document 構築
│   ├── markdown/       # cmark-gfm ラッパ
│   ├── render/         # inja テンプレート適用
│   ├── site/           # Site モデル、コレクション、タクソノミ、ページネーション
│   ├── output/         # 出力書き込み、アセットコピー、sitemap / RSS
│   └── util/           # パス、UTF-8、slug、日時
├── tests/
│   ├── unit/
│   ├── golden/         # 入力サイト → 期待 HTML の比較
│   └── fixtures/       # 日本語・絵文字を含むこと（必須）
├── themes/default/     # 同梱テーマ（base.html / post.html / page.html / index.html / tag.html / landing.html）
└── examples/
    ├── blog/
    └── landing/
```

**規則:** `src/` 直下の各ディレクトリは 1 つの責務に対応する。どこに置くか迷うコードは、置く前に相談すること。

---

## 4. アーキテクチャ

一方向のパイプライン。**逆流させない**（後段が前段を書き換えない）。

```
Config読込 → 走査 → 解析 → Siteモデル構築 → レンダリング → 書き出し
 config/    content/  content/    site/         render/      output/
                      markdown/
```

| 段階 | 入力 | 出力 | 主な型 |
|---|---|---|---|
| Config読込 | `site.yaml` | 検証済み設定 | `Config` |
| 走査 | ソースディレクトリ | ファイル一覧 | `SourceFile` |
| 解析 | `SourceFile` | front matter + 本文 HTML | `Document` |
| モデル構築 | `Document[]` | サイト全体 | `Site`, `Collection`, `Taxonomy` |
| レンダリング | `Site` | HTML 文字列 | `RenderedPage` |
| 書き出し | `RenderedPage[]` | 出力ディレクトリ | — |

### 中心となる型
- `Config` — サイト設定。不変。構築時に検証を完了させる。
- `Document` — 1 ソースファイル。front matter（`FrontMatter`）+ 本文 HTML + 出力先パス。
- `Collection` — 同種の `Document` の集合（`posts`, `pages`）。
- `Site` — `Config` + 全 `Collection` + タクソノミ。テンプレートに渡す唯一のルート。

### ブログと LP の統一
LP を特別扱いしない。**front matter の `layout:` がテンプレートを決める**だけ。
- ブログ記事 → `layout: post`
- LP → `layout: landing`（front matter の `sections:` 配列を inja が展開）

---

## 5. C++ コーディング規約

### 言語機能
- **C++20**。ただし **modules は使わない**（ツールチェーン差が大きいため。方針変更時は ADR を書く）
- coroutines も v1 では使わない
- `std::ranges` / views をループより優先する
- SFINAE ではなく **concepts** を使う
- `std::format` を使う（`printf` / `iostream` の書式は使わない）
- パスは必ず `std::filesystem::path`。**文字列連結でパスを組み立てない**

### 型と所有権
- 生 `new` / `delete` 禁止。所有は値・`std::unique_ptr` のみ
- 非所有の参照は `const T&` / `std::string_view` / `std::span`
- 戻り値のある関数には `[[nodiscard]]`
- 出力引数を使わない。複数値は構造体か `std::pair` を返す
- 引数の bool フラグを避ける（`enum class` にする）

### スタイル
- 名前空間は `kappan::`（サブは `kappan::content` など）。ヘッダで `using namespace` 禁止
- 型は `PascalCase`、関数・変数は `snake_case`、メンバは末尾 `_`、定数は `kPascalCase`
- ヘッダは `#pragma once`
- 1 関数 50 行以内、1 ファイル 400 行以内を目安。超えたら分割を提案する
- `clang-format`（LLVM ベース、100 桁）と `clang-tidy` に従う。整形は手作業でなく `cmake --build --preset dev --target format`

### 警告
`-Wall -Wextra -Wpedantic -Wshadow -Wconversion` を常時有効。CI では `-Werror`。
**警告を `#pragma` で黙らせない。** 消せない場合は理由をコメントに残して相談する。

---

## 6. エラー処理

- 内部の想定内エラーは **`tl::expected<T, Error>`** を返す。例外を投げない
- `Error` は `{ ErrorCode code; std::string message; std::optional<std::filesystem::path> where; }`
- 例外を捕捉してよいのは `main.cpp` の最上位、または例外を投げる外部ライブラリとの直近境界のみ。
  後者では `tl::expected<T, Error>` かプロトコルの失敗応答へ変換する。例外を呼び出し元へ
  伝播できない worker thread の入口では、失敗状態へ変換し、join 後に `tl::expected<T, Error>`
  で返すためだけに捕捉できる。例外を握りつぶして処理を継続しない
- **エラーメッセージは必ず「どのファイルの、何が、どうダメか」を含める**
  - 悪い例: `parse error`
  - 良い例: `content/posts/2026-01-01-hello.md:3 front matter の 'date' が日付として解釈できません: '2026-13-01'`
- 1 ファイルの失敗でビルド全体を止めない。集約して最後にまとめて報告し、終了コードを非 0 にする

---

## 7. テスト

- フレームワーク: **Catch2 v3**。実行は `ctest --preset dev`
- Catch2 の外で動くテストが 2 つある。どちらも `cmake -P` のスクリプトで、3 OS で同じものが走る
  - `dist_smoke` — 実行ファイル実体に `examples/` を生成させ golden と突き合わせる。全プリセットで登録し、CLI 経路（引数解析・出力先判定・終了コード）を被覆する
  - `dist_selfcontained` — 配布バイナリの依存を許可リストと突き合わせる。`KAPPAN_DIST=ON` のときだけ登録する
- 新機能には必ずテストを添える。**テストなしの機能追加は完了と見なさない**
- ゴールデンテスト: `tests/golden/<case>/` に入力サイトと `expected/` を置き、生成結果と比較
- **フィクスチャには日本語（かな・漢字）、絵文字、半角/全角混在を必ず含める**
- 検証必須の観点:
  - UTF-8 のまま入出力されること（BOM を付けない、壊さない）
  - 日本語ファイル名・日本語見出しからの slug 生成
  - Windows の CRLF 入力を正しく扱うこと
  - front matter が壊れている場合に落ちずにエラー報告すること

---

## 8. 依存ライブラリ

`vcpkg.json` に記載されたもののみ使用する。

| 用途 | ライブラリ |
|---|---|
| Markdown | `cmark-gfm` |
| YAML（front matter / 設定） | `yaml-cpp` |
| テンプレート | `inja` + `nlohmann-json` |
| CLI 引数 | `cli11` |
| ログ | `spdlog` |
| エラー戻り値 | `tl-expected` |
| 開発サーバー | `cpp-httplib` |
| テスト | `catch2` |

**エージェントは依存を勝手に追加しない。** 必要と判断したら、代替案・バイナリサイズ影響・ライセンスを添えて提案し、承認を待つ。

---

## 9. エージェント作業プロトコル

### 着手前
1. 変更するファイル一覧と方針を **3〜10 行**で提示する
2. 承認を得てから書き始める
3. 既存の公開ヘッダ（`include/kappan/`）の API を変える場合は、先に `docs/adr/` に判断を残す

### 作業中
- **1 コミット 1 目的。** 無関係な整形・リネームを混ぜない
- 推測で API を書かない。ライブラリの関数シグネチャは実際のヘッダで確認する
- 既存コードのスタイルに合わせる。全体的な書き換えを提案なしに行わない
- `TODO` を残す場合は「理由」と「次に何をすべきか」を必ず書く

### 完了の定義（Definition of Done）
- [ ] `cmake --build --preset dev` が警告ゼロで通る
- [ ] `ctest --preset dev` が全て通る
- [ ] 新規・変更した挙動にテストがある
- [ ] 日本語を含むケースで動作確認した
- [ ] 変更内容を 5 行以内で要約できる

**上記が揃うまで「完了しました」と言わないこと。**

### コミットメッセージ
```
<type>: <日本語で 50 字以内の要約>

<なぜこの変更が必要か。何をしたかはコードを見れば分かるので、理由を書く>
```
type: `feat` / `fix` / `refactor` / `test` / `docs` / `build` / `chore`

### 言語
- 応答・コミットメッセージ・コード内コメント: **日本語**
- 識別子・ファイル名: **英語**
- 公開ドキュメント（`README` / `CHANGELOG` / `CONTRIBUTING` / `SECURITY` / `docs/spec` / `docs/adr`):
  **英語が正本**。日本語版を `*.ja.md` と `docs/ja/` に対応させて置く。
  片方だけ更新しない。挙動を変えたら `docs/spec/` の両方を、設計判断をしたら `docs/adr/` の両方を更新する
- `AGENTS.md` と `CLAUDE.md` はエージェント向けの内部ハンドブックなので日本語のまま。
  貢献者に見せる内容は `CONTRIBUTING.md` に英語で出す

---

## 10. 実装フェーズ

**前のフェーズが動くまで次に進まない。** 各フェーズの終わりに動くバイナリがあること。

| # | 内容 | 完了条件 |
|---|---|---|
| 0 | CMake / vcpkg / CI / `kappan --version` | ビルドが 3 OS で通る |
| 1 | Markdown 1 ファイル → HTML | 日本語 md が正しく変換される |
| 2 | front matter 分離 + `site.yaml` | 設定エラーが行番号付きで報告される |
| 3 | inja テンプレート + `base/post/page` 継承 | ブログ 1 記事が体裁付きで出る |
| 4 | コレクション・一覧・タグ・ページネーション | `examples/blog` が全ページ生成される |
| 5 | 静的アセット・`sitemap.xml`・RSS | 実サイトとして公開できる |
| 6 | `serve --watch` 差分ビルド | 保存 1 秒以内にブラウザ反映 |
| 7 | LP 対応（`layout: landing`、セクション部品、OGP） | `examples/landing` が生成される |
| 8 | リリース配布（3 OS の CI 緑化、自己完結バイナリ、GitHub Releases） | `v0.1.0` タグで 4 環境のアーカイブが公開される |

---

## 11. 禁止事項

- 依存ライブラリの無断追加
- 承認なしの公開 API 変更
- 警告の `#pragma` による抑制
- テストを消す・無効化することによるビルド通過
- `std::system` / 外部コマンド呼び出しによる機能実装
- 生成物（`_site/`, `build/`）のコミット
- ユーザーが指示していない「ついでのリファクタリング」
