#include "content/parse.hpp"

#include "content/front_matter_yaml.hpp"
#include "markdown/cmark.hpp"
#include "util/datetime.hpp"
#include "util/path.hpp"
#include "util/slug.hpp"
#include "util/utf8.hpp"

#include <yaml-cpp/yaml.h>

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kappan::content {
namespace {

struct SplitMatter {
  std::string yaml;
  std::string body;
  int yaml_start_line = 2;
  bool present = false;
};

[[nodiscard]] std::vector<std::string> split_lines(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  for (std::size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == '\n') {
      lines.emplace_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  return lines;
}

[[nodiscard]] Result<SplitMatter> split_front_matter(std::string_view text,
                                                     const std::filesystem::path &source,
                                                     const Config &config) {
  SplitMatter split;
  split.body = std::string{text};
  const auto lines = split_lines(text);
  if (lines.empty() || lines.front() != "---") {
    return split;
  }
  for (std::size_t i = 1; i < lines.size(); ++i) {
    if (lines[i] != "---") {
      continue;
    }
    split.present = true;
    std::string yaml;
    for (std::size_t j = 1; j < i; ++j) {
      if (j > 1) {
        yaml.push_back('\n');
      }
      yaml += lines[j];
    }
    split.yaml = std::move(yaml);
    std::string body;
    for (std::size_t j = i + 1; j < lines.size(); ++j) {
      if (j > i + 1) {
        body.push_back('\n');
      }
      body += lines[j];
    }
    split.body = std::move(body);
    return split;
  }
  return tl::unexpected(make_error(
      ErrorCode::FrontMatter,
      std::format("{}:1 front matter の閉じ '---' がありません", display(source, config)), source,
      1));
}

[[nodiscard]] Result<FrontMatter> parse_yaml_front_matter(const SplitMatter &split,
                                                          const std::filesystem::path &source,
                                                          const Config &config) {
  FrontMatter fm;
  if (!split.present || split.yaml.empty()) {
    return fm;
  }
  YAML::Node root;
  try {
    root = YAML::Load(split.yaml);
  } catch (const YAML::Exception &ex) {
    const int line = yaml_file_line(ex.mark, split.yaml_start_line);
    return tl::unexpected(make_error(ErrorCode::FrontMatter,
                                     std::format("{}:{} front matter の YAML を解析できません: {}",
                                                 display(source, config), line, ex.msg),
                                     source, line));
  }

  auto title = scalar_key(root["title"], "title", source, config, split.yaml_start_line);
  if (!title) {
    return tl::unexpected(title.error());
  }
  fm.title = std::move(*title);

  auto layout = scalar_key(root["layout"], "layout", source, config, split.yaml_start_line);
  if (!layout) {
    return tl::unexpected(layout.error());
  }
  fm.layout = std::move(*layout);

  auto slug = scalar_key(root["slug"], "slug", source, config, split.yaml_start_line);
  if (!slug) {
    return tl::unexpected(slug.error());
  }
  fm.slug = std::move(*slug);

  auto description =
      scalar_key(root["description"], "description", source, config, split.yaml_start_line);
  if (!description) {
    return tl::unexpected(description.error());
  }
  fm.description = std::move(*description);

  auto tags = read_tags(root["tags"], source, config, split.yaml_start_line);
  if (!tags) {
    return tl::unexpected(tags.error());
  }
  fm.tags = std::move(*tags);

  if (root["draft"] && root["draft"].IsDefined()) {
    try {
      fm.draft = root["draft"].as<bool>();
    } catch (const YAML::Exception &ex) {
      const int line = yaml_file_line(ex.mark, split.yaml_start_line);
      return tl::unexpected(
          make_error(ErrorCode::FrontMatter,
                     std::format("{}:{} front matter の 'draft' が真偽値として解釈できません",
                                 display(source, config), line),
                     source, line));
    }
  }

  if (root["date"] && root["date"].IsDefined()) {
    auto date_text = scalar_key(root["date"], "date", source, config, split.yaml_start_line);
    if (!date_text) {
      return tl::unexpected(date_text.error());
    }
    const auto parsed = util::try_parse_iso_datetime(*date_text);
    if (!parsed) {
      const int line = yaml_file_line(root["date"].Mark(), split.yaml_start_line);
      return tl::unexpected(
          make_error(ErrorCode::FrontMatter,
                     std::format("{}:{} front matter の 'date' が日付として解釈できません: '{}'",
                                 display(source, config), line, *date_text),
                     source, line));
    }
    fm.date = parsed;
  }
  return fm;
}

[[nodiscard]] bool is_home_page(const std::filesystem::path &relative) {
  return relative == std::filesystem::path{"index.md"};
}

[[nodiscard]] bool is_post(const std::filesystem::path &relative) {
  auto it = relative.begin();
  return it != relative.end() && *it == "posts";
}

void apply_source_defaults(FrontMatter &fm, const std::filesystem::path &relative,
                           const util::DatedStem &dated) {
  if (fm.title.empty()) {
    fm.title = dated.stem;
  }
  if (fm.layout.empty()) {
    if (is_home_page(relative)) {
      fm.layout = "index";
    } else if (is_post(relative)) {
      fm.layout = "post";
    } else {
      fm.layout = "page";
    }
  }
  if (!fm.date) {
    fm.date = dated.date;
  }
  if (fm.slug.empty()) {
    if (auto from_stem = util::try_slugify(dated.stem)) {
      fm.slug = std::move(*from_stem);
    } else if (auto from_title = util::try_slugify(fm.title)) {
      fm.slug = std::move(*from_title);
    } else {
      fm.slug = util::slugify(dated.stem);
    }
  } else {
    fm.slug = util::slugify(fm.slug);
  }
}

} // namespace

Result<Document> parse_document(const std::filesystem::path &source, const Config &config) {
  auto text = util::read_utf8_file(source);
  if (!text) {
    return tl::unexpected(text.error());
  }
  auto split = split_front_matter(*text, source, config);
  if (!split) {
    return tl::unexpected(split.error());
  }
  auto fm = parse_yaml_front_matter(*split, source, config);
  if (!fm) {
    return tl::unexpected(fm.error());
  }

  std::error_code ec;
  auto relative = std::filesystem::relative(source, config.content_dir, ec);
  if (ec) {
    relative = source.filename();
  }

  const auto dated = util::split_dated_stem(util::to_utf8(source.stem()));
  apply_source_defaults(*fm, relative, dated);

  Document document;
  document.source = source;
  document.front_matter = std::move(*fm);
  if (is_home_page(relative)) {
    document.permalink = "/";
  } else if (is_post(relative)) {
    document.permalink = std::format("/posts/{}/", document.front_matter.slug);
  } else {
    document.permalink = std::format("/{}/", document.front_matter.slug);
  }
  document.output_path = util::output_from_permalink(document.permalink);

  auto html = markdown::to_html(split->body, source);
  if (!html) {
    return tl::unexpected(html.error());
  }
  document.body_html = std::move(*html);
  return document;
}

} // namespace kappan::content
