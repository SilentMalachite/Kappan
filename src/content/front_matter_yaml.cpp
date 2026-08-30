#include "content/front_matter_yaml.hpp"

#include "util/path.hpp"

#include <array>
#include <format>
#include <system_error>
#include <utility>

namespace kappan::content {
namespace {

[[nodiscard]] Error sequence_error(std::string_view key, const YAML::Node &node,
                                   const std::filesystem::path &source, const Config &config,
                                   int yaml_start_line) {
  const int line = yaml_file_line(node.Mark(), yaml_start_line);
  return make_error(ErrorCode::FrontMatter,
                    std::format("{}:{} front matter の '{}' はマップの配列である必要があります",
                                display(source, config), line, key),
                    source, line);
}

[[nodiscard]] Error element_error(std::string_view key, const YAML::Node &node,
                                  const std::filesystem::path &source, const Config &config,
                                  int yaml_start_line) {
  const int line = yaml_file_line(node.Mark(), yaml_start_line);
  return make_error(ErrorCode::FrontMatter,
                    std::format("{}:{} front matter の '{}' の要素はマップである必要があります",
                                display(source, config), line, key),
                    source, line);
}

[[nodiscard]] Result<std::vector<LandingAction>> read_actions(const YAML::Node &node,
                                                              const std::filesystem::path &source,
                                                              const Config &config,
                                                              int yaml_start_line) {
  std::vector<LandingAction> actions;
  if (!node || !node.IsDefined()) {
    return actions;
  }
  if (!node.IsSequence()) {
    return tl::unexpected(
        sequence_error("sections.actions", node, source, config, yaml_start_line));
  }
  for (const auto &entry : node) {
    if (!entry.IsMap()) {
      return tl::unexpected(
          element_error("sections.actions", entry, source, config, yaml_start_line));
    }
    LandingAction action;
    auto label =
        scalar_key(entry["label"], "sections.actions.label", source, config, yaml_start_line);
    if (!label) {
      return tl::unexpected(label.error());
    }
    action.label = std::move(*label);
    auto href = scalar_key(entry["href"], "sections.actions.href", source, config, yaml_start_line);
    if (!href) {
      return tl::unexpected(href.error());
    }
    action.href = std::move(*href);
    actions.push_back(std::move(action));
  }
  return actions;
}

[[nodiscard]] Result<std::vector<LandingItem>> read_items(const YAML::Node &node,
                                                          const std::filesystem::path &source,
                                                          const Config &config,
                                                          int yaml_start_line) {
  std::vector<LandingItem> items;
  if (!node || !node.IsDefined()) {
    return items;
  }
  if (!node.IsSequence()) {
    return tl::unexpected(sequence_error("sections.items", node, source, config, yaml_start_line));
  }
  for (const auto &entry : node) {
    if (!entry.IsMap()) {
      return tl::unexpected(
          element_error("sections.items", entry, source, config, yaml_start_line));
    }
    LandingItem item;
    const std::array<std::pair<const char *, std::string LandingItem::*>, 3> fields{{
        {"title", &LandingItem::title},
        {"text", &LandingItem::text},
        {"icon", &LandingItem::icon},
    }};
    for (const auto &[key, field] : fields) {
      auto value = scalar_key(entry[key], std::format("sections.items.{}", key), source, config,
                              yaml_start_line);
      if (!value) {
        return tl::unexpected(value.error());
      }
      item.*field = std::move(*value);
    }
    items.push_back(std::move(item));
  }
  return items;
}

[[nodiscard]] Result<LandingSection> read_section(const YAML::Node &node,
                                                  const std::filesystem::path &source,
                                                  const Config &config, int yaml_start_line) {
  LandingSection section;
  const std::array<std::pair<const char *, std::string LandingSection::*>, 5> fields{{
      {"type", &LandingSection::type},
      {"eyebrow", &LandingSection::eyebrow},
      {"title", &LandingSection::title},
      {"text", &LandingSection::text},
      {"image", &LandingSection::image},
  }};
  for (const auto &[key, field] : fields) {
    auto value =
        scalar_key(node[key], std::format("sections.{}", key), source, config, yaml_start_line);
    if (!value) {
      return tl::unexpected(value.error());
    }
    section.*field = std::move(*value);
  }
  auto actions = read_actions(node["actions"], source, config, yaml_start_line);
  if (!actions) {
    return tl::unexpected(actions.error());
  }
  section.actions = std::move(*actions);
  auto items = read_items(node["items"], source, config, yaml_start_line);
  if (!items) {
    return tl::unexpected(items.error());
  }
  section.items = std::move(*items);
  return section;
}

} // namespace

std::string display(const std::filesystem::path &source, const Config &config) {
  std::error_code ec;
  const auto rel = std::filesystem::relative(source, config.source_root, ec);
  if (ec) {
    return util::to_generic_utf8(source);
  }
  return util::to_generic_utf8(rel);
}

int yaml_file_line(const YAML::Mark &mark, int yaml_start_line) {
  if (mark.is_null() || mark.line < 0) {
    return yaml_start_line;
  }
  return yaml_start_line + mark.line;
}

Result<std::string> scalar_key(const YAML::Node &node, std::string_view key,
                               const std::filesystem::path &source, const Config &config,
                               int yaml_start_line) {
  if (!node || !node.IsDefined()) {
    return std::string{};
  }
  if (!node.IsScalar()) {
    const int line = yaml_file_line(node.Mark(), yaml_start_line);
    return tl::unexpected(
        make_error(ErrorCode::FrontMatter,
                   std::format("{}:{} front matter の '{}' は文字列である必要があります",
                               display(source, config), line, key),
                   source, line));
  }
  return node.Scalar();
}

Result<std::vector<std::string>> read_tags(const YAML::Node &node,
                                           const std::filesystem::path &source,
                                           const Config &config, int yaml_start_line) {
  std::vector<std::string> tags;
  if (!node || !node.IsDefined()) {
    return tags;
  }
  if (!node.IsSequence()) {
    const int line = yaml_file_line(node.Mark(), yaml_start_line);
    return tl::unexpected(
        make_error(ErrorCode::FrontMatter,
                   std::format("{}:{} front matter の 'tags' は文字列の配列である必要があります",
                               display(source, config), line),
                   source, line));
  }
  for (const auto &item : node) {
    if (!item.IsScalar()) {
      const int line = yaml_file_line(item.Mark(), yaml_start_line);
      return tl::unexpected(
          make_error(ErrorCode::FrontMatter,
                     std::format("{}:{} front matter の 'tags' の要素は文字列である必要があります",
                                 display(source, config), line),
                     source, line));
    }
    tags.push_back(item.Scalar());
  }
  return tags;
}

Result<std::vector<LandingSection>> read_sections(const YAML::Node &node,
                                                  const std::filesystem::path &source,
                                                  const Config &config, int yaml_start_line) {
  std::vector<LandingSection> sections;
  if (!node || !node.IsDefined()) {
    return sections;
  }
  if (!node.IsSequence()) {
    return tl::unexpected(sequence_error("sections", node, source, config, yaml_start_line));
  }
  for (const auto &entry : node) {
    if (!entry.IsMap()) {
      return tl::unexpected(element_error("sections", entry, source, config, yaml_start_line));
    }
    auto section = read_section(entry, source, config, yaml_start_line);
    if (!section) {
      return tl::unexpected(section.error());
    }
    sections.push_back(std::move(*section));
  }
  return sections;
}

} // namespace kappan::content
