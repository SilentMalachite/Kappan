#include "render/context.hpp"

#include "render/escape.hpp"
#include "util/datetime.hpp"
#include "util/slug.hpp"
#include "util/url.hpp"

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kappan::render {
namespace {

nlohmann::json tags_json(const Document &document) {
  nlohmann::json tags = nlohmann::json::array();
  for (const auto &name : document.front_matter.tags) {
    const auto slug = util::slugify(name);
    tags.push_back({{"name", html_escape(name)},
                    {"slug", html_escape(slug)},
                    {"permalink", html_escape(std::format("/tags/{}/", slug))}});
  }
  return tags;
}

nlohmann::json document_summary(const Document &document) {
  nlohmann::json json = {
      {"title", html_escape(document.front_matter.title)},
      {"permalink", html_escape(document.permalink)},
      {"description", html_escape(document.front_matter.description)},
      {"slug", html_escape(document.front_matter.slug)},
      {"layout", html_escape(document.front_matter.layout)},
      {"tags", tags_json(document)},
      {"date", nullptr},
      {"date_display", nullptr},
  };
  if (document.front_matter.date) {
    json["date"] = util::format_iso_datetime(*document.front_matter.date);
    json["date_display"] = util::format_display_date(*document.front_matter.date);
  }
  return json;
}

nlohmann::json summaries(const Site &site, const std::vector<std::size_t> &indices) {
  nlohmann::json list = nlohmann::json::array();
  for (const auto index : indices) {
    list.push_back(document_summary(site.documents[index]));
  }
  return list;
}

nlohmann::json site_json(const Config &config) {
  return {{"title", html_escape(config.title)},
          {"url", html_escape(config.url)},
          {"language", html_escape(config.language)},
          {"description", html_escape(config.description)}};
}

nlohmann::json collections_json(const Site &site) {
  return {{"posts", summaries(site, site.posts.indices)},
          {"pages", summaries(site, site.pages.indices)}};
}

nlohmann::json pagination_json(const Site &site, const site::Pagination &pagination) {
  return {
      {"page", pagination.page},
      {"pages", pagination.pages},
      {"prev", pagination.prev.empty() ? nlohmann::json(nullptr) : nlohmann::json(pagination.prev)},
      {"next", pagination.next.empty() ? nlohmann::json(nullptr) : nlohmann::json(pagination.next)},
      {"posts", summaries(site, pagination.indices)}};
}

nlohmann::json actions_json(const std::vector<LandingAction> &actions) {
  nlohmann::json list = nlohmann::json::array();
  for (const auto &action : actions) {
    list.push_back({{"label", html_escape(action.label)}, {"href", html_escape(action.href)}});
  }
  return list;
}

nlohmann::json items_json(const std::vector<LandingItem> &items) {
  nlohmann::json list = nlohmann::json::array();
  for (const auto &item : items) {
    list.push_back({{"title", html_escape(item.title)},
                    {"text", html_escape(item.text)},
                    {"icon", html_escape(item.icon)}});
  }
  return list;
}

nlohmann::json sections_json(const std::vector<LandingSection> &sections) {
  nlohmann::json list = nlohmann::json::array();
  for (const auto &section : sections) {
    list.push_back({{"type", html_escape(section.type)},
                    {"eyebrow", html_escape(section.eyebrow)},
                    {"title", html_escape(section.title)},
                    {"text", html_escape(section.text)},
                    {"image", html_escape(section.image)},
                    {"actions", actions_json(section.actions)},
                    {"items", items_json(section.items)}});
  }
  return list;
}

// 同梱テンプレートの {% block title %} と同じ式にする。
std::string title_with_site(const Config &config, std::string_view title) {
  if (title.empty()) {
    return config.title;
  }
  return std::format("{} — {}", title, config.title);
}

// site.url が空、または絶対化できない相対 URL のときは空を返す。壊れた OGP を出さないため。
std::string absolute_url(const Config &config, std::string_view maybe_relative) {
  if (maybe_relative.starts_with("http://") || maybe_relative.starts_with("https://")) {
    return std::string{maybe_relative};
  }
  if (config.url.empty() || !maybe_relative.starts_with('/')) {
    return {};
  }
  return util::join_url(config.url, maybe_relative);
}

struct OgInput {
  std::string_view title;
  std::string_view description;
  std::string_view layout;
  std::string_view permalink;
  std::string_view image;
};

nlohmann::json og_json(const Config &config, const OgInput &input) {
  const std::string_view description =
      input.description.empty() ? std::string_view{config.description} : input.description;
  const auto image = absolute_url(config, input.image);
  const std::string url =
      config.url.empty() ? std::string{} : util::join_url(config.url, input.permalink);
  return {{"title", html_escape(title_with_site(config, input.title))},
          {"description", html_escape(description)},
          {"type", input.layout == "post" ? "article" : "website"},
          {"url", html_escape(url)},
          {"image", html_escape(image)},
          {"twitter_card", image.empty() ? "" : "summary_large_image"}};
}

nlohmann::json og_json(const Config &config, const Document &document) {
  return og_json(config, OgInput{document.front_matter.title, document.front_matter.description,
                                 document.front_matter.layout, document.permalink,
                                 document.front_matter.image});
}

nlohmann::json page_json(const Config &config, const Document &document) {
  auto json = document_summary(document);
  json["content"] = document.body_html;
  json["image"] = html_escape(document.front_matter.image);
  json["sections"] = sections_json(document.front_matter.sections);
  json["og"] = og_json(config, document);
  return json;
}

} // namespace

nlohmann::json make_context(const Site &site, const Document &document,
                            const site::Pagination *pagination) {
  nlohmann::json root = {{"site", site_json(site.config)},
                         {"page", page_json(site.config, document)},
                         {"collections", collections_json(site)},
                         {"tag", nullptr}};
  if (pagination != nullptr) {
    root["pagination"] = pagination_json(site, *pagination);
  } else {
    root["pagination"] = nullptr;
  }
  return root;
}

nlohmann::json make_listing_context(const Site &site, const site::Pagination &pagination) {
  const auto title =
      pagination.page == 1 ? site.config.title : std::format("ページ {}", pagination.page);
  nlohmann::json page = {
      {"title", html_escape(title)},
      {"permalink", html_escape(pagination.permalink)},
      {"content", ""},
      {"layout", "index"},
      {"description", html_escape(site.config.description)},
      {"slug", ""},
      {"tags", nlohmann::json::array()},
      {"date", nullptr},
      {"date_display", nullptr},
      {"image", ""},
      {"sections", nlohmann::json::array()},
      {"og", og_json(site.config,
                     OgInput{title, site.config.description, "index", pagination.permalink, ""})},
  };
  return {{"site", site_json(site.config)},
          {"page", std::move(page)},
          {"collections", collections_json(site)},
          {"pagination", pagination_json(site, pagination)},
          {"tag", nullptr}};
}

nlohmann::json make_tag_context(const Site &site, const TaxonomyTerm &term) {
  site::Pagination pagination;
  pagination.page = 1;
  pagination.pages = 1;
  pagination.permalink = term.permalink;
  pagination.indices = term.indices;
  nlohmann::json page = {
      {"title", html_escape(term.name)},
      {"permalink", html_escape(term.permalink)},
      {"content", ""},
      {"layout", "tag"},
      {"description", html_escape(site.config.description)},
      {"slug", html_escape(term.slug)},
      {"tags", nlohmann::json::array()},
      {"date", nullptr},
      {"date_display", nullptr},
      {"image", ""},
      {"sections", nlohmann::json::array()},
      {"og", og_json(site.config,
                     OgInput{term.name, site.config.description, "tag", term.permalink, ""})},
  };
  nlohmann::json tag = {{"name", html_escape(term.name)},
                        {"slug", html_escape(term.slug)},
                        {"permalink", html_escape(term.permalink)}};
  return {{"site", site_json(site.config)},
          {"page", std::move(page)},
          {"collections", collections_json(site)},
          {"pagination", pagination_json(site, pagination)},
          {"tag", std::move(tag)}};
}

} // namespace kappan::render
