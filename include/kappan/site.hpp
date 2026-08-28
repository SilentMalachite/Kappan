#pragma once

#include <kappan/config.hpp>
#include <kappan/document.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace kappan {

enum class DraftPolicy { Exclude, Include };

struct Collection {
  std::string name;
  std::vector<std::size_t> indices;
};

struct TaxonomyTerm {
  std::string name;
  std::string slug;
  std::string permalink;
  std::vector<std::size_t> indices;
};

struct Taxonomy {
  std::string name;
  std::vector<TaxonomyTerm> terms;
};

struct Site {
  Config config;
  std::vector<Document> documents;
  Collection posts;
  Collection pages;
  Taxonomy tags;
};

namespace site {

[[nodiscard]] Site build(Config config, std::vector<Document> documents, DraftPolicy drafts);

} // namespace site
} // namespace kappan
