#pragma once

#include "site/paginate.hpp"

#include <kappan/site.hpp>

#include <nlohmann/json.hpp>

namespace kappan::render {

[[nodiscard]] nlohmann::json make_context(const Site &site, const Document &document,
                                          const site::Pagination *pagination);

[[nodiscard]] nlohmann::json make_listing_context(const Site &site,
                                                  const site::Pagination &pagination);

[[nodiscard]] nlohmann::json make_tag_context(const Site &site, const TaxonomyTerm &term);

} // namespace kappan::render
