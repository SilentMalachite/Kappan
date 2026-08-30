# ADR-0002: 想定内エラーは `tl::expected`、例外は main のみ

> English (canonical): [`docs/adr/0002-error-handling.md`](../../adr/0002-error-handling.md)

- Status: Accepted
- Date: 2026-08-28

## 文脈

静的サイト生成は「1 ファイルの YAML が壊れている」ような想定内失敗が日常的に起きる。例外で制御を飛ばすと、どのファイルがダメだったかを集約しにくい。一方、メモリ不足のような想定外は例外のままが自然である。

## 決定

- 想定内エラーは `tl::expected<T, kappan::Error>`（エイリアス `kappan::Result<T>`）を返す。ライブラリコードは例外を投げない。
- `Error` は `{ code, message, where, line }`。message は「どのファイルの、何が、どうダメか」を日本語で書く。
- 1 ファイルの失敗でビルド全体を止めない。パイプラインは `std::vector<Error>` に集約し、最後にまとめて報告する。1 件でもあれば終了コードは非 0。
- 例外を捕捉してよいのは `src/main.cpp` の最上位のみ。

Phase 0 ではまだ `tl-expected` を依存に入れない。型は Phase 1 で `include/kappan/error.hpp` に置く。

## 結果

- CLI は複数エラーを列挙できる。
- テストは `Result` の成功/失敗を直接検証できる。
- 想定外（OOM など）は main で捕まえて非 0 終了する。
