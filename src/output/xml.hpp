#pragma once

#include <kappan/site.hpp>

#include <chrono>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace kappan::output {

struct SitemapUrl {
  std::string permalink;
  std::optional<std::chrono::sys_seconds> lastmod;
};

[[nodiscard]] std::string xml_escape(std::string_view text);

[[nodiscard]] std::string join_url(std::string_view base_url, std::string_view permalink);

[[nodiscard]] std::string render_sitemap(std::string_view base_url, std::vector<SitemapUrl> urls);

// written_permalinks は HTML の書き出しに成功したページの permalink。
// レンダリングや書き出しに失敗した記事を購読者に配らないため、これに無い item は落とす。
// sitemap と同じ集合を根拠にする（ADR-0009）。
[[nodiscard]] std::string render_feed(const Site &site,
                                      const std::set<std::string> &written_permalinks);

} // namespace kappan::output
