# Kappan（活版）

Markdown からブログ記事とランディングページを生成する、C++20 の単一バイナリ静的サイトジェネレーターです。

規約とアーキテクチャは [AGENTS.md](AGENTS.md) を正とします。

## 必要環境

- CMake 3.28+
- Ninja
- C++20 コンパイラ（AppleClang 15+ / GCC 13+ / MSVC 19.38+）
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

いま動くサブコマンドは `build`（`site.yaml` のあるサイト根 → HTML）です。`serve` / `new` は後のフェーズで足します。

```bash
./build/dev/kappan build --source tests/fixtures/site-ja --out /tmp/kappan-out
```
