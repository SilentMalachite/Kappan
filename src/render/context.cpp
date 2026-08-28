#include "render/context.hpp"

#include "render/escape.hpp"
#include "util/datetime.hpp"
#include "util/slug.hpp"

#include <format>
#include <utility>

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

nlohmann::json page_json(const Document &document) {
  auto json = document_summary(document);
  json["content"] = document.body_html;
  return json;
}

} // namespace

nlohmann::json make_context(const Site &site, const Document &document,
                            const site::Pagination *pagination) {
  nlohmann::json root = {{"site", site_json(site.config)},
                         {"page", page_json(document)},
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
