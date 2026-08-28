#include <kappan/config.hpp>

#include "util/path.hpp"
#include "util/utf8.hpp"

#include <yaml-cpp/yaml.h>

#include <format>

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
