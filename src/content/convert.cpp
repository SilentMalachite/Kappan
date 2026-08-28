#include "content/convert.hpp"

#include "markdown/cmark.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <filesystem>
#include <format>

namespace kappan::content {

Result<void> convert_markdown_file(const std::filesystem::path &source,
                                   const std::filesystem::path &out_dir) {
  std::error_code ec;
  if (std::filesystem::is_directory(source, ec)) {
    return tl::unexpected(make_error(
        ErrorCode::Cli,
        std::format("{}: --source は Markdown ファイルを指定してください", util::to_utf8(source)),
        source));
  }

  auto text = util::read_utf8_file(source);
  if (!text) {
    return tl::unexpected(text.error());
  }

  auto html = markdown::to_html(*text, source);
  if (!html) {
    return tl::unexpected(html.error());
  }

  auto out_path = out_dir / source.stem();
  out_path.replace_extension(".html");
  return util::write_utf8_file(out_path, *html);
}

} // namespace kappan::content
