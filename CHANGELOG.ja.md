# 変更履歴

このファイルの書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に従い、
バージョンは [セマンティック バージョニング](https://semver.org/lang/ja/) に従います。

*English (canonical): [CHANGELOG.md](CHANGELOG.md)*

## [0.1.1] - 2026-08-30

ドキュメントのみの変更です。バイナリの挙動は 0.1.0 と同じです。

### 変更

- リリースアーカイブに同梱する `README.md` を英語版に差し替えました。本リリース以降、
  英語が正本です。日本語版は `README.ja.md` として並べています
- `docs/spec/` と `docs/adr/` を英語にし、`docs/ja/` に日本語版を対応させました

### 追加

- `CONTRIBUTING.md` / `SECURITY.md` / `CODE_OF_CONDUCT.md`。いずれも日本語版付き
- issue フォームと pull request テンプレート

### 修正

- README と `AGENTS.md` が示していたコンパイラ要件が誤っていました。フェーズ 8 で
  実測した下限は macOS が Xcode 26 以降、Linux が GCC 13 以降、Windows は MSVC です。
  Xcode 16.4 には `std::jthread` が、GCC 12 には `<format>` がありません

## [0.1.0] - 2026-08-30

最初のリリース。

### 追加

- `kappan build` — `site.yaml` のあるサイト根から静的サイトを生成する。
  Markdown（GFM）と YAML front matter を読み、pretty URL で HTML を書き出す
- `kappan serve` — 生成結果を loopback で配信する。`--watch` で保存を検知して再生成し、
  ブラウザを再読み込みする。再生成に失敗しても直前の正常な世代を配信し続ける
- `kappan new` — 空ディレクトリにサイトの骨格を書き出す
- コレクション、タグ一覧、ページネーション
- 静的アセットのコピー、`sitemap.xml`、`feed.xml`
- `layout: landing` によるランディングページと、front matter の `sections:` によるセクション構成
- OGP メタ情報の出力
- 同梱テーマ（`base` / `post` / `page` / `index` / `tag` / `landing`）をバイナリへ埋め込み
- macOS / Linux / Windows 向けの自己完結バイナリを GitHub Releases で配布

### 対象環境

| アーカイブ | 動作環境 |
|---|---|
| `kappan-0.1.0-macos-arm64.tar.gz` | macOS 15 以降（Apple Silicon） |
| `kappan-0.1.0-macos-x86_64.tar.gz` | macOS 15 以降（Intel） |
| `kappan-0.1.0-linux-x86_64.tar.gz` | glibc 2.35 以降の x86_64 Linux |
| `kappan-0.1.0-windows-x86_64.zip` | Windows x64 |

### 備考

- 日本語（かな・漢字）、絵文字、半角/全角混在を含む入力を前提に検証しています
- macOS 向けバイナリは署名していません。初回実行前に quarantine 属性の解除が必要です。
  手順は README の「インストール」節にあります
- 配布方針の判断は [ADR-0011](docs/ja/adr/0011-release-distribution.md) に記録しています

[0.1.1]: https://github.com/SilentMalachite/Kappan/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/SilentMalachite/Kappan/releases/tag/v0.1.0
