#include "content/scan.hpp"

#include "util/path.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string>

namespace kappan::content {
namespace {

[[nodiscard]] Error io_error(const std::filesystem::path &where, std::string message) {
  return make_error(ErrorCode::Io, std::move(message), where);
}

[[nodiscard]] Error scan_error(const std::filesystem::path &content_dir,
                               const std::error_code &ec) {
  return io_error(content_dir, std::format("{}: 走査できません: {}",
                                           util::to_generic_utf8(content_dir), ec.message()));
}

} // namespace

Result<ScanResult> scan_markdown(const std::filesystem::path &content_dir,
                                 std::filesystem::directory_options options) {
  // 種別の問い合わせは 1 回だけ。exists と is_directory で error_code を使い回すと、
  // 問い合わせの失敗を「content ディレクトリがありません」と誤って報告しうる。
  std::error_code ec;
  const auto dir_status = std::filesystem::status(content_dir, ec);
  if (!std::filesystem::status_known(dir_status)) {
    return tl::unexpected(scan_error(content_dir, ec));
  }
  if (!std::filesystem::exists(dir_status) || !std::filesystem::is_directory(dir_status)) {
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format("{}: content ディレクトリがありません", util::to_generic_utf8(content_dir)),
        content_dir));
  }

  ScanResult result;
  auto it = std::filesystem::recursive_directory_iterator(content_dir, options, ec);
  if (ec) {
    return tl::unexpected(scan_error(content_dir, ec));
  }
  // libc++ では increment(ec) が失敗するとイテレータが end と等しくなる。
  // for の条件で先に抜けてしまい ec を見る機会が無くなるので、increment の直後に見る。
  const std::filesystem::recursive_directory_iterator end;
  while (it != end) {
    // 種別の問い合わせは 1 回だけ。投げるオーバーロード（is_directory() など）を使うと、
    // シンボリックリンクの解決失敗（ELOOP、リンク先の親に実行権が無い等）で
    // filesystem_error がここを貫通し、ビルド全体が落ちる。
    // 行き先の無いリンクは not_found という「判明した種別」なので、従来どおり黙って飛ばす。
    std::error_code type_ec;
    const auto entry_status = it->status(type_ec);
    if (!std::filesystem::status_known(entry_status)) {
      const auto detail = type_ec ? type_ec.message() : std::string{"種別が不明です"};
      result.errors.push_back(
          io_error(it->path(), std::format("{}: 種別を判定できません: {}",
                                           util::to_generic_utf8(it->path()), detail)));
    } else {
      const auto name = util::to_utf8(it->path().filename());
      if (std::filesystem::is_directory(entry_status) && name.starts_with('_')) {
        it.disable_recursion_pending();
      } else if (std::filesystem::is_regular_file(entry_status) &&
                 it->path().extension() == ".md") {
        result.files.push_back(it->path());
      }
    }
    it.increment(ec);
    if (ec) {
      result.errors.push_back(scan_error(content_dir, ec));
      break;
    }
  }

  std::ranges::sort(result.files);
  return result;
}

} // namespace kappan::content
