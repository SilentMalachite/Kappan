#include "content/front_matter_yaml.hpp"

#include "util/path.hpp"

#include <format>
#include <system_error>

namespace kappan::content {
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

} // namespace kappan::content
