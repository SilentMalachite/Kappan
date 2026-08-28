#include "output/write.hpp"

#include "util/path.hpp"

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

[[nodiscard]] Result<void> wipe_out_dir(const std::filesystem::path &out,
                                        const std::filesystem::path &out_dir) {
  std::error_code ec;
  if (std::filesystem::exists(out, ec) && std::filesystem::is_regular_file(out, ec)) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("{}: 出力先がファイルです", util::to_generic_utf8(out_dir)),
        out_dir));
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
  return {};
}

} // namespace

Result<void> prepare_out_dir(const std::filesystem::path &source,
                             const std::filesystem::path &out_dir) {
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

  const auto rel = src->lexically_relative(*out);
  if (rel.empty() || rel == ".") {
    return tl::unexpected(same_out_error(out_dir));
  }
  if (!rel.has_root_path() && !contains_dotdot(rel)) {
    return tl::unexpected(make_error(
        ErrorCode::Cli,
        std::format("{}: --out がソースディレクトリを消す位置です", util::to_generic_utf8(out_dir)),
        out_dir));
  }

  return wipe_out_dir(*out, out_dir);
}

bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                  const std::filesystem::path &source, std::vector<Error> &errors) {
  const std::string key{relative};
  if (const auto it = claimed.find(key); it != claimed.end()) {
    errors.push_back(make_error(ErrorCode::Path,
                                std::format("{}: 出力先 '{}' が {} と衝突しています",
                                            util::to_generic_utf8(source), relative,
                                            util::to_generic_utf8(it->second)),
                                source));
    return false;
  }
  claimed.emplace(key, source);
  return true;
}

} // namespace kappan::output
