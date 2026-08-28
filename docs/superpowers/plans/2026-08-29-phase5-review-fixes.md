# Phase 5 レビュー指摘の修正タスク

**出典:** Phase 5（`7b882d4..6babf4b`、8 コミット、20 ファイル、+940/-42）のレビューで挙がった 15 件を、Opus が実バイナリ `build/dev/kappan` と libc++ プローブで全件再検証したもの。取り下げはゼロ。

そのうち 3 件は `docs/spec/output.md` の書き換えを伴うため [ADR-0007](../../adr/0007-out-dir-deletion-policy.md) / [ADR-0008](../../adr/0008-sitemap-lastmod-format.md) / [ADR-0009](../../adr/0009-feed-item-source.md) に切り出した。**本ファイルは残り 12 件（実装のみで閉じるもの）。**

## 進め方

- 1 タスク 1 コミット（AGENTS.md §9）。番号順に進める。重大度の高い順に並べてある。
- 各タスクの「現象」は実測結果。推測ではないので、再現手順として使える。
- 完了の定義は AGENTS.md §9 の Definition of Done。`cmake --build --preset dev` が警告ゼロ、`ctest --preset dev` が全通過。
- 順序の制約: **タスク 9 はタスク 5 より先**（重複を消してから制御文字を直す。でないと 2 箇所直すことになる）。**タスク 10 はタスク 11 より先**。それ以外は独立。
- 公開ヘッダ `include/kappan/` は変えない。依存ライブラリを追加しない（AGENTS.md §8, §11）。

---

## 1. 出力パスの `..` を封じる 【最優先】

**Files:** `src/util/slug.cpp`、`src/output/write.hpp` / `write.cpp`、`src/content/build.cpp`

**現象（実測）**

```
記事に slug: ".."      → out/index.html がその記事に無言で上書きされる（exit=0、エラーなし）
固定ページに slug: ".." → --out の親ディレクトリに index.html が生成される（exit=0、エラーなし）
```

**原因の連鎖**

1. `slug.cpp:33` の `is_reserved` は `0 < > : " / \ | ? *` だけで `.` を含まない → `slugify("..") == ".."`
2. `parse.cpp:255` でユーザー指定 slug も `slugify` を通るが、`..` はそのまま残る
3. `parse.cpp:290` → permalink `/posts/../`
4. `util::output_from_permalink` は正規化しない → `posts/../index.html`
5. `util::to_generic_utf8` は `generic_u8string()` を返すだけ → claim キーはリテラル文字列。ホームの `index.html` と一致せずガードを通過
6. `write_utf8_file` は `ofstream(trunc)` なので OS が `..` を解決して書く

slug 内の `/` は予約文字なので、脱出は `--out` の 1 階層上までに限られる。それでも `--out` の外である。

**変更**

- (a) `util::try_slugify` — 結果が `.` だけで構成される場合は `std::nullopt` を返す（`.` `..` `...` を弾く）。`.` 自体は予約文字にしない（`v1.2` のような slug を壊さないため。ADR-0004 の予約文字規則は変えない）
- (b) `output::contains_dotdot` を `write.hpp` に公開する（現在は `write.cpp` の無名名前空間）
- (c) `content::record_page` — claim の前に `contains_dotdot(page.output_path)` を見て、真なら `ErrorCode::Path` を積んで書かない。`write_xml` も同じ経路に揃える（`name` は ASCII 固定なので実質通らないが、抜け道を残さない）

  ```
  {source}: 出力パス '{output_path}' が --out の外を指しています
  ```

**テスト**

- `tests/unit/test_slug.cpp`: `try_slugify(".")` と `try_slugify("..")` が `nullopt`。`try_slugify("v1.2")` は `"v1.2"` のまま
- `tests/unit/test_build_site.cpp`: 記事 `slug: ".."` で `ErrorCode::Path` が出て `out/index.html` がホームのままであること。固定ページ `slug: ".."` で `--out` の親に何も作られないこと

---

## 2. 大小文字畳み込みによる無言上書きを塞ぐ

**Files:** `src/content/build.cpp`（`write_page`）

**現象（実測、この macOS / APFS 上）**

キリル文字の大文字・小文字だけが違う slug の記事 2 本（`Аbc` = U+0410、`аbc` = U+0430）を置くと、claim キーは別物なので衝突は報告されず `exit=0`。実際には `out/posts/` にディレクトリが 1 つしかできず、後に書いた方だけが残り、もう 1 本は完全に消える。

`docs/spec/output.md` §衝突 の「黙って上書きしない。`ErrorCode::Path` を集約する」に真っ向から反する。

**原因**

`ClaimedOutputs` のキーは大小文字を区別する `std::string`（`write.hpp:12`）。APFS と Windows は非区別。`slugify` は ASCII `A-Z` しか小文字化しない（`slug.cpp:51`）。ADR-0004 のとおり Unicode 正規化もしない。

**変更**

`write_page` が `util::write_utf8_file` を呼ぶ直前に、解決後の宛先 `out_dir / page.output_path` の存在を `std::filesystem::exists(dest, ec)` で確認する。存在したら `ErrorCode::Path` を積んで書かない。`prepare_out_dir` が `--out` を空にしているので、**存在する＝このビルドが既に書いた＝衝突**。

キー正規化ではなく存在確認にする理由: ケースフォールドを自前で持つには ICU が要る（AGENTS.md §8 で依存追加は禁止）。存在確認なら大小文字だけでなく APFS の NFC/NFD エイリアスも同時に捕まえられる。文字列マップは残す（速く、衝突相手を名指しできる良いメッセージが出る）。存在確認は最後の砦。

`copy_static` 側は `copy_file` が既定で上書きしないため既にエラーになる。手当ては不要。

**テスト**

`tests/unit/test_build_site.cpp`: キリル大文字/小文字 slug の記事 2 本で `result.ok()` が false、`ErrorCode::Path`、片方の HTML が残ること。

---

## 3. `increment` のエラーを取りこぼさない

**Files:** `src/output/assets.cpp:72-76`

**現象（実測。Apple clang 21.0.0 / libc++ への直接プローブ）**

同じ `for` 形式で読めないサブディレクトリを含む木を走査させると:

1. `increment(ec)` は `ec = 13 Permission denied` を返す
2. ループ先頭の `if (ec)` は**一度も実行されない**
3. ループ脱出後に `it == end` が YES

未訪問のエントリは黙って捨てられる。`copy_static` は空のエラーベクタを返し、`static/` の末尾が丸ごと欠けたまま `result.ok() == true` になる。

`skip_permission_denied` があるので EACCES 自体は飛ばされるが、走査中のディレクトリ削除・ELOOP・I/O エラー・ネットワークマウントの断絶は同じ経路を通る。

**変更**

`for` を `while` に変え、`increment` の**直後**に `ec` を見る。

```cpp
const auto end = std::filesystem::recursive_directory_iterator{};
while (it != end) {
  // ... 本体 ...
  it.increment(ec);
  if (ec) {
    errors.push_back(scan_error(static_dir, ec));
    break;
  }
}
```

**テスト**

決定的に再現させるため、`copy_static` に走査オプションの引数を足す（既定は現行の `skip_permission_denied`）。テストは `directory_options::none` を渡し、`chmod 000` のサブディレクトリを置いて `increment` を失敗させ、`errors.size() == 1` と `ErrorCode::Io` を確認する。root で走ると権限が効かないので、テストは `geteuid() != 0` でガードする。

---

## 4. `std::filesystem` の投げるオーバーロードをやめる

**Files:** `src/output/assets.cpp:78, 82`

**現象（実測）**

`static/` にシンボリックリンクがあり、その `stat` が ENOENT/ENOTDIR 以外で失敗すると、未捕捉の `std::filesystem_error` が `copy_static` と `build_site` を貫通し `main.cpp:60` の `catch` まで飛ぶ。

```
static/loop -> loop（ループ）
  → filesystem error: in posix_stat: ... Too many levels of symbolic links
実行権のないディレクトリ下を指すリンク
  → filesystem error: in posix_stat: ... Permission denied
```

どちらも stderr に生の libc++ メッセージが出て `exit=1`、`out_dir` は半端なまま残る。

> **注:** ダングリングシンボリックリンクは ENOENT で `not_found` が返るだけなので例外にならない（実測で `exit=0`）。当初のレビューはここを誤っていた。引き金は「リンクであること」ではなく「リンク先の stat が ENOENT/ENOTDIR 以外で失敗すること」。

**違反していた規約:** AGENTS.md §6 の 3 行 —— 154（例外を投げない）、156（捕捉は `main.cpp` のみ）、160（1 ファイルの失敗でビルド全体を止めない）。

**変更**

`it->is_directory()` / `it->is_regular_file()` を `error_code` 版に置き換え、`ec` が立ったら `Error` を積んで `continue`。

**テスト**

`tests/unit/test_output.cpp`: `static/` にシンボリックリンクループを作り、`copy_static` が例外を投げずに 1 件の `Error` を返し、同じ `static/` 内の通常ファイルはコピーされること。

---

## 5. XML エスケープで不正な制御文字を落とす

> タスク 9（重複の解消）を先にやること。でないと同じ修正を 2 箇所ですることになる。

**Files:** `src/output/xml.cpp:39-65`（タスク 9 の統合後のヘルパ）

**現象（実測）**

本文に生の `0x0C` を 1 文字入れた記事があると `exit=0`・警告なしでビルドが通り、`xmllint` が feed 全体を拒否する。

```
out/feed.xml:13: parser error : PCDATA invalid Char value 12
```

1 記事の 1 バイトで全購読者のフィードが壊れる。front matter に `description` が無いと本文がそのまま `<description>` に入る（`xml.cpp:13-15`）ので経路は短い。

**変更**

XML 1.0 の `Char` 生成規則が許さないバイト（タブ `0x09`・LF `0x0A`・CR `0x0D` を除く `0x00-0x1F`）を出力しない。**文字参照にしても不正**なので、エスケープではなく除去する。

> **訂正:** この計画の初版は `0x7F` も除去対象に挙げていたが誤り。`0x7F` は `[#x20-#xD7FF]` に入るので XML 1.0 では正当な文字。`0x80` 以上も同様に正当で、しかも UTF-8 の続きバイトなので絶対に触らないこと。

除去は黙って行う（警告を出さない）。フィード全体を壊すより良い。この判断の理由をコード上のコメントに残すこと。

**テスト**

- `tests/unit/test_output.cpp`: エスケープ関数が `"前\x0C後"` から制御文字を落とし、タブ・改行は残すこと
- `tests/unit/test_build_site.cpp`: `0x0C` を含む記事を置いたビルドの `feed.xml` に `0x0C` が無いこと

---

## 6. ドライブ跨ぎの `--out` を誤って拒否しない（Windows）

**Files:** `src/output/write.cpp:87-90`

**現象**

`kappan build --source C:\site --out D:\out` が `ErrorCode::Cli`「`--out` がソースディレクトリと同じです」で拒否される。誤った拒否であり、同時に事実と違う状態を述べるメッセージでもある。

**原因**

`[fs.path.gen]` により `root_name` が異なると `lexically_relative` は `path()` を返す。88 行目の `rel.empty()` がそれを `same_out_error` に写している。空の `rel` は「無関係なパス」を意味するので、同一として扱わず許可すべき。

POSIX では `root_name` が常に空なので発現しない。**macOS 上では実行検証できていない**（コード読解と規格の条文による確定）。Windows が対象プラットフォームである根拠は AGENTS.md:16。

**変更**

`rel.empty()` を `same_out_error` の条件から外す。同一判定は 83 行目の `*src == *out` で足りている。`rel == "."` は同一なので残す。`rel` が空なら「無関係」として先へ進む。

**テスト**

`root_name` を持つ環境が要るのでユニットテストは書けない。`#ifdef _WIN32` でガードしたケースを `tests/unit/test_output.cpp` に置くか、CI の Windows ジョブでの結合テストにするか、実装時に判断してよい。

---

## 7. `static/` のドットファイルをコピーしない

**Files:** `src/output/assets.cpp:78-84`、`docs/spec/output.md`

**現象（実測）**

`examples/blog` をビルドした出力は `tests/golden/blog-ja/expected` と素で一致する。ところが `examples/blog/static/images/.DS_Store` を置いて同じことをすると出力に `./images/.DS_Store` が増え、`test_golden.cpp` の `REQUIRE(actual_files == expected_files)` が落ちる。

`.DS_Store` はリポジトリの `.gitignore` に入っているので `git status` にも現れず、**ソース変更では説明のつかない失敗**になる。macOS の Finder で一度開けば起きる。

**原因**

78 行目の `_` プレフィックススキップはディレクトリだけが対象。ドットファイル/ドットディレクトリのフィルタが実装にも仕様にも無い。

**変更**

- `.` で始まる名前はファイル・ディレクトリとも走査対象から外す。ディレクトリは `disable_recursion_pending`。ADR-0007 の `.kappan-out` と規則が揃う
- `docs/spec/output.md` の静的アセットの箇条書きに 1 行足す:

  > - `.` で始まるファイル・ディレクトリはコピーしない（`.DS_Store` などを出力に混ぜない）。

**テスト**

`tests/unit/test_output.cpp` の `copy_static` ケースに `.DS_Store` と `.git/` を置き、出力に出ないことを確認。

---

## 8. `error_code` を無視して誤ったメッセージを出さない

**Files:** `src/output/assets.cpp:61`、`src/output/write.cpp:48`

**現象**

`is_directory(static_dir, ec)` が失敗した場合（親の権限、I/O エラー、断絶したネットワークマウント）、`ec` を見ずに `false` を真に受けて「`static` はディレクトリである必要があります」を出す。`static/` はごく普通のディレクトリなのに、利用者は実際の原因に到達できない。AGENTS.md §6 の「どのファイルの、何が、どうダメか」の「何が」が間違っている。

同じパターンが `write.cpp:48` の `exists(out, ec) && is_regular_file(out, ec)` にもある。ここは 2 つの呼び出しが同じ `ec` を共有していて後の呼び出しが前の `ec` を上書きする点も併せて直す。

**変更**

両方とも `ec` を見て、立っていたら `ErrorCode::Io` に `ec.message()` を載せて返す。

**テスト**

挙動が変わるのはエラーメッセージだけ。安価に再現できるならテストを足し、難しければコミットメッセージに「テストを書かなかった理由」を書く。

---

## 9. `xml_escape` と `html_escape` を 1 つにする

> タスク 5 より先にやること。

**Files:** `src/output/xml.cpp:39`、`src/render/escape.cpp:5`

**現象**

両者はループ・`reserve`・`switch`・5 つの `case`・`default` まで完全に同一で、`case '\''` の `&apos;` と `&#39;` だけが違う（どちらも XML で整形式）。

**実害**

タスク 5 の制御文字除去を、重複を残したまま直すと 2 箇所でやることになる。次の修正でも同じことが起きる。

**変更**

共通ヘルパを 1 つ置き、アポストロフィのエンティティを引数にするか、XML でも整形式な `&#39;` に統一する。置き場所は実装者判断だが、`render` が `output` に依存するのは層が逆になるので `util` か新しい小さなヘッダが妥当。

**テスト**

既存の `xml_escape` / `html_escape` のテストがそのまま通ること。エンティティを統一する場合は、`'` を含む出力がゴールデンにあるか確認し、あれば再生成する。

---

## 10. 到達不能な分岐を消す

**Files:** `src/content/build.cpp:95-110`

**現象**

97 行目で `permalinks.contains()` を見て `continue` した後、105 行目まで `permalinks` を変更するものが無い（間にあるのは `engine.render_listing` だけ）。したがって 105 行目の `claim_permalink` は常に成功し、106 行目の `continue` は決して実行されない。

加えてこのループだけが上の document ループ（83 行目）や下の tag ループ（113 行目）と逆順（claim が render の後）になっており、読み手には意味のある差に見える。

**変更**

97 行目の `contains` チェックを消し、render の前に `claim_permalink` 1 回に揃える。3 つのループの形が同じになる。

**テスト**

既存のページネーションと permalink 衝突のテストが通ること。新規テストは不要。

---

## 11. claim ヘルパを共通化する

**Files:** `src/content/build.cpp:26-39`、`src/output/write.cpp:101-113`

**現象**

`claim_permalink` と `claim_output` は同じ `std::map<std::string, path>`、同じ `ErrorCode::Path`、同形のメッセージで、名詞（`permalink` / `出力先`）しか違わない。

**変更**

名詞を引数に取る汎用ヘルパ 1 つにまとめる。

**やらないこと**

`claim_output` への一本化はしない。`claim_permalink` は render の**前**に走って重複時のレンダリングを節約しており、`permalinks` マップはタスク 10 の後も一覧ループで使われる。呼び出し 2 箇所は残す。

**テスト**

既存の衝突テストがメッセージ込みで通ること。

---

## 12. `write_xml` を claim → render の順にする

**Files:** `src/content/build.cpp:132-135`

**現象**

`render_sitemap` / `render_feed` が `write_xml` の引数として先に評価されるので、`claim_output` が拒否した場合にドキュメント全体が無駄になる。

**実害はほぼない。** 発火条件は「出力先がリテラルに `sitemap.xml` / `feed.xml` になる document がある」という極めて稀なケースのみ。しかも `docs/spec/output.md` は「`url` が空のとき `static/sitemap.xml` を置くのはよい」としており、`url` が空なら `publish_feeds` はそもそも早期 return する。

**変更**

`write_xml` が本文を遅延生成できるようにする（ラムダを渡すか、claim と write を 2 段階に分ける）。他を直すついでで構わない。

**テスト**

不要。

---

## ADR の承認待ち（3 件）

`docs/spec/output.md` の書き換えを伴うため、ADR が Accepted になってから着手する。

| ADR | 内容 | 主な変更先 |
|---|---|---|
| [0007](../../adr/0007-out-dir-deletion-policy.md) | `--out` にマーカーを置き、無い非空ディレクトリは消さない。`--force` を追加 | `src/output/write.cpp`、`src/main.cpp`、`docs/spec/output.md`、`docs/spec/cli.md`、`tests/golden/blog-ja/expected/.kappan-out` |
| [0008](../../adr/0008-sitemap-lastmod-format.md) | `<lastmod>` を W3C Datetime にする（`util::format_w3c_datetime` 新設） | `src/util/datetime.*`、`src/output/xml.cpp`、`docs/spec/output.md` |
| [0009](../../adr/0009-feed-item-source.md) | feed の `item` を書き出しに成功したページに限定する | `src/output/xml.*`、`src/content/build.cpp`、`docs/spec/output.md` |

ADR-0007 とタスク 2 は同じ `prepare_out_dir` を触るが、依存はしない。タスク 2 の存在確認は「`--out` が空の状態から始まる」ことだけを前提にしており、ADR-0007 はその前提を変えない。
