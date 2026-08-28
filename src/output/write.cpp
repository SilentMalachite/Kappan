#include "output/write.hpp"

#include "util/path.hpp"
#include "util/utf8.hpp"

#include <format>

namespace kappan::output {
namespace {

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

[[nodiscard]] Error same_out_error(const std::filesystem::path &out_dir) {
  return make_error(
      ErrorCode::Cli,
      std::format("{}: --out がソースディレクトリと同じです", util::to_generic_utf8(out_dir)),
      out_dir);
}

// 消してよい --out か。空、または前回 kappan が書いた印がある場合だけ真。
// 前回の出力で常に非空になるので「非空なら拒否」だけでは毎回 --force が要り、機能しない。
[[nodiscard]] Result<bool> out_dir_is_reusable(const std::filesystem::path &out,
                                               const std::filesystem::path &out_dir) {
  std::error_code ec;
  if (std::filesystem::exists(out / kOutMarker, ec)) {
    return true;
  }
  const bool empty = std::filesystem::is_empty(out, ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: 出力先を確認できません: {}", util::to_generic_utf8(out_dir), ec.message()),
        out_dir));
  }
  return empty;
}

[[nodiscard]] Result<void> wipe_out_dir(const std::filesystem::path &out,
                                        const std::filesystem::path &out_dir, OutDirPolicy policy) {
  // 種別の問い合わせは 1 回だけ。exists と is_regular_file で error_code を使い回すと、
  // 後の呼び出しが前の ec を上書きし、問い合わせの失敗を「ファイルです」と誤って報告しうる。
  // 存在しないパスでは status が ec に ENOENT を入れるので、ec ではなく status_known で見る。
  std::error_code ec;
  const auto status = std::filesystem::status(out, ec);
  if (!std::filesystem::status_known(status)) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: 出力先を確認できません: {}", util::to_generic_utf8(out_dir), ec.message()),
        out_dir));
  }
  if (std::filesystem::is_regular_file(status)) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("{}: 出力先がファイルです", util::to_generic_utf8(out_dir)),
        out_dir));
  }

  if (std::filesystem::exists(status) && policy == OutDirPolicy::Refuse) {
    const auto reusable = out_dir_is_reusable(out, out_dir);
    if (!reusable) {
      return tl::unexpected(reusable.error());
    }
    if (!*reusable) {
      return tl::unexpected(make_error(
          ErrorCode::Cli,
          std::format(
              "{}: kappan の出力先ではないディレクトリが空ではありません。消してよければ --force "
              "を付けてください",
              util::to_generic_utf8(out_dir)),
          out_dir));
    }
  }

  std::filesystem::remove_all(out, ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: 出力先を削除できません: {}", util::to_generic_utf8(out_dir), ec.message()),
        out_dir));
  }
  std::filesystem::create_directories(out, ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io,
        std::format("{}: 出力先を作成できません: {}", util::to_generic_utf8(out_dir), ec.message()),
        out_dir));
  }
  // 印は作成直後に書く。途中で失敗しても次回の再ビルドが拒否されないようにするため。
  return util::write_utf8_file(out / std::filesystem::path{kOutMarker},
                               "kappan output directory\n");
}

} // namespace

Result<void> prepare_out_dir(const std::filesystem::path &source,
                             const std::filesystem::path &out_dir, OutDirPolicy policy) {
  const auto src = canonical_abs(source);
  if (!src) {
    return tl::unexpected(src.error());
  }
  const auto out = canonical_abs(out_dir);
  if (!out) {
    return tl::unexpected(out.error());
  }

  if (*src == *out) {
    return tl::unexpected(same_out_error(out_dir));
  }

  if (source_inside_out(src->lexically_relative(*out))) {
    return tl::unexpected(make_error(
        ErrorCode::Cli,
        std::format("{}: --out がソースディレクトリを消す位置です", util::to_generic_utf8(out_dir)),
        out_dir));
  }

  return wipe_out_dir(*out, out_dir, policy);
}

bool source_inside_out(const std::filesystem::path &relative_source) {
  // ルート名が違うと lexically_relative は空を返す（[fs.path.gen]）。無関係なので内側ではない。
  if (relative_source.empty()) {
    return false;
  }
  return !relative_source.has_root_path() && !contains_dotdot(relative_source);
}

bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                  const std::filesystem::path &source, std::vector<Error> &errors) {
  // 書き込み・コピーはすべてここを通る。--out の外を指す出力先は claim させない。
  const auto path = util::from_utf8(relative);
  if (path.has_root_path() || contains_dotdot(path)) {
    errors.push_back(make_error(ErrorCode::Path,
                                std::format("{}: 出力パス '{}' が --out の外を指しています",
                                            util::to_generic_utf8(source), relative),
                                source));
    return false;
  }

  return claim_unique(claimed, relative, "出力先", source, errors);
}

bool claim_unique(ClaimedOutputs &claimed, std::string_view key, std::string_view noun,
                  const std::filesystem::path &source, std::vector<Error> &errors) {
  const std::string owned{key};
  if (const auto it = claimed.find(owned); it != claimed.end()) {
    errors.push_back(
        make_error(ErrorCode::Path,
                   std::format("{}: {} '{}' が {} と衝突しています", util::to_generic_utf8(source),
                               noun, key, util::to_generic_utf8(it->second)),
                   source));
    return false;
  }
  claimed.emplace(owned, source);
  return true;
}

bool claim_destination(const std::filesystem::path &dest, const std::filesystem::path &source,
                       std::vector<Error> &errors) {
  // --out は書き出し前に空にしてある。宛先が既にあるなら、この同じビルドの
  // 別の出力が同じファイルに解決されたということ。
  std::error_code ec;
  if (std::filesystem::exists(dest, ec)) {
    errors.push_back(make_error(
        ErrorCode::Path,
        std::format("{}: 出力先 '{}' は別のページが既に書いています（大文字小文字か Unicode "
                    "正規化の違いで同じファイルに解決されています）",
                    util::to_generic_utf8(source), util::to_generic_utf8(dest)),
        source));
    return false;
  }
  if (ec) {
    errors.push_back(make_error(
        ErrorCode::Io,
        std::format("{}: 出力先を確認できません: {}", util::to_generic_utf8(dest), ec.message()),
        dest));
    return false;
  }
  return true;
}

} // namespace kappan::output
