# ADR-0007: `--out` を消してよい条件

> English (canonical): [`docs/adr/0007-out-dir-deletion-policy.md`](../../adr/0007-out-dir-deletion-policy.md)

- Status: Accepted
- Date: 2026-08-29
- 関連: [ADR-0006](0006-output-assets-feeds.md)（この ADR が `--out` の扱いを上書きする）

## 文脈

Phase 5 のレビューで、`output::prepare_out_dir` が `--out` を無条件に `remove_all` することを実測で確認した。

ADR-0006 は「`--out` は書き出し前に空にする。`--out` がソース根と同じ、またはソースが `--out` の内側なら `ErrorCode::Cli`」とだけ決めており、`docs/spec/output.md` もガードの目的を「ソースを消さないため」と書いている。つまり守る対象は `--source` だけで、**`--source` と無関係な `--out` は保護されていない**。

実測（`build/dev/kappan`）:

```
precious/photos/wedding.jpg と precious/docs/2025.pdf を置いたディレクトリに対して
kappan build --source s --out precious
→ exit=0、確認なし、警告なしで両方とも再帰削除された
```

`--out` は既定値のない必須オプション（`src/main.cpp:29`）なので、打ち間違いや未定義のシェル変数展開がそのまま復旧不能な削除になる。`prepare_out_dir` の 3 つのチェックはいずれも `src` と `out` の位置関係しか見ず、`src` が `out` の外にあれば `rel` に `..` が入って全部通過する。

単純な「非空なら拒否」は機能しない。前回のビルド出力で `--out` は常に非空になるため、毎回 `--force` が要ることになる。

## 決定

**出力根にマーカーファイルを置き、有効なマーカーが無い非空ディレクトリは消さない。**

- マーカーは `<out>/.kappan-out`。中身は固定の 1 行 `kappan output directory` + 改行。生成時刻やバージョンは入れない（ゴールデンに差分を出さないため）。
- マーカーが有効なのは、この順に、ディレクトリエントリーが symbolic link ではなく、通常ファイルであり、raw bytes が `kappan output directory\n`（24 bytes）と完全一致するときだけ。BOM 除去や改行正規化はしない。status または内容を検査できなければ `ErrorCode::Io`、不正と確認できたマーカーなら `ErrorCode::Cli` で拒否し、どちらも何も消さない。
- `prepare_out_dir` の判断順:
  1. 既存の 3 チェック（`--out` == source / source が `--out` の内側 / `--out` が既存ファイル）。ここで拒否したときは**何も消さない**。従来どおり。
  2. `--out` が存在しない → 作る。
  3. `--out` が空 → そのまま使う。
  4. `--out` が非空 かつ有効な `.kappan-out` がある → 従来どおり `remove_all` + `create_directories`。
  5. `--out` が非空 かつ `.kappan-out` が無い、または不正と確認できた → `ErrorCode::Cli` で拒否し、**何も消さない**。
  6. 出力先の空判定、マーカーの status、またはマーカーの内容を検査できない → `ErrorCode::Io` で失敗し、**何も消さない**。
- マーカーは `create_directories` の直後に書く。ビルドが途中で失敗しても次回の再ビルドが 4 に入るようにするため。
- `build` に `--force` フラグを足す。マーカー検証を迂回し、5 の場合でも削除する。`--force` は 1 の 3 チェックには効かない（ソースは常に守る）。
- 5 のメッセージは、次に何をすればよいかを含める:

  ```
  {out}: kappan の出力先ではないディレクトリが空ではありません。消してよければ --force を付けてください
  ```

## 却下した案

**前回の出力マニフェストを持ち、自分が書いたファイルだけ消す。** `remove_all` が要らなくなり出力にマーカーが混ざらない点は良いが、マニフェスト自身をどこに置くかで同じ問題に戻り、手で消されたときや部分失敗したときの整合が難しい。Phase 6 の差分ビルドでマニフェストが要るようになったら再検討する。

## 結果

- `kappan build --out ~` は初回に拒否される。これが本 ADR の目的。
- CI のように毎回新しい `--out` に書く運用は、ディレクトリが空なので影響を受けない。
- `.kappan-out` が出力に 1 件増える。ゴールデンの `tests/golden/blog-ja/expected/` にも入れる。ドット始まりなので通常の Web サーバーは配信しない。
- 利用者が `static/.kappan-out` を置いても衝突しない。`copy_static` は `.` 始まりを走査対象から外すため（`docs/spec/output.md` の静的アセット節）、そもそも出力に出ない。
- `docs/spec/output.md`（出力先の準備）と `docs/spec/cli.md`（`--force`）を書き換える。
