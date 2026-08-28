#include "output/xml.hpp"

#include "util/datetime.hpp"

#include <algorithm>
#include <ranges>

namespace kappan::output {

std::string xml_escape(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (const char ch : text) {
    switch (static_cast<unsigned char>(ch)) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    case '\'':
      out += "&apos;";
      break;
    default:
      out.push_back(ch);
      break;
    }
  }
  return out;
}

std::string join_url(std::string_view base_url, std::string_view permalink) {
  std::string base{base_url};
  while (!base.empty() && base.back() == '/') {
    base.pop_back();
  }
  if (permalink.empty()) {
    return base + "/";
  }
  return base + std::string{permalink};
}

std::string render_sitemap(std::string_view base_url, std::vector<SitemapUrl> urls) {
  std::ranges::sort(urls, {}, &SitemapUrl::permalink);

  std::string out;
  out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  out += "<urlset xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\">\n";
  for (const auto &url : urls) {
    out += "  <url>\n";
    out += "    <loc>";
    out += xml_escape(join_url(base_url, url.permalink));
    out += "</loc>\n";
    if (url.lastmod) {
      out += "    <lastmod>";
      out += util::format_iso_datetime(*url.lastmod);
      out += "</lastmod>\n";
    }
    out += "  </url>\n";
  }
  out += "</urlset>\n";
  return out;
}

} // namespace kappan::output
