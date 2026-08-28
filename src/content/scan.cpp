#include "content/scan.hpp"

#include "util/path.hpp"

#include <algorithm>
#include <format>
#include <ranges>

namespace kappan::content {

Result<std::vector<std::filesystem::path>> scan_markdown(const std::filesystem::path &content_dir) {
  std::error_code ec;
  if (!std::filesystem::is_directory(content_dir, ec)) {
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format("{}: content ディレクトリがありません", util::to_generic_utf8(content_dir)),
        content_dir));
  }

  std::vector<std::filesystem::path> files;
  const auto options = std::filesystem::directory_options::skip_permission_denied;
  for (auto it = std::filesystem::recursive_directory_iterator(content_dir, options, ec);
       it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      return tl::unexpected(make_error(
          ErrorCode::Io,
          std::format("{}: 走査できません: {}", util::to_generic_utf8(content_dir), ec.message()),
          content_dir));
    }
    const auto name = util::to_utf8(it->path().filename());
    if (it->is_directory() && name.starts_with('_')) {
      it.disable_recursion_pending();
      continue;
    }
    if (it->is_regular_file() && it->path().extension() == ".md") {
      files.push_back(it->path());
    }
  }

  std::ranges::sort(files);
  return files;
}

} // namespace kappan::content
