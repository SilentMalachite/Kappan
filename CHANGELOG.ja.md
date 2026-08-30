# 変更履歴

このファイルの書式は [Keep a Changelog](https://keepachangelog.com/ja/1.1.0/) に従い、
バージョンは [セマンティック バージョニング](https://semver.org/lang/ja/) に従います。

*English (canonical): [CHANGELOG.md](CHANGELOG.md)*

## [Unreleased]

## [0.1.2] - 2026-08-30

### 修正

- `--force` を指定せずに非空の出力先を再利用する条件を、`.kappan-out` が通常ファイルで、
  内容が規定のバイト列と完全に一致する場合に限定しました。偽装・不正・symlink・読取不能な
  マーカーでは削除しません
- YAML のルートが mapping でない front matter を、空の mapping として扱わず、ソース位置付きで
  エラーにするよう修正しました
- `site.url` の不正な authority・host・port を拒否するようにしました。生成する絶対 URL では、
  設定した base URL の query・fragment より前に各 permalink を挿入します
- Windows の予約デバイス名になる slug の先頭にアンダースコアを加え、対応する全
  プラットフォームで同じ出力を生成できるようにしました
- `kappan new` が `landing.html` を含む同梱テンプレート6種をすべて出力するようにしました
- 自動生成するホーム（一覧の1ページ目）が、HTML の `<title>` でサイト名を重複させない
  ように修正しました

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

> **0.1.0 のアーカイブに同梱したドキュメントは 0.1.1 で置き換えられています。**
> 中の `README.md` は英語を正本に切り替える前の版で、あとから誤りと分かったコンパイラ要件が
> 書かれています。実行ファイルには影響ありません。0.1.1 はドキュメントのみの変更で挙動は
> 同一です。[v0.1.1](https://github.com/SilentMalachite/Kappan/releases/tag/v0.1.1) を
> お使いください。

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

[Unreleased]: https://github.com/SilentMalachite/Kappan/compare/v0.1.2...HEAD
[0.1.2]: https://github.com/SilentMalachite/Kappan/compare/v0.1.1...v0.1.2
[0.1.1]: https://github.com/SilentMalachite/Kappan/compare/v0.1.0...v0.1.1
[0.1.0]: https://github.com/SilentMalachite/Kappan/releases/tag/v0.1.0
