#pragma once

#include <chrono>
#include <optional>
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

} // namespace kappan::output
