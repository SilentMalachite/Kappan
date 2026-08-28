#include "output/assets.hpp"

#include "util/path.hpp"

#include <format>

namespace kappan::output {
namespace {

[[nodiscard]] Error io_error(const std::filesystem::path &where, std::string message) {
  return make_error(ErrorCode::Io, std::move(message), where);
}

[[nodiscard]] Error scan_error(const std::filesystem::path &static_dir, const std::error_code &ec) {
  return io_error(static_dir, std::format("{}: 走査できません: {}",
                                          util::to_generic_utf8(static_dir), ec.message()));
}

void copy_one(const std::filesystem::path &file, const std::filesystem::path &static_dir,
              const std::filesystem::path &out_dir, ClaimedOutputs &claimed,
              std::vector<Error> &errors) {
  std::error_code ec;
  const auto rel = std::filesystem::relative(file, static_dir, ec);
  if (ec || rel.empty() || rel == ".") {
    const auto detail = ec ? ec.message() : std::string{"空です"};
    errors.push_back(io_error(file, std::format("{}: 相対パスを求められません: {}",
                                                util::to_generic_utf8(file), detail)));
    return;
  }
  if (!claim_output(claimed, util::to_generic_utf8(rel), file, errors)) {
    return;
  }

  const auto dest = out_dir / rel;
  std::filesystem::create_directories(dest.parent_path(), ec);
  if (ec) {
    errors.push_back(io_error(dest, std::format("{}: 出力先を作成できません: {}",
                                                util::to_generic_utf8(dest), ec.message())));
    return;
  }

  // copy_file も既定では上書きしないが、返る Io の "File exists" では原因が伝わらない。
  if (!claim_destination(dest, file, errors)) {
    return;
  }

  std::filesystem::copy_file(file, dest, ec);
  if (ec) {
    errors.push_back(io_error(
        file, std::format("{}: コピーできません: {}", util::to_generic_utf8(file), ec.message())));
  }
}

} // namespace

std::vector<Error> copy_static(const std::filesystem::path &static_dir,
                               const std::filesystem::path &out_dir, ClaimedOutputs &claimed,
                               std::filesystem::directory_options options) {
  // 種別の問い合わせは 1 回だけ。exists と is_directory で error_code を使い回すと、
  // 問い合わせの失敗を「ディレクトリである必要があります」と誤って報告しうる。
  std::error_code ec;
  const auto dir_status = std::filesystem::status(static_dir, ec);
  if (!std::filesystem::status_known(dir_status)) {
    return {scan_error(static_dir, ec)};
  }
  if (!std::filesystem::exists(dir_status)) {
    return {};
  }
  if (!std::filesystem::is_directory(dir_status)) {
    return {io_error(static_dir, std::format("{}: static はディレクトリである必要があります",
                                             util::to_generic_utf8(static_dir)))};
  }

  std::vector<Error> errors;
  auto it = std::filesystem::recursive_directory_iterator(static_dir, options, ec);
  if (ec) {
    return {scan_error(static_dir, ec)};
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
      errors.push_back(
          io_error(it->path(), std::format("{}: 種別を判定できません: {}",
                                           util::to_generic_utf8(it->path()), detail)));
    } else {
      const auto name = util::to_utf8(it->path().filename());
      const bool is_dir = std::filesystem::is_directory(entry_status);
      // '_' 始まりのディレクトリは content/ と同じ規則で走査しない。
      // '.' 始まりはファイル・ディレクトリとも出力に混ぜない。macOS の Finder が書く
      // .DS_Store がゴールデンを壊すため（利用者は .gitignore 済みで気付けない）。
      if (name.starts_with('.') || (is_dir && name.starts_with('_'))) {
        if (is_dir) {
          it.disable_recursion_pending();
        }
      } else if (std::filesystem::is_regular_file(entry_status)) {
        copy_one(it->path(), static_dir, out_dir, claimed, errors);
      }
    }
    it.increment(ec);
    if (ec) {
      errors.push_back(scan_error(static_dir, ec));
      break;
    }
  }
  return errors;
}

} // namespace kappan::output
