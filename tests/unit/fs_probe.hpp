#pragma once

#include <filesystem>
#include <system_error>

// プラットフォーム依存の前提を実行時に確かめるための小道具。
// #ifdef でテストを丸ごと落とすと 3 OS のうち 1 つで永久に未検証になるので、
// 前提が成立する環境では必ず走り、成立しない環境だけ SUCCEED で飛ばす（AGENTS.md §7）。
namespace kappan::testing {

// シンボリックリンクを作る。作れない環境（Windows で SeCreateSymbolicLinkPrivilege が
// 無い場合など）では false を返す。POSIX では通常成功する。
[[nodiscard]] inline bool try_create_symlink(const std::filesystem::path &target,
                                             const std::filesystem::path &link) {
  std::error_code ec;
  std::filesystem::create_symlink(target, link, ec);
  return !ec;
}

// そのパスの種別問い合わせが失敗することを確かめる。循環リンクを解決できてしまう
// 環境では false を返し、テスト側で前提不成立としてスキップする。
[[nodiscard]] inline bool status_unresolvable(const std::filesystem::path &path) {
  std::error_code ec;
  return !std::filesystem::status_known(std::filesystem::status(path, ec));
}

} // namespace kappan::testing
