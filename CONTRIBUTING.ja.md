# Kappan への貢献

関心を持っていただきありがとうございます。変更を取り込むまでに必要なことをまとめます。

*English (canonical): [CONTRIBUTING.md](CONTRIBUTING.md)*

このリポジトリの実装規約の正は `AGENTS.md` です。本ページより踏み込んだ内容が書かれており、食い違う場合は `AGENTS.md` を優先します。

## 着手する前に

- バグは、最小の再現手順を添えて issue を立ててください。入力サイト、実行したコマンド、期待した結果、実際の結果の 4 点です。
- 機能追加は、先に issue を立ててください。Kappan は意図的に範囲を小さく保っており、コードの前に設計を話すほうが双方の手間が減ります。
- `include/kappan/` の公開ヘッダを変える場合は、コードを書く前に `docs/adr/` へ判断を残してください。

## 開発環境の準備

```bash
git clone https://github.com/SilentMalachite/Kappan.git
cd Kappan

# vcpkg は兄弟ディレクトリに置く
git clone https://github.com/microsoft/vcpkg.git ../vcpkg
../vcpkg/bootstrap-vcpkg.sh          # Windows は bootstrap-vcpkg.bat

cmake --preset dev
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

別の場所の vcpkg を使うときは、gitignore 対象の `CMakeUserPresets.json` で `VCPKG_ROOT` を設定します。

**プリセット外のビルドコマンドを新設しないでください。** 必要なら `CMakePresets.json` に追加します。

| プリセット | 用途 |
|---|---|
| `dev` | Debug ビルド。手元では警告をエラーにしない。CI では有効にする |
| `release` | 最適化ビルド。警告をエラーにする |
| `dist` / `dist-windows` | 配布用の自己完結ビルド（後述） |

## コーディング規約

- **C++20。modules と coroutines は使わない**（[ADR-0001](docs/ja/adr/0001-cpp20-no-modules.md)）。
- **想定内エラーは `kappan::Result<T>`（`tl::expected`）を返す。** ライブラリコードは例外を投げず、捕捉してよいのは `src/main.cpp` の最上位だけです（[ADR-0002](docs/ja/adr/0002-error-handling.md)）。メッセージは「どのファイルの、何が、どうダメか」を行番号付きで書きます。
- **所有は値と `std::unique_ptr` のみ。** 生 `new` / `delete` は禁止です。非所有の参照は `const T&` / `std::string_view` / `std::span` を使います。
- **パスは必ず `std::filesystem::path`。** 文字列連結でパスを組み立てないでください。
- ループより **`std::ranges` と views**、SFINAE より **concepts**、`printf` や iostream の書式より **`std::format`** を使います。
- 型は `PascalCase`、関数・変数は `snake_case`、メンバは末尾 `_`、定数は `kPascalCase`。ヘッダは `#pragma once`。1 関数 50 行、1 ファイル 400 行を目安にします。
- **警告を `#pragma` で黙らせないでください。** `-Werror` や `/WX` を外して回避するのも同じです。どうしても消せない警告は、理由をコメントに残したうえで issue か PR で相談してください。
- 整形は `cmake --build --preset dev --target format` で行い、手作業ではしないでください。
- 識別子とファイル名は英語です。コメントとドキュメントは、編集しているファイルの言語に合わせます。

## テスト

テストは Catch2 v3 で、`ctest --preset dev` で実行します。

- **テストなしの機能追加は完了と見なしません。** 新規・変更した挙動には必ずテストを添えてください。
- ゴールデンテストは `tests/golden/<case>/` に入力サイトと `expected/` を置き、生成結果とバイト単位で比較します。
- **フィクスチャには日本語（かな・漢字）、絵文字、半角/全角混在を必ず含めてください。** これは任意ではなく、Kappan が約束していることの中核です。
- 次は常に検証してください。UTF-8 のまま入出力されること（BOM を付けない・壊さない）、日本語ファイル名と日本語見出しからの slug 生成、Windows の CRLF 入力、front matter が壊れているときに落ちずに報告すること。

Catch2 のバイナリ外で動くテストが 2 つあります。

| テスト | 内容 |
|---|---|
| `dist_smoke` | ビルドした実行ファイルに `examples/` を生成させ、ゴールデンと比較する。全プリセットで登録され、CLI 経路を被覆する |
| `dist_selfcontained` | 配布バイナリの共有ライブラリ依存を許可リストと突き合わせる。`KAPPAN_DIST=ON` のときだけ登録される |

## 配布ビルド

リリースアーカイブの自己完結バイナリは `dist` プリセットで作ります。

```bash
cmake --preset dist                 # Windows は --preset dist-windows
cmake --build --preset dist
ctest --preset dist --output-on-failure
cmake --install build/dist --prefix stage
```

Linux では libstdc++ と libgcc を静的リンクし、Windows では vcpkg の `x64-windows-static` triplet と `/MT` を使います。`ctest --preset dist` には `dist_selfcontained` が含まれ、許可リスト外の依存が付いたら落ちます。理由は [ADR-0011](docs/ja/adr/0011-release-distribution.md) にあります。

## 依存ライブラリ

**合意なしに依存を追加しないでください。** 依存は vcpkg の manifest モード（`vcpkg.json`）で管理しています。必要と判断した場合は、検討した代替案・バイナリサイズへの影響・ライセンスを添えて issue を立ててください。

## コミットと Pull Request

1 コミット 1 目的です。無関係な整形やリネームを混ぜないでください。

```
<type>: <日本語で 50 字以内の要約>

<なぜこの変更が必要か。何をしたかはコードを見れば分かるので、理由を書く>
```

`type` は `feat` / `fix` / `refactor` / `test` / `docs` / `build` / `chore` / `perf` / `ci` のいずれかです。

Pull Request を出す前に、次がすべて揃っていることを確認してください。

- [ ] `cmake --build --preset dev` が警告ゼロで通る
- [ ] `ctest --preset dev` が全て通る
- [ ] 新規・変更した挙動にテストがある
- [ ] 日本語を含むケースで動作確認した
- [ ] 変更内容を 5 行以内で要約できる

CI は macOS / Linux / Windows で警告をエラーとしてビルドします。3 つとも緑である必要があります。

## ドキュメント

英語が正本です。`docs/spec/` と `docs/adr/` に英語版を置き、`docs/ja/` が日本語版を対応させています。

挙動を変えたら `docs/spec/` の該当ページを更新してください。設計判断をしたら `docs/adr/` に ADR を追加してください。同じ PR で日本語版も更新してもらえると助かりますが、難しければその旨を説明に書いていただければこちらで追随します。

## 行動規範

参加は [行動規範](CODE_OF_CONDUCT.md) に従います。
