# ADR-0011: 配布は GitHub Releases に限り、自己完結性は ctest で検証する

> English (canonical): [`docs/adr/0011-release-distribution.md`](../../adr/0011-release-distribution.md)

- Status: Accepted
- Date: 2026-08-30
- 関連: [ADR-0001](0001-cpp20-no-modules.md)

## 文脈

AGENTS.md §1 の目標のうち「依存ゼロで配布できる単一実行ファイル（macOS / Windows / Linux）」だけが未達だった。
フェーズ 0〜7 で機能は揃っているが、利用者が vcpkg と CMake と C++20 コンパイラを用意せずに使う手段が無い。

着手時の調査で、前提も崩れていた。直近 5 回の CI がすべて失敗しており、Linux は libstdc++ で見えない
インクルードと `-Wmissing-field-initializers` で、Windows は MSVC ではなく runner の MinGW が拾われていた。
**Linux と Windows では `ctest` が一度も実行されたことが無かった。** 赤い CI の上に配布を積むことはできないため、
まず 3 OS を緑に戻した（8a）。その過程で、CRLF によるゴールデン破壊、Windows で開いたままのファイルを
削除できない問題、triplet によって cmark-gfm のターゲット名が変わる問題が判明した。

決めるべきことは 3 つある。どこに配るか、どの環境を対象にするか、自己完結性をどう保証するか。

## 決定

### 1. 配布先は GitHub Releases に限る

`v*` タグの push で `.github/workflows/release.yml` が動き、4 環境ぶんのアーカイブと `SHA256SUMS` を公開する。
Homebrew tap、Scoop bucket、`curl | sh` のインストールスクリプト、apt / dnf / winget への登録は行わない。

### 2. 対象は 4 環境。下限は実測して固定する

| target | runner | 下限 |
|---|---|---|
| `macos-arm64` | `macos-15` + Xcode 26.3 | macOS 15 |
| `macos-x86_64` | `macos-15-intel` + Xcode 26.3 | macOS 15 |
| `linux-x86_64` | `ubuntu-22.04` + g++-13 | glibc 2.35 |
| `windows-x86_64` | `windows-latest` + MSVC | Windows 8 相当の API セット |

Linux / Windows の arm64 は対象にしない。

runner の既定のツールチェーンでは、この下限が成り立たなかった。

- `ubuntu-22.04` の GCC は最大 12.3 で `<format>` が無い（GCC 13 以降）。`ubuntu-24.04` に上げれば通るが
  glibc の下限が 2.39 になるため、runner は 22.04 のまま `ubuntu-toolchain-r/test` の g++-13 を入れる
- `macos-15` 系の既定 Xcode 16.4 の libc++ には `std::jthread` / `std::stop_token` が無い。`macos-26` の
  runner に上げれば通るが macOS の下限が 26 になるため、runner は 15 のまま Xcode 26.3 を選ぶ

### 3. 自己完結性は ctest のテストとして検証する

`cmake/dist_selfcontained.cmake` を `cmake -P` で実行し、`ldd` / `otool -L` / `dumpbin /dependents` の出力を
許可リストと突き合わせる。`KAPPAN_DIST=ON` のときだけ登録する。CI のシェルステップには置かない。

`cmake/dist_smoke.cmake` は実行ファイル実体に `examples/` を生成させ、`tests/golden/*/expected` と
バイト単位で突き合わせる。こちらは `KAPPAN_DIST` の有無にかかわらず登録し、`ctest --preset dev` でも走らせる。

### 4. リンク設定は `KAPPAN_DIST` にまとめる

| プラットフォーム | 設定 |
|---|---|
| MSVC | vcpkg は `x64-windows-static` triplet、CRT は `MultiThreaded`（`/MT`） |
| Linux | `-static-libstdc++ -static-libgcc`。glibc は動的のまま |
| macOS | 追加なし |

### 5. macOS のバイナリは署名も公証もしない

## 理由

- **パッケージマネージャ**への登録は、別リポジトリの作成とマニフェスト更新の自動化を伴う。配布導線が実際に
  動くことを確かめる前に増やす保守対象ではない
- **arm64 の Linux / Windows** は、こちらで動作を確認できる環境が無い。検証できないものを配らない
- **glibc ごと静的リンク**すると `getaddrinfo` まわりで警告が出るため、`serve` の追加検証が必須になる。
  glibc 2.35 を下限にすれば実用上足りる。libstdc++ と libgcc だけ静的にすれば、ビルド環境より新しい
  GCC を実行側に要求しない
- **署名**には Apple Developer Program の年額費用と鍵の運用が要る。受け取る側の手間の差は
  `xattr -d com.apple.quarantine` の 1 コマンドであり、費用に見合わない
- **検証を CI のシェル**に書くと手元で再現できず、bash 版と pwsh 版の二重管理になる。ctest に置けば
  同じ検証が両方で回り、配布物の検証がテスト資産として残る
- **バージョン整合の関門**を置くのは、`project(kappan VERSION ...)` と `vcpkg.json` の更新漏れが
  リリース時点でしか露見しないため。タグから決まる値と `kappan --version` の出力を突き合わせて止める

## 却下した案

**検証を `release.yml` のシェルステップに直接書く。** 手元で再現できず、bash 版と pwsh 版の二重管理になる。
配布物の検証がテスト資産として残らない。

**goreleaser などの外部リリースツール。** Go 前提で vcpkg の C++ ビルドに噛み合わない。AGENTS.md §11 の
依存追加に当たる。

**musl で完全 static。** 可搬性は最高だが、vcpkg の triplet とビルド環境を新設する必要があり、得られる
可搬性に対して重い。

**`ubuntu-24.04` に上げて glibc 2.39 を下限にする。** ワークフローは単純になるが、Ubuntu 22.04 と
Debian 12 の利用者が外れる。PPA を 1 つ足すほうが失うものが小さい。

**`macos-26` 系の runner を使う。** 同じく単純だが、macOS の下限が 26 になる。runner の OS はビルド環境で
あって配布対象ではないので、Xcode だけ選び直すほうが筋が良い。

**最初から `v1.0.0` を打つ。** 配布導線が実際に動くことを未確認のまま v1 を名乗ることになる。
`v0.1.0` で一度通してから v1 を打つ。

**macOS を universal binary にまとめる。** CI での lipo 結合工程が増える。arm64 と x86_64 を個別に配れば
要求は満たせる。

## 結果

- `v0.1.0` タグの push だけで、4 アーカイブと `SHA256SUMS` が Releases に載る
- 実測した依存は次のとおりで、いずれも対象環境に元から在るものだけだった

  | 環境 | 依存 |
  |---|---|
  | macOS | `CFNetwork`, `CoreFoundation`, `/usr/lib/libc++.1.dylib`, `/usr/lib/libSystem.B.dylib` |
  | Linux | `libm.so.6`, `libc.so.6`, `ld-linux-x86-64.so.2`（要求する glibc シンボルは最大 `GLIBC_2.35`） |
  | Windows | `KERNEL32.dll`, `WS2_32.dll`, `api-ms-win-core-synch-l1-2-0.dll` |

- macOS の利用者は、初回に `xattr -d com.apple.quarantine ./kappan` が必要になる。README に手順を書く
- glibc 2.35 未満の環境（RHEL 9 は 2.34）ではソースからビルドする必要がある
- `ubuntu-toolchain-r/test` PPA と `ilammy/msvc-dev-cmd` に依存する。後者はコミット SHA で固定する
- 8a で `.gitattributes` を入れ、ワークツリーの改行を LF に固定した。Windows の checkout による
  CRLF 変換が、バイト単位で比較するゴールデンと、バイナリへ埋め込むテーマを壊していたため
