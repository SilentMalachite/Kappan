# ADR-0001: C++20 を使い、modules は使わない

> English (canonical): [`docs/adr/0001-cpp20-no-modules.md`](../../adr/0001-cpp20-no-modules.md)

- Status: Accepted
- Date: 2026-08-28

## 文脈

Kappan は macOS / Windows / Linux で同じソースから単一バイナリを出す。C++20 は `std::format`、`std::filesystem`、concepts、`std::span` を標準で使える。C++20 modules はコンパイラと CMake / vcpkg の対応差が大きい。

## 決定

言語は **C++20**。**modules は使わない**。ヘッダ + 翻訳単位の従来構成にする。coroutines も v1 では使わない。

## 結果

- README に挙げたツールチェーンで同じコードが通る前提を置ける。
- 公開 API は `include/kappan/*.hpp` に置き、実装は `src/` に置く。
- modules に移行する場合は、この ADR を置き換える。

> 注記（2026-08-30）: 本 ADR は当初 AppleClang 15+ / GCC 13+ / MSVC 19.38+ と書いていた。
> フェーズ 8 で実際の下限を実測し、macOS はこれより高いことが分かった。README と [ADR-0011](0011-release-distribution.md) を見ること。
