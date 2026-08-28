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
#include <set>
#include <string_view>
#include <utility>
#include <vector>

namespace kappan::content {
namespace {

[[nodiscard]] bool claim_permalink(output::ClaimedOutputs &permalinks, const std::string &permalink,
                                   const std::filesystem::path &source, BuildResult &result) {
  return output::claim_unique(permalinks, permalink, "permalink", source, result.errors);
}

// HTML と XML を実際に書くのはここだけ。static のコピーは copy_one が同じ関門を通る。
[[nodiscard]] bool write_output_file(BuildResult &result, const std::filesystem::path &dest,
                                     const std::filesystem::path &source,
                                     std::string_view content) {
  if (!output::claim_destination(dest, source, result.errors)) {
    return false;
  }
  auto written = util::write_utf8_file(dest, content);
  if (!written) {
    result.errors.push_back(written.error());
    return false;
  }
  return true;
}

[[nodiscard]] bool write_page(BuildResult &result, const std::filesystem::path &out_dir,
                              const std::filesystem::path &source,
                              const render::RenderedPage &page) {
  if (!write_output_file(result, out_dir / page.output_path, source, page.html)) {
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
  if (!write_page(result, out_dir, source, page)) {
    return;
  }
  sitemap_urls.push_back(std::move(url));
}

// body は遅延生成する。claim に失敗する出力先のために XML 全体を組み立てても捨てるだけ。
template <typename RenderBody>
void write_xml(BuildResult &result, output::ClaimedOutputs &claimed,
               const std::filesystem::path &source, const std::filesystem::path &out_dir,
               std::string_view name, RenderBody &&render_body) {
  if (!output::claim_output(claimed, name, source, result.errors)) {
    return;
  }
  static_cast<void>(
      write_output_file(result, out_dir / std::filesystem::path{name}, source, render_body()));
}

void publish_html(BuildResult &result, const Site &built, const render::Engine &engine,
                  const std::filesystem::path &out_dir, output::ClaimedOutputs &claimed,
                  std::vector<output::SitemapUrl> &sitemap_urls) {
  output::ClaimedOutputs permalinks;
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
    // 1 ページ目の permalink は "/" で、content/index.md と必ず重なる。
    // これは衝突ではなく「Document が一覧より優先される」という規則なので、
    // 上下のループと違ってエラーにせず黙って飛ばす。
    if (permalinks.contains(page.permalink)) {
      continue;
    }
    auto rendered = engine.render_listing(built, page.page);
    if (!rendered) {
      result.errors.push_back(rendered.error());
      continue;
    }
    // 直前の contains で不在は確定しているので、ここでの claim は必ず成功する。
    permalinks.emplace(page.permalink, built.config.source_root);
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
  // sitemap_urls を move する前に集合を作る。feed は sitemap と同じ集合を根拠にする。
  std::set<std::string> written;
  for (const auto &url : sitemap_urls) {
    written.insert(url.permalink);
  }
  write_xml(result, claimed, built.config.source_root, out_dir, "sitemap.xml",
            [&] { return output::render_sitemap(built.config.url, std::move(sitemap_urls)); });
  write_xml(result, claimed, built.config.source_root, out_dir, "feed.xml",
            [&] { return output::render_feed(built, written); });
}

} // namespace

BuildResult build_site(const std::filesystem::path &source, const std::filesystem::path &out_dir,
                       DraftPolicy drafts, output::OutDirPolicy out_policy) {
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

  auto prepared = output::prepare_out_dir(source, out_dir, out_policy);
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
