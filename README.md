# Kappan（活版）

Markdown からブログ記事とランディングページを生成する、C++20 の単一バイナリ静的サイトジェネレーターです。

規約とアーキテクチャは [AGENTS.md](AGENTS.md) を正とします。

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

展開すると `kappan-<version>-<target>/` の下に実行ファイルと `README.md` と `LICENSE` が出ます。
実行ファイルを PATH の通ったディレクトリへ移します。

```bash
tar -xzf kappan-0.1.0-macos-arm64.tar.gz
sudo mv kappan-0.1.0-macos-arm64/kappan /usr/local/bin/
```

macOS では、配布バイナリに署名していないため（[ADR-0011](docs/adr/0011-release-distribution.md)）、
Gatekeeper が quarantine 属性を見て実行を止めます。1 度だけ属性を外してください。

```bash
xattr -d com.apple.quarantine /usr/local/bin/kappan
```

導入できたか確認します。

```bash
kappan --version   # 0.1.0
```

テーマはバイナリに埋め込まれているため、別途置くデータファイルはありません。

## ソースからビルドする場合の必要環境

- CMake 3.28+
- Ninja
- C++20 コンパイラ。`std::format` と `std::jthread` を使うため、次より古いものでは通りません
  - macOS: Xcode 26 以降（Xcode 16.4 の libc++ には `std::jthread` が無い）
  - Linux: GCC 13 以降（GCC 12 には `<format>` が無い）
  - Windows: MSVC（MinGW と Clang は vcpkg の `x64-windows` triplet と ABI が合わないため使えない）
- [vcpkg](https://github.com/microsoft/vcpkg)（リポジトリの兄弟ディレクトリ `../vcpkg`）

## ビルド

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

ローカルの vcpkg 場所を変える場合は、gitignore 対象の `CMakeUserPresets.json` で `VCPKG_ROOT` を設定してください。

## 実行

```bash
./build/dev/kappan --version
./build/dev/kappan build --source examples/blog --out _site
./build/dev/kappan serve --source examples/blog --port 8080 --watch
./build/dev/kappan new my-site
```

いま動くサブコマンドは `build`、`serve`、`new` です。

- `build` — `site.yaml` のあるサイト根 → HTML
- `serve` — 生成結果を loopback（既定 `127.0.0.1`）で配信する。`--watch` で保存後に再生成する。再生成が失敗してもサーバーは止まらず、直前に成功したページを出し続ける
- `new` — 空ディレクトリにサイト骨格を書き出す

```bash
./build/dev/kappan build --source tests/fixtures/site-ja --out /tmp/kappan-out
```
