#include "render/context.hpp"

#include "render/escape.hpp"
#include "util/datetime.hpp"

namespace kappan::render {

nlohmann::json make_context(const Config &config, const Document &document) {
  nlohmann::json tags = nlohmann::json::array();
  for (const auto &tag : document.front_matter.tags) {
    tags.push_back(html_escape(tag));
  }

  nlohmann::json page = {
      {"title", html_escape(document.front_matter.title)},
      {"permalink", html_escape(document.permalink)},
      {"content", document.body_html},
      {"layout", html_escape(document.front_matter.layout)},
      {"description", html_escape(document.front_matter.description)},
      {"slug", html_escape(document.front_matter.slug)},
      {"tags", std::move(tags)},
      {"date", nullptr},
  };
  if (document.front_matter.date) {
    page["date"] = util::format_iso_datetime(*document.front_matter.date);
  }

  return {
      {"site",
       {{"title", html_escape(config.title)},
        {"url", html_escape(config.url)},
        {"language", html_escape(config.language)},
        {"description", html_escape(config.description)}}},
      {"page", std::move(page)},
  };
}

} // namespace kappan::render
