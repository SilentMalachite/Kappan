#include <kappan/site.hpp>

#include "util/slug.hpp"

#include <algorithm>
#include <format>
#include <map>
#include <ranges>
#include <utility>
#include <vector>

namespace kappan::site {
namespace {

[[nodiscard]] bool is_post(const Document &document) {
  return document.permalink.starts_with("/posts/");
}

[[nodiscard]] bool is_home(const Document &document) { return document.permalink == "/"; }

[[nodiscard]] bool recency_before(const Document &left, const Document &right) {
  if (left.front_matter.date && right.front_matter.date) {
    if (*left.front_matter.date != *right.front_matter.date) {
      return *left.front_matter.date > *right.front_matter.date;
    }
    return left.front_matter.slug < right.front_matter.slug;
  }
  if (left.front_matter.date && !right.front_matter.date) {
    return true;
  }
  if (!left.front_matter.date && right.front_matter.date) {
    return false;
  }
  return left.front_matter.slug < right.front_matter.slug;
}

void sort_by_recency(Site &site, std::vector<std::size_t> &indices) {
  std::ranges::sort(indices, [&](std::size_t left, std::size_t right) {
    return recency_before(site.documents[left], site.documents[right]);
  });
}

[[nodiscard]] Taxonomy build_tags(const Site &site) {
  Taxonomy tags;
  tags.name = "tags";
  std::map<std::string, TaxonomyTerm> by_slug;
  for (const auto index : site.posts.indices) {
    const auto &document = site.documents[index];
    for (const auto &name : document.front_matter.tags) {
      auto slug = util::slugify(name);
      auto it = by_slug.find(slug);
      if (it == by_slug.end()) {
        TaxonomyTerm term;
        term.name = name;
        term.slug = slug;
        term.permalink = std::format("/tags/{}/", slug);
        it = by_slug.emplace(std::move(slug), std::move(term)).first;
      }
      it->second.indices.push_back(index);
    }
  }
  for (auto &[_, term] : by_slug) {
    tags.terms.push_back(std::move(term));
  }
  std::ranges::sort(tags.terms, {}, &TaxonomyTerm::slug);
  return tags;
}

} // namespace

Site build(Config config, std::vector<Document> documents, DraftPolicy drafts) {
  if (drafts == DraftPolicy::Exclude) {
    std::erase_if(documents, [](const Document &document) { return document.front_matter.draft; });
  }

  Site site;
  site.config = std::move(config);
  site.documents = std::move(documents);
  site.posts.name = "posts";
  site.pages.name = "pages";

  for (std::size_t i = 0; i < site.documents.size(); ++i) {
    const auto &document = site.documents[i];
    if (is_post(document)) {
      site.posts.indices.push_back(i);
    } else if (!is_home(document)) {
      site.pages.indices.push_back(i);
    }
  }
  sort_by_recency(site, site.posts.indices);
  std::ranges::sort(site.pages.indices, [&](std::size_t left, std::size_t right) {
    return site.documents[left].front_matter.slug < site.documents[right].front_matter.slug;
  });
  site.tags = build_tags(site);
  return site;
}

} // namespace kappan::site
