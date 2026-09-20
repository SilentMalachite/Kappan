#include "content/scan.hpp"
#include "fs_probe.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

void write_markdown(const std::filesystem::path &path, std::string_view title) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  REQUIRE(out.is_open());
  out << "---\ntitle: " << title << "\n---\n\n本文\n";
  REQUIRE(out.good());
}

[[nodiscard]] bool contains_filename(const std::vector<std::filesystem::path> &files,
                                     std::string_view name) {
  return std::ranges::any_of(files, [&](const std::filesystem::path &file) {
    return kappan::util::to_utf8(file.filename()) == name;
  });
}

} // namespace

TEST_CASE("scan_markdown reports a missing content directory") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-missing";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);

  const auto scanned = kappan::content::scan_markdown(root / "content");
  REQUIRE_FALSE(scanned);
  REQUIRE(scanned.error().code == kappan::ErrorCode::Config);

  std::filesystem::remove_all(root);
}

TEST_CASE("scan_markdown collects Japanese file names and skips underscore directories") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-basic";
  std::filesystem::remove_all(root);
  const auto content = root / "content";
  write_markdown(content / kappan::util::from_utf8("こんにちは.md"), "こんにちは");
  write_markdown(content / "posts" / kappan::util::from_utf8("記事 🐙.md"), "記事");
  write_markdown(content / "_drafts" / "hidden.md", "hidden");
  {
    std::ofstream other(content / "note.txt", std::ios::binary);
    other << "md ではない\n";
  }

  const auto scanned = kappan::content::scan_markdown(content);
  REQUIRE(scanned);
  REQUIRE(scanned->errors.empty());
  REQUIRE(scanned->files.size() == 2);
  REQUIRE(contains_filename(scanned->files, "こんにちは.md"));
  REQUIRE(contains_filename(scanned->files, "記事 🐙.md"));
  REQUIRE_FALSE(contains_filename(scanned->files, "hidden.md"));
  // 走査結果は安定した順序で返る
  REQUIRE(std::ranges::is_sorted(scanned->files));

  std::filesystem::remove_all(root);
}

TEST_CASE("scan_markdown reports an unresolvable symlink without throwing") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-symlink";
  std::filesystem::remove_all(root);
  const auto content = root / "content";
  write_markdown(content / "ok.md", "ok");
  // 自分を指すループ。stat が ELOOP で失敗し、種別を判定できない。
  // 行き先なし。stat は not_found を返すだけなのでエラーではない（黙って飛ばす）。
  if (!kappan::testing::try_create_symlink("loop.md", content / "loop.md") ||
      !kappan::testing::try_create_symlink("nowhere.md", content / "dangling.md")) {
    std::filesystem::remove_all(root);
    SUCCEED("シンボリックリンクを作れない環境ではスキップする");
    return;
  }
  if (!kappan::testing::status_unresolvable(content / "loop.md")) {
    std::filesystem::remove_all(root);
    SUCCEED("循環リンクを解決できてしまう環境ではスキップする");
    return;
  }

  kappan::Result<kappan::content::ScanResult> scanned =
      tl::unexpected(kappan::make_error(kappan::ErrorCode::Io, "未実行"));
  REQUIRE_NOTHROW(scanned = kappan::content::scan_markdown(content));

  REQUIRE(scanned);
  REQUIRE(scanned->errors.size() == 1);
  REQUIRE(scanned->errors.front().code == kappan::ErrorCode::Io);
  REQUIRE(scanned->errors.front().message.find("種別を判定できません") != std::string::npos);
  REQUIRE(scanned->errors.front().where);
  // 1 件の失敗で走査全体を止めない（AGENTS.md §6）
  REQUIRE(scanned->files.size() == 1);
  REQUIRE(contains_filename(scanned->files, "ok.md"));

  std::filesystem::remove_all(root);
}

TEST_CASE("scan_markdown reports a content directory that cannot be resolved") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-loop-dir";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root);
  // content/ 自体がループ。「ありません」ではなく走査の失敗として報告する。
  if (!kappan::testing::try_create_symlink("content", root / "content")) {
    std::filesystem::remove_all(root);
    SUCCEED("シンボリックリンクを作れない環境ではスキップする");
    return;
  }
  if (!kappan::testing::status_unresolvable(root / "content")) {
    std::filesystem::remove_all(root);
    SUCCEED("循環リンクを解決できてしまう環境ではスキップする");
    return;
  }

  kappan::Result<kappan::content::ScanResult> scanned =
      tl::unexpected(kappan::make_error(kappan::ErrorCode::Io, "未実行"));
  REQUIRE_NOTHROW(scanned = kappan::content::scan_markdown(root / "content"));

  REQUIRE_FALSE(scanned);
  REQUIRE(scanned.error().code == kappan::ErrorCode::Io);
  REQUIRE(scanned.error().message.find("走査できません") != std::string::npos);

  std::filesystem::remove_all(root);
}

// POSIX のパーミッションビットは NTFS の ACL に写らない。Windows では
// permissions(perms::none) を掛けても走査は成功し、前提が原理的に成立しない。
#ifndef _WIN32
TEST_CASE("scan_markdown reports a directory it cannot open") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-locked-root";
  std::filesystem::remove_all(root);
  const auto content = root / "content";
  write_markdown(content / "ok.md", "ok");
  std::filesystem::permissions(content, std::filesystem::perms::none);

  // root は権限を無視するので、このテストの前提（構築が失敗すること）が成立しない。
  std::error_code probe_ec;
  const std::filesystem::directory_iterator probe(content, probe_ec);
  if (!probe_ec) {
    std::filesystem::permissions(content, std::filesystem::perms::owner_all);
    std::filesystem::remove_all(root);
    SUCCEED("権限が効かない環境ではスキップする");
    return;
  }

  // イテレータの構築に失敗すると end と等しくなる。ループ本体で ec を見ていると
  // 「content が空」と読み違えるので、構築の直後に見ていることを押さえる。
  // 既定の skip_permission_denied は構築失敗を握りつぶすため、none を渡す。
  const auto scanned =
      kappan::content::scan_markdown(content, std::filesystem::directory_options::none);

  std::filesystem::permissions(content, std::filesystem::perms::owner_all);

  REQUIRE_FALSE(scanned);
  REQUIRE(scanned.error().code == kappan::ErrorCode::Io);
  REQUIRE(scanned.error().message.find("走査できません") != std::string::npos);

  std::filesystem::remove_all(root);
}

TEST_CASE("scan_markdown reports a scan error instead of stopping silently") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-scan-locked-subdir";
  std::filesystem::remove_all(root);
  const auto content = root / "content";
  write_markdown(content / "ok.md", "ok");
  write_markdown(content / "locked" / "inner.md", "inner");
  std::filesystem::permissions(content / "locked", std::filesystem::perms::none);

  std::error_code probe_ec;
  const std::filesystem::directory_iterator probe(content / "locked", probe_ec);
  if (!probe_ec) {
    std::filesystem::permissions(content / "locked", std::filesystem::perms::owner_all);
    std::filesystem::remove_all(root);
    SUCCEED("権限が効かない環境ではスキップする");
    return;
  }

  // skip_permission_denied を外すと increment が失敗する。for の条件で先に抜けると
  // ec を見る機会が無くなるので、increment の直後に見ていることを押さえる。
  const auto scanned =
      kappan::content::scan_markdown(content, std::filesystem::directory_options::none);

  std::filesystem::permissions(content / "locked", std::filesystem::perms::owner_all);

  REQUIRE(scanned);
  REQUIRE(scanned->errors.size() == 1);
  REQUIRE(scanned->errors.front().code == kappan::ErrorCode::Io);
  REQUIRE(scanned->errors.front().message.find("走査できません") != std::string::npos);
  // 走査順は readdir 依存なので、打ち切りまでに何を拾えたかは主張しない。
  // ここで押さえたいのは「例外ではなく Error として報告される」こと。

  std::filesystem::remove_all(root);
}
#endif
