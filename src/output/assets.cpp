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

  std::filesystem::copy_file(file, dest, ec);
  if (ec) {
    errors.push_back(io_error(
        file, std::format("{}: コピーできません: {}", util::to_generic_utf8(file), ec.message())));
  }
}

} // namespace

std::vector<Error> copy_static(const std::filesystem::path &static_dir,
                               const std::filesystem::path &out_dir, ClaimedOutputs &claimed) {
  std::error_code ec;
  const bool exists = std::filesystem::exists(static_dir, ec);
  if (ec) {
    return {scan_error(static_dir, ec)};
  }
  if (!exists) {
    return {};
  }
  if (!std::filesystem::is_directory(static_dir, ec)) {
    return {io_error(static_dir, std::format("{}: static はディレクトリである必要があります",
                                             util::to_generic_utf8(static_dir)))};
  }

  std::vector<Error> errors;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  auto it = std::filesystem::recursive_directory_iterator(static_dir, options, ec);
  if (ec) {
    return {scan_error(static_dir, ec)};
  }
  for (; it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      errors.push_back(scan_error(static_dir, ec));
      break;
    }
    const auto name = util::to_utf8(it->path().filename());
    if (it->is_directory() && name.starts_with('_')) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file()) {
      copy_one(it->path(), static_dir, out_dir, claimed, errors);
    }
  }
  return errors;
}

} // namespace kappan::output
