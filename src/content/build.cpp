#include "content/build.hpp"

#include "content/parse.hpp"
#include "content/scan.hpp"
#include "render/engine.hpp"
#include "site/paginate.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/config.hpp>
#include <kappan/site.hpp>

#include <format>
#include <map>
#include <utility>
#include <vector>

namespace kappan::content {
namespace {

[[nodiscard]] bool claim_permalink(std::map<std::string, std::filesystem::path> &permalinks,
                                   const std::string &permalink,
                                   const std::filesystem::path &source, BuildResult &result) {
  if (auto it = permalinks.find(permalink); it != permalinks.end()) {
    result.errors.push_back(make_error(ErrorCode::Path,
                                       std::format("{}: permalink '{}' が {} と衝突しています",
                                                   util::to_generic_utf8(source), permalink,
                                                   util::to_generic_utf8(it->second)),
                                       source));
    return false;
  }
  permalinks.emplace(permalink, source);
  return true;
}

void write_page(BuildResult &result, const std::filesystem::path &out_dir,
                const render::RenderedPage &page) {
  auto written = util::write_utf8_file(out_dir / page.output_path, page.html);
  if (!written) {
    result.errors.push_back(written.error());
    return;
  }
  ++result.pages_written;
}

} // namespace

BuildResult build_site(const std::filesystem::path &source, const std::filesystem::path &out_dir,
                       DraftPolicy drafts) {
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

  std::vector<Document> parsed;
  for (const auto &file : *files) {
    auto document = parse_document(file, *config);
    if (!document) {
      result.errors.push_back(document.error());
      continue;
    }
    parsed.push_back(std::move(*document));
  }

  auto built = site::build(std::move(*config), std::move(parsed), drafts);
  auto engine = render::Engine::load(built.config);
  if (!engine) {
    result.errors.push_back(engine.error());
    return result;
  }

  std::map<std::string, std::filesystem::path> permalinks;
  for (const auto &document : built.documents) {
    if (!claim_permalink(permalinks, document.permalink, document.source, result)) {
      continue;
    }
    auto page = engine->render(built, document);
    if (!page) {
      result.errors.push_back(page.error());
      continue;
    }
    write_page(result, out_dir, *page);
  }

  const auto listing = site::paginate(built.posts.indices, built.config.posts_per_page);
  for (const auto &page : listing) {
    if (permalinks.contains(page.permalink)) {
      continue;
    }
    auto rendered = engine->render_listing(built, page.page);
    if (!rendered) {
      result.errors.push_back(rendered.error());
      continue;
    }
    if (!claim_permalink(permalinks, page.permalink, built.config.source_root, result)) {
      continue;
    }
    write_page(result, out_dir, *rendered);
  }

  for (const auto &term : built.tags.terms) {
    if (!claim_permalink(permalinks, term.permalink, built.config.source_root, result)) {
      continue;
    }
    auto rendered = engine->render_tag(built, term.slug);
    if (!rendered) {
      result.errors.push_back(rendered.error());
      continue;
    }
    write_page(result, out_dir, *rendered);
  }
  return result;
}

} // namespace kappan::content
