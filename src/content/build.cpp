#include "content/build.hpp"

#include "content/parse.hpp"
#include "content/scan.hpp"
#include "output/assets.hpp"
#include "output/write.hpp"
#include "output/xml.hpp"
#include "render/engine.hpp"
#include "site/paginate.hpp"
#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/config.hpp>
#include <kappan/site.hpp>

#include <format>
#include <map>
#include <optional>
#include <string_view>
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

[[nodiscard]] bool write_page(BuildResult &result, const std::filesystem::path &out_dir,
                              const render::RenderedPage &page) {
  auto written = util::write_utf8_file(out_dir / page.output_path, page.html);
  if (!written) {
    result.errors.push_back(written.error());
    return false;
  }
  ++result.pages_written;
  return true;
}

void record_page(BuildResult &result, output::ClaimedOutputs &claimed,
                 std::vector<output::SitemapUrl> &sitemap_urls,
                 const std::filesystem::path &out_dir, const std::filesystem::path &source,
                 const render::RenderedPage &page, output::SitemapUrl url) {
  if (!output::claim_output(claimed, util::to_generic_utf8(page.output_path), source,
                            result.errors)) {
    return;
  }
  if (!write_page(result, out_dir, page)) {
    return;
  }
  sitemap_urls.push_back(std::move(url));
}

void write_xml(BuildResult &result, output::ClaimedOutputs &claimed,
               const std::filesystem::path &source, const std::filesystem::path &out_dir,
               std::string_view name, std::string_view body) {
  if (!output::claim_output(claimed, name, source, result.errors)) {
    return;
  }
  auto written = util::write_utf8_file(out_dir / std::filesystem::path{name}, body);
  if (!written) {
    result.errors.push_back(written.error());
  }
}

void publish_html(BuildResult &result, const Site &built, const render::Engine &engine,
                  const std::filesystem::path &out_dir, output::ClaimedOutputs &claimed,
                  std::vector<output::SitemapUrl> &sitemap_urls) {
  std::map<std::string, std::filesystem::path> permalinks;
  for (const auto &document : built.documents) {
    if (!claim_permalink(permalinks, document.permalink, document.source, result)) {
      continue;
    }
    auto page = engine.render(built, document);
    if (!page) {
      result.errors.push_back(page.error());
      continue;
    }
    record_page(result, claimed, sitemap_urls, out_dir, document.source, *page,
                {document.permalink, document.front_matter.date});
  }

  const auto listing = site::paginate(built.posts.indices, built.config.posts_per_page);
  for (const auto &page : listing) {
    if (permalinks.contains(page.permalink)) {
      continue;
    }
    auto rendered = engine.render_listing(built, page.page);
    if (!rendered) {
      result.errors.push_back(rendered.error());
      continue;
    }
    if (!claim_permalink(permalinks, page.permalink, built.config.source_root, result)) {
      continue;
    }
    record_page(result, claimed, sitemap_urls, out_dir, built.config.source_root, *rendered,
                {page.permalink, std::nullopt});
  }

  for (const auto &term : built.tags.terms) {
    if (!claim_permalink(permalinks, term.permalink, built.config.source_root, result)) {
      continue;
    }
    auto rendered = engine.render_tag(built, term.slug);
    if (!rendered) {
      result.errors.push_back(rendered.error());
      continue;
    }
    record_page(result, claimed, sitemap_urls, out_dir, built.config.source_root, *rendered,
                {term.permalink, std::nullopt});
  }
}

void publish_feeds(BuildResult &result, output::ClaimedOutputs &claimed, const Site &built,
                   const std::filesystem::path &out_dir,
                   std::vector<output::SitemapUrl> sitemap_urls) {
  if (built.config.url.empty()) {
    return;
  }
  write_xml(result, claimed, built.config.source_root, out_dir, "sitemap.xml",
            output::render_sitemap(built.config.url, std::move(sitemap_urls)));
  write_xml(result, claimed, built.config.source_root, out_dir, "feed.xml",
            output::render_feed(built));
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

  auto prepared = output::prepare_out_dir(source, out_dir);
  if (!prepared) {
    result.errors.push_back(prepared.error());
    return result;
  }

  output::ClaimedOutputs claimed;
  std::vector<output::SitemapUrl> sitemap_urls;
  publish_html(result, built, *engine, out_dir, claimed, sitemap_urls);
  publish_feeds(result, claimed, built, out_dir, std::move(sitemap_urls));
  auto copied = output::copy_static(built.config.source_root / "static", out_dir, claimed);
  result.errors.insert(result.errors.end(), copied.begin(), copied.end());
  return result;
}

} // namespace kappan::content
