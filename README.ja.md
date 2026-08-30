# Kappan（活版）

[![CI](https://github.com/SilentMalachite/Kappan/actions/workflows/ci.yml/badge.svg)](https://github.com/SilentMalachite/Kappan/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/SilentMalachite/Kappan?sort=semver)](https://github.com/SilentMalachite/Kappan/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](docs/ja/adr/0001-cpp20-no-modules.md)

Markdown からブログ記事とランディングページを生成する、日本語のための C++20 単一バイナリ静的サイトジェネレーターです。

*English (canonical): [README.md](README.md)*

## Kappan の特徴

- **実行ファイル 1 つ、ランタイム不要。** Node も Python も Ruby も要りません。既定テーマはバイナリに埋め込んであるので、隣に置くデータファイルもありません。
- **日本語が主役。** かな・漢字・絵文字・半角全角の混在が、ファイル名から slug、URL まで壊れずに通ります。ローマ字化はせず、日本語 URL をパーセントエンコードもしません。
- **どのファイルが悪いか言うエラー。** 想定内の失敗は「どのファイルの、何が、どうダメか」を行番号付きで出します。1 ファイルの失敗でビルドを止めず、全件を集約して最後に報告します。
- **嘘をつかないプレビュー。** `serve --watch` は保存で再生成し、失敗したときは壊れたページではなく直前の正常な世代を配信し続けます。
- **バイト単位で固定された出力。** ゴールデンテストが生成物をバイト単位で比較します。配布バイナリが通る CLI 経路も対象です。

## インストール

[Releases](https://github.com/SilentMalachite/Kappan/releases) から、環境に合うアーカイブを取得します。

| 環境 | アーカイブ |
|---|---|
| macOS 15 以降（Apple Silicon） | `kappan-<version>-macos-arm64.tar.gz` |
| macOS 15 以降（Intel） | `kappan-<version>-macos-x86_64.tar.gz` |
| Linux x86_64（glibc 2.35 以降） | `kappan-<version>-linux-x86_64.tar.gz` |
| Windows x64 | `kappan-<version>-windows-x86_64.zip` |

同じページの `SHA256SUMS` を並べて置き、取得したファイルを検証します。

```bash
sha256sum -c SHA256SUMS --ignore-missing   # macOS は shasum -a 256 -c
```

展開すると `kappan-<version>-<target>/` の下に実行ファイルと `README.md` と `LICENSE` が出ます。実行ファイルを PATH の通ったディレクトリへ移します。

```bash
tar -xzf kappan-0.1.0-macos-arm64.tar.gz
sudo mv kappan-0.1.0-macos-arm64/kappan /usr/local/bin/
```

macOS では、配布バイナリに署名していないため（[ADR-0011](docs/ja/adr/0011-release-distribution.md)）、Gatekeeper が quarantine 属性を見て実行を止めます。1 度だけ属性を外してください。

```bash
xattr -d com.apple.quarantine /usr/local/bin/kappan
```

導入できたか確認します。

```bash
kappan --version   # 0.1.0
```

テーマはバイナリに埋め込まれているため、別途置くデータファイルはありません。

## はじめかた

```bash
kappan new my-site
kappan serve --source my-site --watch     # http://127.0.0.1:8080
kappan build --source my-site --out _site
```

サイト根は `site.yaml` のあるディレクトリです。

```
my-site/
├── site.yaml          # title / url / language / pagination
├── content/
│   ├── index.md       # ホーム（layout: index）
│   ├── about.md       # → /about/
│   └── posts/
│       └── 2026-01-01-hello.md   # → /posts/hello/
├── static/            # 出力根へそのままコピーされる
└── templates/         # 任意。同梱テーマを上書きする
```

## サブコマンド

| コマンド | 内容 |
|---|---|
| `kappan build --source <dir> --out <dir>` | サイト根 → HTML。`static/`・`sitemap.xml`・`feed.xml` も出す |
| `kappan serve --source <dir> [--watch]` | loopback で配信。`--watch` で保存後に再生成しブラウザを再読み込みする |
| `kappan new <dir>` | 空ディレクトリにサイト骨格を書き出す |

`--drafts` は `draft: true` を含めます。`--force` は kappan が作ったものでない出力先を `build` が空にすることを許します。詳細は [docs/ja/spec/cli.md](docs/ja/spec/cli.md) にあります。

```bash
kappan build --source examples/blog --out _site
kappan build --source examples/landing --out _site
```

## ドキュメント

英語が正本で、各ページから日本語版へリンクしています。

| 内容 | ページ |
|---|---|
| CLI リファレンス | [docs/ja/spec/cli.md](docs/ja/spec/cli.md) |
| `site.yaml` | [docs/ja/spec/site-yaml.md](docs/ja/spec/site-yaml.md) |
| Front matter | [docs/ja/spec/front-matter.md](docs/ja/spec/front-matter.md) |
| コレクション・タグ・ページネーション | [docs/ja/spec/collections.md](docs/ja/spec/collections.md) |
| テンプレートと変数 | [docs/ja/spec/templates.md](docs/ja/spec/templates.md) |
| ランディングページと OGP | [docs/ja/spec/landing.md](docs/ja/spec/landing.md) |
| 出力・sitemap・RSS | [docs/ja/spec/output.md](docs/ja/spec/output.md) |
| アーキテクチャ決定記録 | [docs/ja/adr/](docs/ja/adr/) |
| 変更履歴 | [CHANGELOG.ja.md](CHANGELOG.ja.md) |
| 貢献の手引き | [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md) |

## ソースからビルドする

必要環境:

- CMake 3.28+
- Ninja
- C++20 コンパイラ。`std::format` と `std::jthread` を使うため、次より古いものでは通りません
  - macOS: Xcode 26 以降（Xcode 16.4 の libc++ には `std::jthread` が無い）
  - Linux: GCC 13 以降（GCC 12 には `<format>` が無い）
  - Windows: MSVC（MinGW と Clang は vcpkg の `x64-windows` triplet と ABI が合わない）
- [vcpkg](https://github.com/microsoft/vcpkg)（リポジトリの兄弟ディレクトリ `../vcpkg`）

```bash
# 初回のみ：vcpkg の取得
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
../vcpkg/bootstrap-vcpkg.sh          # Windows は bootstrap-vcpkg.bat

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

リリースビルド:

```bash
cmake --preset release
cmake --build --preset release
```

リリースアーカイブと同じ自己完結バイナリを作る手順は [CONTRIBUTING.ja.md](CONTRIBUTING.ja.md#配布ビルド) にあります。

ローカルの vcpkg 場所を変える場合は、gitignore 対象の `CMakeUserPresets.json` で `VCPKG_ROOT` を設定してください。

## セキュリティ

Kappan は cmark-gfm の `tagfilter` 拡張を有効にしており危険な HTML タグは無効化されますが、それ以外の生 HTML は本文から素通りします。`content/` の中身は信頼できる入力として扱ってください。脆弱性の報告方法は [SECURITY.ja.md](SECURITY.ja.md) にあります。

## ライセンス

[MIT](LICENSE)
