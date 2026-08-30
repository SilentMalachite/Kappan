#include <kappan/config.hpp>

#include "util/path.hpp"
#include "util/utf8.hpp"

#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <charconv>
#include <format>
#include <optional>
#include <string_view>

namespace kappan::config {
namespace {

[[nodiscard]] int file_line(const YAML::Mark &mark) {
  if (mark.is_null() || mark.line < 0) {
    return 1;
  }
  return mark.line + 1;
}

[[nodiscard]] Result<YAML::Node> load_root(const std::string &text,
                                           const std::filesystem::path &where) {
  try {
    return YAML::Load(text);
  } catch (const YAML::Exception &ex) {
    const int line = file_line(ex.mark);
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format("{}:{} YAML を解析できません: {}", util::to_generic_utf8(where), line, ex.msg),
        where, line));
  }
}

[[nodiscard]] Result<std::string> require_scalar(const YAML::Node &node, const char *key,
                                                 const std::filesystem::path &where) {
  if (!node || !node.IsDefined()) {
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format("{}:1 必須キー '{}' がありません", util::to_generic_utf8(where), key), where,
        1));
  }
  if (!node.IsScalar()) {
    const int line = file_line(node.Mark());
    return tl::unexpected(make_error(ErrorCode::Config,
                                     std::format("{}:{} キー '{}' は文字列である必要があります",
                                                 util::to_generic_utf8(where), line, key),
                                     where, line));
  }
  return node.Scalar();
}

[[nodiscard]] Result<std::string> optional_scalar(const YAML::Node &root, const char *key,
                                                  const std::filesystem::path &where,
                                                  std::string fallback) {
  const auto node = root[key];
  if (!node || !node.IsDefined()) {
    return fallback;
  }
  if (!node.IsScalar()) {
    const int line = file_line(node.Mark());
    return tl::unexpected(make_error(ErrorCode::Config,
                                     std::format("{}:{} キー '{}' は文字列である必要があります",
                                                 util::to_generic_utf8(where), line, key),
                                     where, line));
  }
  return node.Scalar();
}

[[nodiscard]] bool has_forbidden_url_byte(std::string_view value) {
  return std::ranges::any_of(value, [](const char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte <= 0x20U || byte == 0x7FU;
  });
}

[[nodiscard]] bool is_ascii_digit(const char value) { return value >= '0' && value <= '9'; }

[[nodiscard]] bool is_ascii_alpha(const char value) {
  return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

[[nodiscard]] bool is_ascii_alnum(const char value) {
  return is_ascii_alpha(value) || is_ascii_digit(value);
}

[[nodiscard]] bool is_ascii_hex_digit(const char value) {
  return is_ascii_digit(value) || (value >= 'A' && value <= 'F') || (value >= 'a' && value <= 'f');
}

[[nodiscard]] bool valid_port(std::string_view port) {
  if (port.empty()) {
    return false;
  }
  int value = 0;
  const auto result = std::from_chars(port.data(), port.data() + port.size(), value);
  return result.ec == std::errc{} && result.ptr == port.data() + port.size() && value >= 1 &&
         value <= 65535;
}

[[nodiscard]] bool valid_ipv4(std::string_view address) {
  int part_count = 0;
  std::size_t start = 0;
  while (start <= address.size()) {
    const auto separator = address.find('.', start);
    const auto part = address.substr(start, separator - start);
    if (part.empty() || part.size() > 3 ||
        !std::ranges::all_of(part, [](const char value) { return is_ascii_digit(value); })) {
      return false;
    }
    int value = 0;
    const auto result = std::from_chars(part.data(), part.data() + part.size(), value);
    if (result.ec != std::errc{} || result.ptr != part.data() + part.size() || value > 255) {
      return false;
    }
    ++part_count;
    if (separator == std::string_view::npos) {
      break;
    }
    start = separator + 1;
  }
  return part_count == 4;
}

[[nodiscard]] bool valid_dns_name(std::string_view host) {
  if (host.empty() || host.find(':') != std::string_view::npos) {
    return false;
  }
  if (host.ends_with('.')) {
    host.remove_suffix(1);
  }
  if (host.empty() || host.size() > 253) {
    return false;
  }
  if (std::ranges::count(host, '.') == 3 && std::ranges::all_of(host, [](const char value) {
        return is_ascii_digit(value) || value == '.';
      })) {
    return valid_ipv4(host);
  }

  std::size_t start = 0;
  while (start <= host.size()) {
    const auto separator = host.find('.', start);
    const auto label = host.substr(start, separator - start);
    if (label.empty() || label.size() > 63 || !is_ascii_alnum(label.front()) ||
        !is_ascii_alnum(label.back()) || !std::ranges::all_of(label, [](const char value) {
          return is_ascii_alnum(value) || value == '-';
        })) {
      return false;
    }
    if (separator == std::string_view::npos) {
      break;
    }
    start = separator + 1;
  }
  return true;
}

[[nodiscard]] bool valid_hextet(std::string_view hextet) {
  return !hextet.empty() && hextet.size() <= 4 &&
         std::ranges::all_of(hextet, [](const char value) { return is_ascii_hex_digit(value); });
}

enum class Ipv4TailPolicy { Forbid, Allow };

[[nodiscard]] std::optional<int> ipv6_units(std::string_view side,
                                            const Ipv4TailPolicy ipv4_tail_policy) {
  if (side.empty()) {
    return 0;
  }
  int units = 0;
  std::size_t start = 0;
  while (start <= side.size()) {
    const auto separator = side.find(':', start);
    const auto token = side.substr(start, separator - start);
    const bool is_last = separator == std::string_view::npos;
    if (token.find('.') != std::string_view::npos) {
      if (ipv4_tail_policy == Ipv4TailPolicy::Forbid || !is_last || !valid_ipv4(token)) {
        return std::nullopt;
      }
      units += 2;
    } else if (valid_hextet(token)) {
      ++units;
    } else {
      return std::nullopt;
    }
    if (is_last) {
      break;
    }
    start = separator + 1;
  }
  return units;
}

[[nodiscard]] bool valid_ipv6_literal(std::string_view literal) {
  if (literal.empty() || literal.find('%') != std::string_view::npos) {
    return false;
  }
  const auto compression = literal.find("::");
  if (compression == std::string_view::npos) {
    const auto units = ipv6_units(literal, Ipv4TailPolicy::Allow);
    return units && *units == 8;
  }
  if (literal.find("::", compression + 2) != std::string_view::npos) {
    return false;
  }
  const auto left_units = ipv6_units(literal.substr(0, compression), Ipv4TailPolicy::Forbid);
  const auto right_units = ipv6_units(literal.substr(compression + 2), Ipv4TailPolicy::Allow);
  return left_units && right_units && *left_units + *right_units < 8;
}

[[nodiscard]] bool valid_authority(std::string_view authority) {
  if (authority.empty() || authority.find('@') != std::string_view::npos) {
    return false;
  }
  if (authority.starts_with('[')) {
    const auto close = authority.find(']');
    if (close == std::string_view::npos || !valid_ipv6_literal(authority.substr(1, close - 1))) {
      return false;
    }
    const auto suffix = authority.substr(close + 1);
    return suffix.empty() || (suffix.starts_with(':') && valid_port(suffix.substr(1)));
  }

  const auto port_separator = authority.rfind(':');
  auto host = authority;
  if (port_separator != std::string_view::npos) {
    host = authority.substr(0, port_separator);
    if (host.find(':') != std::string_view::npos ||
        !valid_port(authority.substr(port_separator + 1))) {
      return false;
    }
  }
  return valid_dns_name(host);
}

// site.url は sitemap / feed / OGP の絶対 URL の基点になる。相対値やスキーム無しを
// 通すと、壊れた URL のまま出力してしまうため、ここで境界を閉じる。
[[nodiscard]] bool is_absolute_http_url(std::string_view url) {
  if (has_forbidden_url_byte(url)) {
    return false;
  }
  constexpr std::string_view http{"http://"};
  constexpr std::string_view https{"https://"};
  std::string_view remainder;
  if (url.starts_with(https)) {
    remainder = url.substr(https.size());
  } else if (url.starts_with(http)) {
    remainder = url.substr(http.size());
  } else {
    return false;
  }
  return valid_authority(remainder.substr(0, remainder.find_first_of("/?#")));
}

} // namespace

Result<Config> load(const std::filesystem::path &site_yaml) {
  auto text = util::read_utf8_file(site_yaml);
  if (!text) {
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format("{}: 設定ファイルを読み込めません", util::to_generic_utf8(site_yaml)),
        site_yaml));
  }
  auto root = load_root(*text, site_yaml);
  if (!root) {
    return tl::unexpected(root.error());
  }
  if (!root->IsMap()) {
    return tl::unexpected(make_error(ErrorCode::Config,
                                     std::format("{}:1 site.yaml はマップである必要があります",
                                                 util::to_generic_utf8(site_yaml)),
                                     site_yaml, 1));
  }

  Config config;
  auto title = require_scalar((*root)["title"], "title", site_yaml);
  if (!title) {
    return tl::unexpected(title.error());
  }
  auto url = optional_scalar(*root, "url", site_yaml, "");
  if (!url) {
    return tl::unexpected(url.error());
  }
  if (!url->empty() && !is_absolute_http_url(*url)) {
    const int line = file_line((*root)["url"].Mark());
    return tl::unexpected(make_error(
        ErrorCode::Config,
        std::format(
            "{}:{} キー 'url' は http:// または https:// で始まる絶対 URL である必要があります",
            util::to_generic_utf8(site_yaml), line),
        site_yaml, line));
  }
  auto language = optional_scalar(*root, "language", site_yaml, "ja");
  if (!language) {
    return tl::unexpected(language.error());
  }
  auto description = optional_scalar(*root, "description", site_yaml, "");
  if (!description) {
    return tl::unexpected(description.error());
  }

  config.title = std::move(*title);
  config.url = std::move(*url);
  config.language = std::move(*language);
  config.description = std::move(*description);

  const auto pagination = (*root)["pagination"];
  if (pagination && pagination.IsDefined()) {
    if (!pagination.IsMap()) {
      const int line = file_line(pagination.Mark());
      return tl::unexpected(
          make_error(ErrorCode::Config,
                     std::format("{}:{} キー 'pagination' はマップである必要があります",
                                 util::to_generic_utf8(site_yaml), line),
                     site_yaml, line));
    }
    const auto per_page = pagination["posts_per_page"];
    if (per_page && per_page.IsDefined()) {
      if (!per_page.IsScalar()) {
        const int line = file_line(per_page.Mark());
        return tl::unexpected(make_error(
            ErrorCode::Config,
            std::format("{}:{} キー 'pagination.posts_per_page' は整数である必要があります",
                        util::to_generic_utf8(site_yaml), line),
            site_yaml, line));
      }
      try {
        config.posts_per_page = per_page.as<int>();
      } catch (const YAML::Exception &) {
        const int line = file_line(per_page.Mark());
        return tl::unexpected(make_error(
            ErrorCode::Config,
            std::format("{}:{} キー 'pagination.posts_per_page' は整数である必要があります",
                        util::to_generic_utf8(site_yaml), line),
            site_yaml, line));
      }
      if (config.posts_per_page < 0) {
        const int line = file_line(per_page.Mark());
        return tl::unexpected(make_error(
            ErrorCode::Config,
            std::format("{}:{} キー 'pagination.posts_per_page' は 0 以上である必要があります",
                        util::to_generic_utf8(site_yaml), line),
            site_yaml, line));
      }
    }
  }

  config.source_root = site_yaml.parent_path();
  config.content_dir = config.source_root / "content";
  return config;
}

} // namespace kappan::config
