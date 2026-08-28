# Phase 5 出力（static / sitemap / RSS）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `kappan build` の出力をそのまま公開できるようにする。`<source>/static/` のコピー、`sitemap.xml`、RSS 2.0 の `feed.xml` を足す。

**Architecture:** パイプラインの最終段を `src/output/` に置く。`content::build_site` はオーケストレーションのまま、設定・走査・解析のあと `output::prepare_out_dir` で `--out` を空にし、HTML を書き、`url` があれば XML を書き、最後に `static/` をコピーする。後段は `Site` / `RenderedPage` を書き換えない。公開ヘッダは変えない。

**Tech Stack:** C++20、Catch2 v3、`std::filesystem`、既存の `util::write_utf8_file`。依存ライブラリは追加しない。

**Spec:** [docs/spec/output.md](../../spec/output.md)（判断は [docs/adr/0006-output-assets-feeds.md](../../adr/0006-output-assets-feeds.md)）

## Global Constraints

- C++20。modules / coroutines は使わない。
- `vcpkg.json` に無い依存を追加しない。
- 想定内エラーは `kappan::Result<T>` / `std::vector<Error>`。例外は `main.cpp` 以外で投げない。`std::filesystem` は `std::error_code` オーバーロードを使う。
- パスは `std::filesystem::path`。文字列連結で組み立てない。日本語パスは `util::from_utf8` / `to_utf8` / `to_generic_utf8`。
- `std::format` を使う。`printf` / iostream の書式は使わない。
- 識別子は英語、メッセージ・コメント・コミットは日本語。メンバは末尾 `_`。`[[nodiscard]]`。出力引数を増やさない（衝突マップだけは呼び出し側が所有し参照で渡す）。
- 公開ヘッダ `include/kappan/` は変えない。`Config::url` と `source_root` で足りる。
- 1 関数 50 行・1 ファイル 400 行を目安。超えたら分割する。
- 整形は `cmake --build --preset dev --target format`。警告は `#pragma` で黙らせない。
- テストなしの機能追加は完了と見なさない。フィクスチャに日本語・絵文字を含める。
- コミットは 1 目的。メッセージ形式は `AGENTS.md` の `<type>: <日本語 50 字以内>`。

## ファイル構成

| ファイル | 責務 |
|---|---|
| `src/output/xml.hpp` / `xml.cpp` | `xml_escape`、`join_url`、`render_sitemap`、`render_feed` |
| `src/output/write.hpp` / `write.cpp` | `prepare_out_dir`、`claim_output` |
| `src/output/assets.hpp` / `assets.cpp` | `copy_static` |
| `src/util/datetime.hpp` / `datetime.cpp` | `format_rfc822`（RSS `pubDate`） |
| `src/content/build.cpp` | 上記を呼ぶ。公開 API `build_site` は維持 |
| `tests/unit/test_output.cpp` | xml / prepare / assets の単体 |
| `tests/unit/test_datetime.cpp` | RFC 822 を追加 |
| `tests/unit/test_build_site.cpp` | パイプライン結合 |
| `examples/blog/static/` | ゴールデン用の日本語ファイル名アセット |
| `tests/golden/blog-ja/expected/` | `sitemap.xml` / `feed.xml` / コピーされたアセット |
| `CMakeLists.txt` / `tests/CMakeLists.txt` | ソース追加 |

書き出し順（衝突時は上書きしない）: HTML → `sitemap.xml` / `feed.xml` → `static/`。生成 XML を static が消さない。

`ClaimedOutputs` のキーは出力相対パスの generic UTF-8（`about/index.html`、`sitemap.xml`、`images/🐙.svg`）。

---

### Task 1: xml_escape と join_url

**Files:**
- Create: `src/output/xml.hpp`
- Create: `src/output/xml.cpp`
- Create: `tests/unit/test_output.cpp`
- Modify: `CMakeLists.txt`（`kappan_lib` に `src/output/xml.cpp` を追加）
- Modify: `tests/CMakeLists.txt`（`unit/test_output.cpp` を追加）

**Interfaces:**
- Consumes: なし
- Produces:

```cpp
namespace kappan::output {
[[nodiscard]] std::string xml_escape(std::string_view text);
[[nodiscard]] std::string join_url(std::string_view base_url, std::string_view permalink);
}
```

`xml_escape` は `&` `<` `>` `"` `'` を `&amp;` `&lt;` `&gt;` `&quot;` `&apos;` にする。`&` は switch で 1 文字ずつ（二重エスケープしない）。

`join_url` は `base_url` 末尾の `/` をすべて除き、`permalink` を連結する。`permalink` は `/` で始まる。`join_url("https://example.com/", "/")` は `https://example.com/`。

- [ ] **Step 1: 失敗するテストを書く**

`tests/unit/test_output.cpp`:

```cpp
#include "output/xml.hpp"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("xml_escape converts XML special characters") {
  REQUIRE(kappan::output::xml_escape("A&B <c> \"'\"") == "A&amp;B &lt;c&gt; &quot;&apos;&quot;");
  REQUIRE(kappan::output::xml_escape("日本語 🐙") == "日本語 🐙");
}

TEST_CASE("join_url strips trailing slashes and keeps Japanese permalinks") {
  REQUIRE(kappan::output::join_url("https://example.com/", "/") == "https://example.com/");
  REQUIRE(kappan::output::join_url("https://example.com", "/posts/こんにちは/") ==
          "https://example.com/posts/こんにちは/");
  REQUIRE(kappan::output::join_url("https://example.com/blog/", "/about/") ==
          "https://example.com/blog/about/");
}
```

CMake: `kappan_lib` のソース一覧に `src/output/xml.cpp` を `src/render/engine.cpp` の次へ。`tests/CMakeLists.txt` の実行ファイルに `unit/test_output.cpp` を `unit/test_site.cpp` の次へ。

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "xml_escape converts XML special characters"
```

Expected: コンパイル失敗（`output/xml.hpp` が無い）またはリンク失敗。スタブを先に置くなら `REQUIRE` が空文字で失敗する。

- [ ] **Step 3: 最小実装**

`src/output/xml.hpp`:

```cpp
#pragma once

#include <string>
#include <string_view>

namespace kappan::output {

[[nodiscard]] std::string xml_escape(std::string_view text);

[[nodiscard]] std::string join_url(std::string_view base_url, std::string_view permalink);

} // namespace kappan::output
```

`src/output/xml.cpp`:

```cpp
#include "output/xml.hpp"

namespace kappan::output {

std::string xml_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    switch (static_cast<unsigned char>(ch)) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&apos;";
      break;
    default:
      out.push_back(ch);
      break;
    }
  }
  return out;
}

std::string join_url(std::string_view base_url, std::string_view permalink) {
  std::string base{base_url};
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (permalink.empty()) {
    return base + "/";
  }
  return base + std::string{permalink};
}

} // namespace kappan::output
```

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "xml_escape converts XML special characters"
./build/dev/tests/kappan_tests "join_url strips trailing slashes and keeps Japanese permalinks"
```

Expected: PASS。警告ゼロ。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/output/xml.hpp src/output/xml.cpp tests/unit/test_output.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: XML エスケープと絶対 URL 結合を追加する

sitemap と RSS が日本語 URL を XML に載せるため。
EOF
)"
```

---

### Task 2: RFC 822 日付

**Files:**
- Modify: `src/util/datetime.hpp`
- Modify: `src/util/datetime.cpp`
- Modify: `tests/unit/test_datetime.cpp`

**Interfaces:**
- Consumes: 既存の `std::chrono::sys_seconds`
- Produces:

```cpp
[[nodiscard]] std::string format_rfc822(std::chrono::sys_seconds tp);
```

曜日・月は英語固定（`Sun`…`Sat`、`Jan`…`Dec`）。UTC、オフセットは常に `+0000`。`std::put_time` とロケールは使わない。`weekday::c_encoding()` は日曜 = 0。

- [ ] **Step 1: 失敗するテストを書く**

`tests/unit/test_datetime.cpp` に追加:

```cpp
TEST_CASE("format_rfc822 prints English UTC without locale") {
  const auto midnight = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  REQUIRE(kappan::util::format_rfc822(midnight) == "Thu, 01 Jan 2026 00:00:00 +0000");
  const auto afternoon = midnight + std::chrono::hours{15} + std::chrono::minutes{4} +
                         std::chrono::seconds{5};
  REQUIRE(kappan::util::format_rfc822(afternoon) == "Thu, 01 Jan 2026 15:04:05 +0000");
}
```

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "format_rfc822 prints English UTC without locale"
```

Expected: コンパイル失敗（`format_rfc822` が無い）。

- [ ] **Step 3: 最小実装**

`datetime.hpp` の `format_display_date` の次に宣言を足す。

`datetime.cpp` に実装を足す:

```cpp
std::string format_rfc822(std::chrono::sys_seconds tp) {
  constexpr const char *kDays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  constexpr const char *kMonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                     "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const auto day_point = std::chrono::floor<std::chrono::days>(tp);
  const std::chrono::year_month_day ymd{day_point};
  const std::chrono::weekday wd{day_point};
  const auto since = tp - std::chrono::sys_seconds{day_point};
  const auto hours = std::chrono::duration_cast<std::chrono::hours>(since);
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(since - hours);
  const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(since - hours - minutes);
  return std::format("{}, {:02} {} {:04} {:02}:{:02}:{:02} +0000", kDays[wd.c_encoding()],
                     static_cast<unsigned>(ymd.day()),
                     kMonths[static_cast<unsigned>(ymd.month()) - 1], static_cast<int>(ymd.year()),
                     hours.count(), minutes.count(), seconds.count());
}
```

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "format_rfc822 prints English UTC without locale"
```

Expected: PASS。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/util/datetime.hpp src/util/datetime.cpp tests/unit/test_datetime.cpp
git commit -m "$(cat <<'EOF'
feat: RSS 用の RFC 822 日付整形を追加する

pubDate をロケール非依存の英語曜日で出すため。
EOF
)"
```

---

### Task 3: 出力先の掃除と衝突検出

**Files:**
- Create: `src/output/write.hpp`
- Create: `src/output/write.cpp`
- Modify: `CMakeLists.txt`（`src/output/write.cpp` を追加）
- Modify: `tests/unit/test_output.cpp`

**Interfaces:**
- Consumes: `kappan::Error` / `make_error`、`util::to_generic_utf8`
- Produces:

```cpp
namespace kappan::output {
using ClaimedOutputs = std::map<std::string, std::filesystem::path>;

[[nodiscard]] Result<void> prepare_out_dir(const std::filesystem::path &source,
                                           const std::filesystem::path &out_dir);

[[nodiscard]] bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                                const std::filesystem::path &source, std::vector<Error> &errors);
}
```

`prepare_out_dir` は安全確認のあと `remove_all` + `create_directories`。`--out` がソースと同じ、またはソースが `--out` の内側なら `ErrorCode::Cli` を返し、**消さない**。比較は `absolute` → `weakly_canonical`（どちらも `error_code`）。内側判定: `source.lexically_relative(out)` が空でも `.` でもなく、ルートも `..` も含まない。`--out` がソースの内側は許可。`--out` が既存ファイルなら `ErrorCode::Io`。

`claim_output` のキーは `relative` そのもの。衝突時は `ErrorCode::Path`、メッセージ:

`{source}: 出力先 '{relative}' が {other} と衝突しています`

`Cli` メッセージ:

- `{out}: --out がソースディレクトリと同じです`
- `{out}: --out がソースディレクトリを消す位置です`

- [ ] **Step 1: 失敗するテストを書く**

`tests/unit/test_output.cpp` に追加（必要なヘッダ: `output/write.hpp`、`kappan/error.hpp`、`<filesystem>`、`<fstream>`）:

```cpp
TEST_CASE("prepare_out_dir rejects out equal to source without deleting it") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-out-same";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream keep(dir / "keep.txt", std::ios::binary);
    keep << "残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(dir, dir);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Cli);
  REQUIRE(result.error().message.find("同じ") != std::string::npos);
  REQUIRE(std::filesystem::exists(dir / "keep.txt"));
  std::filesystem::remove_all(dir);
}

TEST_CASE("prepare_out_dir rejects source inside out without deleting source") {
  const auto out = std::filesystem::temp_directory_path() / "kappan-out-parent";
  const auto source = out / "site";
  std::filesystem::remove_all(out);
  std::filesystem::create_directories(source);
  {
    std::ofstream keep(source / "site.yaml", std::ios::binary);
    keep << "title: 残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(source, out);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Cli);
  REQUIRE(result.error().message.find("消す") != std::string::npos);
  REQUIRE(std::filesystem::exists(source / "site.yaml"));
  std::filesystem::remove_all(out);
}

TEST_CASE("prepare_out_dir wipes previous output when out is inside source") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-out-child";
  const auto out = source / "out";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(out);
  {
    std::ofstream stale(out / "stale.html", std::ios::binary);
    stale << "古い\n";
  }
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(source, out);
  REQUIRE(result);
  REQUIRE(std::filesystem::is_directory(out));
  REQUIRE_FALSE(std::filesystem::exists(out / "stale.html"));
  REQUIRE(std::filesystem::exists(source / "site.yaml"));
  std::filesystem::remove_all(source);
}

TEST_CASE("claim_output reports a Japanese path collision") {
  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> errors;
  const auto first = std::filesystem::path{"content"} / "a.md";
  const auto second = std::filesystem::path{"static"} / kappan::util::from_utf8("画像") /
                      "index.html";
  REQUIRE(kappan::output::claim_output(claimed, "about/index.html", first, errors));
  REQUIRE_FALSE(kappan::output::claim_output(claimed, "about/index.html", second, errors));
  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE(errors.front().message.find("about/index.html") != std::string::npos);
}
```

`claim_output` のテストは `util/path.hpp` が要る。

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "prepare_out_dir rejects out equal to source without deleting it"
```

Expected: コンパイル失敗（`output/write.hpp` が無い）。

- [ ] **Step 3: 最小実装**

`write.hpp`:

```cpp
#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace kappan::output {

using ClaimedOutputs = std::map<std::string, std::filesystem::path>;

[[nodiscard]] Result<void> prepare_out_dir(const std::filesystem::path &source,
                                           const std::filesystem::path &out_dir);

[[nodiscard]] bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                                const std::filesystem::path &source, std::vector<Error> &errors);

} // namespace kappan::output
```

`write.cpp` の要点:

```cpp
[[nodiscard]] bool contains_dotdot(const std::filesystem::path &rel) {
  for (const auto &part : rel) {
    if (part == "..") {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Result<std::filesystem::path> canonical_abs(const std::filesystem::path &path) {
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: パスを解決できません: {}", util::to_generic_utf8(path), ec.message()),
        path));
  }
  const auto canonical = std::filesystem::weakly_canonical(absolute, ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: パスを解決できません: {}", util::to_generic_utf8(path), ec.message()),
        path));
  }
  return canonical;
}
```

`prepare_out_dir`: `canonical_abs(source)` と `canonical_abs(out_dir)` を取る。等しければ Cli「同じ」。`auto rel = src.lexically_relative(out)` が `rel.empty() || rel == "."` なら同じ扱いで既に返している。`rel.has_root_path()` または `contains_dotdot(rel)` なら内側ではない。それ以外はソースが out の内側 → Cli「消す」。そのあと `exists && is_regular_file` なら Io。`remove_all` と `create_directories` の失敗も Io。メッセージにパスと `ec.message()` を入れる。

`claim_output`: `claimed.find(std::string{relative})` があればエラーを push して `false`。無ければ emplace して `true`。

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev
./build/dev/tests/kappan_tests "prepare_out_dir rejects out equal to source without deleting it"
./build/dev/tests/kappan_tests "prepare_out_dir rejects source inside out without deleting source"
./build/dev/tests/kappan_tests "prepare_out_dir wipes previous output when out is inside source"
./build/dev/tests/kappan_tests "claim_output reports a Japanese path collision"
```

Expected: PASS。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/output/write.hpp src/output/write.cpp tests/unit/test_output.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: 出力先の掃除と衝突検出を追加する

--out がソースを消さないことと、静的ファイルの上書き防止のため。
EOF
)"
```

---

### Task 4: static/ のコピー

**Files:**
- Create: `src/output/assets.hpp`
- Create: `src/output/assets.cpp`
- Modify: `CMakeLists.txt`（`src/output/assets.cpp` を追加）
- Modify: `tests/unit/test_output.cpp`

**Interfaces:**
- Consumes: `claim_output`、`ClaimedOutputs`、`util::to_utf8` / `to_generic_utf8` / `from_utf8`
- Produces:

```cpp
[[nodiscard]] std::vector<Error> copy_static(const std::filesystem::path &static_dir,
                                             const std::filesystem::path &out_dir,
                                             ClaimedOutputs &claimed);
```

仕様: ディレクトリが無ければ空のエラー配列。存在するがディレクトリでなければ `ErrorCode::Io`（`{path}: static はディレクトリである必要があります`）。`_` 始まりのディレクトリは `disable_recursion_pending`。通常ファイルだけコピー。相対パスを保つ。バイト列のまま（`std::filesystem::copy_file`。overwrite しない。UTF-8 検査なし）。走査失敗は `ErrorCode::Io` を 1 件返して打ち切ってよい。個別ファイルのコピー失敗は集約して続行。衝突は `claim_output` が false ならスキップ。

- [ ] **Step 1: 失敗するテストを書く**

```cpp
TEST_CASE("copy_static copies Japanese filenames and raw bytes") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-copy";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir / "images");
  std::filesystem::create_directories(static_dir / kappan::util::from_utf8("_隠し"));
  std::filesystem::create_directories(out);
  {
    std::ofstream file(static_dir / "images" / kappan::util::from_utf8("🐙.svg"), std::ios::binary);
    file << "<svg xmlns='http://www.w3.org/2000/svg'/>";
  }
  {
    std::ofstream bin(static_dir / "blob.bin", std::ios::binary);
    const char bytes[] = {'\0', '\x01', '\xFF', '\xFE'};
    bin.write(bytes, 4);
  }
  {
    std::ofstream hidden(static_dir / kappan::util::from_utf8("_隠し") / "nope.css",
                         std::ios::binary);
    hidden << "body{}\n";
  }
  kappan::output::ClaimedOutputs claimed;
  const auto errors = kappan::output::copy_static(static_dir, out, claimed);
  REQUIRE(errors.empty());
  const auto copied = out / "images" / kappan::util::from_utf8("🐙.svg");
  REQUIRE(std::filesystem::exists(copied));
  std::ifstream in(copied, std::ios::binary);
  const std::string got{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  REQUIRE(got.find("svg") != std::string::npos);
  std::ifstream bin_in(out / "blob.bin", std::ios::binary);
  const std::string raw{std::istreambuf_iterator<char>(bin_in), std::istreambuf_iterator<char>()};
  REQUIRE(raw.size() == 4);
  REQUIRE(static_cast<unsigned char>(raw[2]) == 0xFF);
  REQUIRE_FALSE(std::filesystem::exists(out / kappan::util::from_utf8("_隠し") / "nope.css"));
  std::filesystem::remove_all(root);
}

TEST_CASE("copy_static ignores a missing directory") {
  kappan::output::ClaimedOutputs claimed;
  const auto errors = kappan::output::copy_static(
      std::filesystem::temp_directory_path() / "kappan-no-static-dir",
      std::filesystem::temp_directory_path(), claimed);
  REQUIRE(errors.empty());
}

TEST_CASE("copy_static skips files that collide with claimed outputs") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-collide";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir);
  std::filesystem::create_directories(out);
  {
    std::ofstream file(static_dir / "index.html", std::ios::binary);
    file << "static\n";
  }
  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> claim_errors;
  REQUIRE(kappan::output::claim_output(claimed, "index.html", root / "content" / "index.md",
                                       claim_errors));
  const auto errors = kappan::output::copy_static(static_dir, out, claimed);
  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE_FALSE(std::filesystem::exists(out / "index.html"));
  std::filesystem::remove_all(root);
}
```

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "copy_static copies Japanese filenames and raw bytes"
```

Expected: コンパイル失敗（`copy_static` が無い）。

- [ ] **Step 3: 最小実装**

走査は `src/content/scan.cpp` と同じ `recursive_directory_iterator` + `directory_options::skip_permission_denied`。拡張子フィルタはしない。コピー前に `dest` の親を `create_directories`。`copy_file` はデフォルト（上書きしない）。

相対パス: `std::filesystem::relative(file, static_dir, ec)`。キーは `util::to_generic_utf8(rel)`。出力は `out_dir / rel`。

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev
./build/dev/tests/kappan_tests "copy_static copies Japanese filenames and raw bytes"
./build/dev/tests/kappan_tests "copy_static ignores a missing directory"
./build/dev/tests/kappan_tests "copy_static skips files that collide with claimed outputs"
```

Expected: PASS。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/output/assets.hpp src/output/assets.cpp tests/unit/test_output.cpp CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: static ディレクトリを出力根へコピーする

公開に必要な CSS と日本語ファイル名の画像を変換せず出すため。
EOF
)"
```

---

### Task 5: sitemap.xml の生成

**Files:**
- Modify: `src/output/xml.hpp`
- Modify: `src/output/xml.cpp`
- Modify: `tests/unit/test_output.cpp`

**Interfaces:**
- Consumes: `xml_escape`、`join_url`、`util::format_iso_datetime`
- Produces:

```cpp
struct SitemapUrl {
  std::string permalink;
  std::optional<std::chrono::sys_seconds> lastmod;
};

[[nodiscard]] std::string render_sitemap(std::string_view base_url, std::vector<SitemapUrl> urls);
```

並びは `permalink` 昇順。`lastmod` が無ければ要素ごと省略。BOM なし。末尾改行あり。XML 宣言と `urlset` の xmlns は spec の例どおり。`format_iso_datetime` をそのまま使う（日付だけなら `YYYY-MM-DD`）。

- [ ] **Step 1: 失敗するテストを書く**

```cpp
TEST_CASE("render_sitemap sorts permalinks and keeps Japanese loc") {
  using kappan::output::SitemapUrl;
  const auto day = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  const auto xml = kappan::output::render_sitemap(
      "https://example.com/", {{"/posts/こんにちは/", day}, {"/", std::nullopt}});
  REQUIRE_FALSE(xml.starts_with("\xEF\xBB\xBF"));
  REQUIRE(xml.find("xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\"") != std::string::npos);
  const auto home = xml.find("<loc>https://example.com/</loc>");
  const auto post = xml.find("<loc>https://example.com/posts/こんにちは/</loc>");
  REQUIRE(home != std::string::npos);
  REQUIRE(post != std::string::npos);
  REQUIRE(home < post);
  REQUIRE(xml.find("<lastmod>2026-01-01</lastmod>") != std::string::npos);
  const auto home_url_end = xml.find("</url>", home);
  REQUIRE(xml.find("<lastmod>", home) > home_url_end);
}
```

`xml.hpp` に `<chrono>` `<optional>` `<vector>` を足す。

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "render_sitemap sorts permalinks and keeps Japanese loc"
```

Expected: コンパイル失敗。

- [ ] **Step 3: 最小実装**

`std::ranges::sort(urls, {}, &SitemapUrl::permalink)` のあと、次の形で連結する:

```
<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">
  <url>
    <loc>...</loc>
    <lastmod>...</lastmod>
  </url>
</urlset>\n
```

`loc` は `xml_escape(join_url(base_url, permalink))`。

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "render_sitemap sorts permalinks and keeps Japanese loc"
```

Expected: PASS。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/output/xml.hpp src/output/xml.cpp tests/unit/test_output.cpp
git commit -m "$(cat <<'EOF'
feat: sitemap.xml を生成する

公開 URL を検索エンジンへ渡すため。
EOF
)"
```

---

### Task 6: RSS 2.0 の feed.xml

**Files:**
- Modify: `src/output/xml.hpp`
- Modify: `src/output/xml.cpp`
- Modify: `tests/unit/test_output.cpp`

**Interfaces:**
- Consumes: `Site`、`join_url`、`xml_escape`、`util::format_rfc822`
- Produces:

```cpp
[[nodiscard]] std::string render_feed(const Site &site);
```

`item` は `site.posts.indices` の順（date 降順は `site::build` 済み）。ページ・タグは入れない。`channel/link` は `join_url(url, "/")`。`channel/description` が空なら `title`。`item/description` は front matter `description`、空なら `body_html`。どちらも XML エスケープ（CDATA なし）。`date` が無ければ `pubDate` を省略。`guid isPermaLink="true"`。

- [ ] **Step 1: 失敗するテストを書く**

```cpp
TEST_CASE("render_feed lists posts only with RFC 822 dates") {
  kappan::Config config;
  config.title = "活版ブログ";
  config.url = "https://example.com";
  config.language = "ja";
  config.description = "日本語と絵文字 🐙 を含むサイト";

  kappan::Document post;
  post.permalink = "/posts/こんにちは/";
  post.front_matter.title = "こんにちは";
  post.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  post.body_html = "<p>最初の記事です。</p>\n";

  kappan::Document page;
  page.permalink = "/about/";
  page.front_matter.title = "概要";
  page.body_html = "<p>概要</p>\n";

  kappan::Document draft;
  draft.permalink = "/posts/下書き/";
  draft.front_matter.title = "下書き";
  draft.front_matter.draft = true;
  draft.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 2};

  auto site = kappan::site::build(config, {post, page, draft}, kappan::DraftPolicy::Exclude);
  const auto xml = kappan::output::render_feed(site);
  REQUIRE(xml.find("<rss version=\"2.0\">") != std::string::npos);
  REQUIRE(xml.find("<link>https://example.com/</link>") != std::string::npos);
  REQUIRE(xml.find("日本語と絵文字 🐙 を含むサイト") != std::string::npos);
  REQUIRE(xml.find("<title>こんにちは</title>") != std::string::npos);
  REQUIRE(xml.find("<guid isPermaLink=\"true\">https://example.com/posts/こんにちは/</guid>") !=
          std::string::npos);
  REQUIRE(xml.find("Thu, 01 Jan 2026 00:00:00 +0000") != std::string::npos);
  REQUIRE(xml.find("&lt;p&gt;最初の記事です。&lt;/p&gt;") != std::string::npos);
  REQUIRE(xml.find("概要") == std::string::npos);
  REQUIRE(xml.find("下書き") == std::string::npos);
}

TEST_CASE("render_feed uses title when site description is empty") {
  kappan::Config config;
  config.title = "タイトルだけ";
  config.url = "https://example.com";
  auto site = kappan::site::build(config, {}, kappan::DraftPolicy::Exclude);
  const auto xml = kappan::output::render_feed(site);
  REQUIRE(xml.find("<description>タイトルだけ</description>") != std::string::npos);
  REQUIRE(xml.find("<item>") == std::string::npos);
}
```

`#include <kappan/site.hpp>` が要る。

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "render_feed lists posts only with RFC 822 dates"
```

Expected: コンパイル失敗。

- [ ] **Step 3: 最小実装**

`xml.hpp` に `#include <kappan/site.hpp>` と `render_feed` 宣言。

`render_feed` は channel を書いてから `for (const auto index : site.posts.indices)`。関数が 50 行を超えそうなら `append_item(std::string &out, const Document &)` を無名名前空間に抜く。

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev
./build/dev/tests/kappan_tests "render_feed lists posts only with RFC 822 dates"
./build/dev/tests/kappan_tests "render_feed uses title when site description is empty"
```

Expected: PASS。下書きは `site::build` で落ちているので XML に出ない。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/output/xml.hpp src/output/xml.cpp tests/unit/test_output.cpp
git commit -m "$(cat <<'EOF'
feat: RSS 2.0 の feed.xml を生成する

記事の更新をフィードリーダーへ渡すため。
EOF
)"
```

---

### Task 7: build_site から出力段を呼ぶ

**Files:**
- Modify: `src/content/build.cpp`
- Modify: `tests/unit/test_build_site.cpp`

**Interfaces:**
- Consumes: `output::prepare_out_dir`、`output::claim_output`、`output::copy_static`、`output::render_sitemap`、`output::render_feed`、既存の `claim_permalink` / `write_page`
- Produces: 既存の `content::build_site` の挙動に、掃除・static・XML を足す。`pages_written` は HTML だけ。

処理順:

1. いまどおり source 検証・config・scan・parse・`site::build`・`Engine::load`。ここまでの失敗では `--out` を消さない。
2. `output::prepare_out_dir(source, out_dir)`。失敗したらそのエラーを入れて return。
3. HTML ループ。permalink 衝突は既存どおり。成功したページは `claim_output(claimed, to_generic_utf8(page.output_path), source, result.errors)` もする。sitemap 用に `SitemapUrl{permalink, lastmod}` を積む。Document だけ `lastmod = date`。一覧・タグは `nullopt`。既存の `permalinks.contains` でスキップしたホームは二重に積まない。
4. `config.url` が空でなければ `sitemap.xml` と `feed.xml` を `claim_output` してから `util::write_utf8_file`。失敗は集約。`pages_written` は増やさない。
5. `copy_static(config.source_root / "static", out_dir, claimed)` のエラーを `result.errors` へ追加。

`build_site` が 50 行を超えるなら、無名名前空間に `publish_html` と `publish_feeds` を抜く。ファイルが 400 行を超えるならそれ以上足さない。

既存の permalink 衝突テストはメッセージに `permalink` が残るので `claim_permalink` は消さない。

- [ ] **Step 1: 失敗するテストを書く**

`tests/unit/test_build_site.cpp` に追加:

```cpp
TEST_CASE("build_site does not wipe out on config error") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-keep-out";
  const auto out = std::filesystem::temp_directory_path() / "kappan-keep-out-dest";
  std::filesystem::remove_all(source);
  std::filesystem::remove_all(out);
  std::filesystem::create_directories(source);
  std::filesystem::create_directories(out);
  {
    std::ofstream keep(out / "keep.txt", std::ios::binary);
    keep << "残す\n";
  }
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Config);
  REQUIRE(std::filesystem::exists(out / "keep.txt"));
  std::filesystem::remove_all(source);
  std::filesystem::remove_all(out);
}

TEST_CASE("build_site copies static files and writes sitemap and feed") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-phase5-site";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content" / "posts");
  std::filesystem::create_directories(source / "static" / "images");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 活版\nurl: https://example.com\nlanguage: ja\ndescription: 説明 🐙\n";
  }
  {
    std::ofstream index(source / "content" / "index.md", std::ios::binary);
    index << "---\ntitle: ホーム\n---\n# ホーム\n";
  }
  {
    std::ofstream post(source / "content" / "posts" /
                           kappan::util::from_utf8("2026-01-01-こんにちは.md"),
                       std::ios::binary);
    post << "---\ntitle: こんにちは\ndate: 2026-01-01\n---\n本文です。\n";
  }
  {
    std::ofstream draft(source / "content" / "posts" /
                            kappan::util::from_utf8("2026-01-02-下書き.md"),
                        std::ios::binary);
    draft << "---\ntitle: 下書き\ndate: 2026-01-02\ndraft: true\n---\n秘密\n";
  }
  {
    std::ofstream svg(source / "static" / "images" / kappan::util::from_utf8("🐙.svg"),
                      std::ios::binary);
    svg << "<svg xmlns='http://www.w3.org/2000/svg'/>";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(std::filesystem::exists(out / "images" / kappan::util::from_utf8("🐙.svg")));
  const auto sitemap = read_all(out / "sitemap.xml");
  REQUIRE(sitemap.find("https://example.com/posts/こんにちは/") != std::string::npos);
  REQUIRE(sitemap.find("下書き") == std::string::npos);
  const auto feed = read_all(out / "feed.xml");
  REQUIRE(feed.find("<rss version=\"2.0\">") != std::string::npos);
  REQUIRE(feed.find("こんにちは") != std::string::npos);
  REQUIRE(feed.find("下書き") == std::string::npos);
  REQUIRE_FALSE(sitemap.starts_with("\xEF\xBB\xBF"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site omits sitemap and feed when url is empty") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-no-url";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: URL無し\n";
  }
  {
    std::ofstream index(source / "content" / "index.md", std::ios::binary);
    index << "---\ntitle: ホーム\n---\n# ホーム\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE_FALSE(std::filesystem::exists(out / "sitemap.xml"));
  REQUIRE_FALSE(std::filesystem::exists(out / "feed.xml"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site reports static collision with generated HTML") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-static-html-collide";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content");
  std::filesystem::create_directories(source / "static" / "about");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 衝突\n";
  }
  {
    std::ofstream about(source / "content" / "about.md", std::ios::binary);
    about << "---\ntitle: 概要\n---\n本文\n";
  }
  {
    std::ofstream stale(source / "static" / "about" / "index.html", std::ios::binary);
    stale << "static\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Path);
  const auto html = read_all(out / "about" / "index.html");
  REQUIRE(html.find("本文") != std::string::npos);
  std::filesystem::remove_all(source);
}
```

- [ ] **Step 2: テストが失敗することを確認する**

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "build_site copies static files and writes sitemap and feed"
```

Expected: FAIL（`sitemap.xml` が無い）。`build_site omits sitemap` は url 空の現状どおり PASS し得る。先にコピー/XML のテストが FAIL することを確認する。

- [ ] **Step 3: 最小実装**

`src/content/build.cpp` に `#include "output/write.hpp"`、`#include "output/assets.hpp"`、`#include "output/xml.hpp"`。

`Engine::load` 成功の直後に `prepare_out_dir`。HTML 書き込みで `ClaimedOutputs claimed` と `std::vector<output::SitemapUrl> sitemap_urls` を持つ。

フィード書き込みのヘルパ例:

```cpp
void write_xml(BuildResult &result, output::ClaimedOutputs &claimed,
               const std::filesystem::path &source, const std::filesystem::path &out_dir,
               std::string_view name, std::string_view body) {
  if (!output::claim_output(claimed, name, source, result.errors)) {
    return;
  }
  auto written = util::write_utf8_file(out_dir / std::filesystem::path{name}, body);
  if (!written) {
    result.errors.push_back(written.error());
  }
}
```

`std::filesystem::path{name}` は ASCII の `sitemap.xml` / `feed.xml` だけ。

HTML の `write_page` のあと `sitemap_urls.push_back`。url が空でなければ:

```cpp
write_xml(result, claimed, built.config.source_root, out_dir, "sitemap.xml",
          output::render_sitemap(built.config.url, sitemap_urls));
write_xml(result, claimed, built.config.source_root, out_dir, "feed.xml",
          output::render_feed(built));
```

最後に:

```cpp
auto copied = output::copy_static(built.config.source_root / "static", out_dir, claimed);
result.errors.insert(result.errors.end(), copied.begin(), copied.end());
```

- [ ] **Step 4: テストが通ることを確認する**

```bash
cmake --build --preset dev && ctest --preset dev --output-on-failure
```

Expected: 既存テストを含め PASS。ゴールデンはまだ `sitemap.xml` が無いので **FAIL する**。この Task ではゴールデン以外を通す。ゴールデンを一時的に消したり無効化したりしない。

ゴールデン以外:

```bash
./build/dev/tests/kappan_tests "build_site"
./build/dev/tests/kappan_tests "create_site"
./build/dev/tests/kappan_tests "copy_static"
./build/dev/tests/kappan_tests "render_"
./build/dev/tests/kappan_tests "prepare_out_dir"
```

`golden blog-ja matches examples/blog` は Task 8 まで FAIL でよい。失敗理由は expected に無い `sitemap.xml` / `feed.xml` が actual にあること。それ以外の理由で落ちたら Task 7 で直す。

- [ ] **Step 5: コミット**

```bash
cmake --build --preset dev --target format
git add src/content/build.cpp tests/unit/test_build_site.cpp
git commit -m "$(cat <<'EOF'
feat: ビルドから static とフィード書き出しを呼ぶ

公開用ファイルを HTML と同じパイプラインで出すため。
EOF
)"
```

---

### Task 8: ゴールデンと examples/blog の static

**Files:**
- Create: `examples/blog/static/css/site.css`
- Create: `examples/blog/static/images/🐙.svg`
- Create: `tests/golden/blog-ja/expected/sitemap.xml`
- Create: `tests/golden/blog-ja/expected/feed.xml`
- Create: `tests/golden/blog-ja/expected/css/site.css`
- Create: `tests/golden/blog-ja/expected/images/🐙.svg`

**Interfaces:**
- Consumes: Task 7 の `build_site`
- Produces: ゴールデンが examples/blog の生成物と一致する

`examples/blog` は `url: https://example.com` があるので XML が出る。下書き `2026-01-04-下書き.md` は sitemap / feed に出ない。

sitemap の permalink 昇順（UTF-8 バイト順）:

1. `/`
2. `/about/`
3. `/page/2/`
4. `/posts/こんにちは/`
5. `/posts/新しい/`
6. `/posts/絵文字/`
7. `/tags/日本語/`
8. `/tags/絵文字/`

`lastmod` があるのは日付付き posts だけ。ホーム・about・page/2・tags は要素ごと無し。

feed の item 順（date 降順）: 新しい → 絵文字 → こんにちは。`pubDate` は `Sat, 03 Jan 2026` / `Fri, 02 Jan 2026` / `Thu, 01 Jan 2026`（いずれも `00:00:00 +0000`）。description は本文 HTML をエスケープしたもの。

- [ ] **Step 1: アセットを置き、失敗しているゴールデンを確認する**

`examples/blog/static/css/site.css`:

```css
/* 活版サンプル */
body { font-family: sans-serif; }
```

`examples/blog/static/images/🐙.svg`:

```svg
<svg xmlns="http://www.w3.org/2000/svg" width="16" height="16"><circle cx="8" cy="8" r="7" fill="#333"/></svg>
```

```bash
cmake --build --preset dev && ./build/dev/tests/kappan_tests "golden blog-ja matches examples/blog"
```

Expected: FAIL。actual に `sitemap.xml` / `feed.xml` / `css/site.css` / `images/🐙.svg` があり、expected に無い。

- [ ] **Step 2: 生成物を expected に取り込む**

```bash
rm -rf /tmp/kappan-phase5-golden
./build/dev/kappan build --source examples/blog --out /tmp/kappan-phase5-golden
```

生成した `sitemap.xml` と `feed.xml` を開き、次を目で確認してからコピーする。

- BOM が無い
- `<loc>` が `https://example.com/posts/こんにちは/` を含む
- `下書き` が無い
- feed が `<rss version="2.0">` で posts 3 件
- `css/site.css` と `images/🐙.svg` のバイトが examples と一致

```bash
cp /tmp/kappan-phase5-golden/sitemap.xml tests/golden/blog-ja/expected/sitemap.xml
cp /tmp/kappan-phase5-golden/feed.xml tests/golden/blog-ja/expected/feed.xml
mkdir -p tests/golden/blog-ja/expected/css tests/golden/blog-ja/expected/images
cp examples/blog/static/css/site.css tests/golden/blog-ja/expected/css/site.css
cp examples/blog/static/images/🐙.svg tests/golden/blog-ja/expected/images/🐙.svg
```

Windows パスを手で組まない。`cp` の日本語ファイル名はそのまま。

- [ ] **Step 3: ゴールデンを再実行する（まだ expected が古いと FAIL のまま）**

このステップは「expected を置いた」ことの確認。実装の追加はしない。

- [ ] **Step 4: 全テストが通ることを確認する**

```bash
cmake --build --preset dev --target format
cmake --build --preset dev
ctest --preset dev --output-on-failure
```

Expected: 全部 PASS。警告ゼロ。`create_site` は `url` が空なので XML 無しのまま `pages_written == 3`。

- [ ] **Step 5: コミット**

```bash
git add examples/blog/static tests/golden/blog-ja/expected
git commit -m "$(cat <<'EOF'
test: ゴールデンに sitemap と static を足す

examples/blog が公開可能な出力になることを固定するため。
EOF
)"
```

---

## Spec coverage（自己レビュー）

| spec 項目 | Task |
|---|---|
| `--out` 掃除は config/走査/解析のあと | 7（呼び出し位置）、3（実装）、7（config 失敗で消さないテスト） |
| `--out` == source を拒否し消さない | 3 |
| source が `--out` の内側を拒否 | 3 |
| `--out` が source の内側は可 | 3 |
| `static/` を相対パスのままコピー | 4, 7, 8 |
| `static/` 無しはエラーにしない | 4 |
| `_` ディレクトリを走査しない | 4 |
| バイナリを UTF-8 検査しない | 4 |
| 日本語ファイル名 | 4, 7, 8 |
| 同梱テーマから static を出さない | 実装しない（テーマ埋め込みは HTML のみのまま） |
| `url` 空なら sitemap/RSS なし | 7 |
| sitemap は全 HTML、下書きなし、permalink 昇順 | 5, 7, 8 |
| `<loc>` は UTF-8 連結、XML エスケープ | 1, 5 |
| `<lastmod>` は Document の date だけ | 5, 7, 8 |
| RSS 2.0 `feed.xml`、posts のみ | 6, 7, 8 |
| description 空なら title / 本文 HTML | 6 |
| pubDate RFC 822 英語 UTC | 2, 6 |
| 衝突は上書きせず Path | 3, 4, 7 |
| `pages_written` は HTML だけ | 7（既存カウントを XML で増やさない） |

プレースホルダ（TBD / 「適切に」）は無し。Task 1 の `xml.hpp` と Task 5/6 の追加は同じ型名 `kappan::output::xml_escape` / `join_url` / `SitemapUrl` / `render_sitemap` / `render_feed`。`ClaimedOutputs` は Task 3 で定義し 4 と 7 が使う。
