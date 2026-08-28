#include "content/build.hpp"

#include "content/parse.hpp"
#include "content/scan.hpp"
#include "render/engine.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/config.hpp>

#include <format>
#include <map>
#include <utility>

namespace kappan::content {

BuildResult build_site(const std::filesystem::path &source, const std::filesystem::path &out_dir) {
  BuildResult result;
  std::error_code ec;
  if (std::filesystem::is_regular_file(source, ec)) {
    result.errors.push_back(make_error(
        ErrorCode::Cli,
        std::format(
            "{}: --source はサイトの根ディレクトリを指定してください（site.yaml が必要です）",
            util::to_generic_utf8(source)),
        source));
    return result;
  }

  const auto site_yaml = source / "site.yaml";
  auto config = config::load(site_yaml);
  if (!config) {
    result.errors.push_back(config.error());
    return result;
  }

  auto files = scan_markdown(config->content_dir);
  if (!files) {
    result.errors.push_back(files.error());
    return result;
  }

  auto engine = render::Engine::load(*config);
  if (!engine) {
    result.errors.push_back(engine.error());
    return result;
  }

  std::map<std::string, std::filesystem::path> permalinks;
  for (const auto &file : *files) {
    auto document = parse_document(file, *config);
    if (!document) {
      result.errors.push_back(document.error());
      continue;
    }
    if (auto it = permalinks.find(document->permalink); it != permalinks.end()) {
      result.errors.push_back(make_error(
          ErrorCode::Path,
          std::format("{}: permalink '{}' が {} と衝突しています", util::to_generic_utf8(file),
                      document->permalink, util::to_generic_utf8(it->second)),
          file));
      continue;
    }
    permalinks.emplace(document->permalink, file);
    auto page = engine->render(*document);
    if (!page) {
      result.errors.push_back(page.error());
      continue;
    }
    const auto out_path = out_dir / page->output_path;
    auto written = util::write_utf8_file(out_path, page->html);
    if (!written) {
      result.errors.push_back(written.error());
      continue;
    }
    ++result.pages_written;
  }
  return result;
}

} // namespace kappan::content
