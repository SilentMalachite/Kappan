#include "output/xml.hpp"

#include "util/datetime.hpp"
#include "util/escape.hpp"
#include "util/url.hpp"

#include <algorithm>
#include <ranges>

namespace kappan::output {
namespace {

void append_item(std::string &out, std::string_view base_url, const Document &document) {
  const auto abs = util::join_url(base_url, document.permalink);
  const auto &description = document.front_matter.description.empty()
                                ? document.body_html
                                : document.front_matter.description;
  out += "    <item>\n";
  out += "      <title>";
  out += xml_escape(document.front_matter.title);
  out += "</title>\n";
  out += "      <link>";
  out += xml_escape(abs);
  out += "</link>\n";
  out += "      <guid isPermaLink=\"true\">";
  out += xml_escape(abs);
  out += "</guid>\n";
  if (document.front_matter.date) {
    out += "      <pubDate>";
    out += util::format_rfc822(*document.front_matter.date);
    out += "</pubDate>\n";
  }
  out += "      <description>";
  out += xml_escape(description);
  out += "</description>\n";
  out += "    </item>\n";
}

} // namespace

std::string xml_escape(std::string_view text) { return util::escape_markup(text, "&apos;"); }

std::string render_sitemap(std::string_view base_url, std::vector<SitemapUrl> urls) {
  std::ranges::sort(urls, {}, &SitemapUrl::permalink);

  std::string out;
  out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out += "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
  for (const auto &url : urls) {
    out += "  <url>\n";
    out += "    <loc>";
    out += xml_escape(util::join_url(base_url, url.permalink));
    out += "</loc>\n";
    if (url.lastmod) {
      out += "    <lastmod>";
      out += util::format_w3c_datetime(*url.lastmod);
      out += "</lastmod>\n";
    }
    out += "  </url>\n";
  }
  out += "</urlset>\n";
  return out;
}

std::string render_feed(const Site &site, const std::set<std::string> &written_permalinks) {
  const auto &config = site.config;
  const auto &description = config.description.empty() ? config.title : config.description;

  std::string out;
  out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out += "<rss version=\"2.0\">\n";
  out += "  <channel>\n";
  out += "    <title>";
  out += xml_escape(config.title);
  out += "</title>\n";
  out += "    <link>";
  out += xml_escape(util::join_url(config.url, "/"));
  out += "</link>\n";
  out += "    <description>";
  out += xml_escape(description);
  out += "</description>\n";
  out += "    <language>";
  out += xml_escape(config.language);
  out += "</language>\n";
  for (const auto index : site.posts.indices) {
    const auto &document = site.documents[index];
    // 書き出せなかった記事は sitemap にも入らない。feed だけが 404 を配らないようにする。
    if (!written_permalinks.contains(document.permalink)) {
      continue;
    }
    append_item(out, config.url, document);
  }
  out += "  </channel>\n";
  out += "</rss>\n";
  return out;
}

} // namespace kappan::output
