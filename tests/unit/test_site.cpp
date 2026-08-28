#include <kappan/document.hpp>
#include <kappan/site.hpp>

#include "site/paginate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace {

kappan::Document make_doc(std::string permalink, std::string slug, bool draft = false,
                          std::optional<std::chrono::sys_seconds> date = {},
                          std::vector<std::string> tags = {}) {
  kappan::Document document;
  document.permalink = std::move(permalink);
  document.front_matter.slug = std::move(slug);
  document.front_matter.title = document.front_matter.slug;
  document.front_matter.draft = draft;
  document.front_matter.date = date;
  document.front_matter.tags = std::move(tags);
  return document;
}

std::chrono::sys_seconds day(int y, unsigned m, unsigned d) {
  return std::chrono::sys_days{std::chrono::year{y} / std::chrono::month{m} / std::chrono::day{d}};
}

} // namespace

TEST_CASE("site::build sorts posts by date descending and leaves undated last") {
  kappan::Config config;
  std::vector<kappan::Document> documents;
  documents.push_back(make_doc("/posts/old/", "old", false, day(2026, 1, 1)));
  documents.push_back(make_doc("/posts/new/", "new", false, day(2026, 1, 3)));
  documents.push_back(make_doc("/posts/none/", "none"));
  documents.push_back(make_doc("/posts/mid/", "mid", false, day(2026, 1, 2)));
  documents.push_back(make_doc("/about/", "about"));
  documents.push_back(make_doc("/", "home"));

  const auto site = kappan::site::build(config, std::move(documents), kappan::DraftPolicy::Exclude);
  REQUIRE(site.posts.indices.size() == 4);
  REQUIRE(site.documents[site.posts.indices[0]].front_matter.slug == "new");
  REQUIRE(site.documents[site.posts.indices[1]].front_matter.slug == "mid");
  REQUIRE(site.documents[site.posts.indices[2]].front_matter.slug == "old");
  REQUIRE(site.documents[site.posts.indices[3]].front_matter.slug == "none");
  REQUIRE(site.pages.indices.size() == 1);
  REQUIRE(site.documents[site.pages.indices[0]].permalink == "/about/");
}

TEST_CASE("site::build excludes drafts unless included") {
  kappan::Config config;
  std::vector<kappan::Document> documents;
  documents.push_back(make_doc("/posts/公開/", "公開", false, day(2026, 1, 1), {"日本語"}));
  documents.push_back(make_doc("/posts/下書き/", "下書き", true, day(2026, 1, 2), {"日本語"}));

  const auto hidden = kappan::site::build(config, documents, kappan::DraftPolicy::Exclude);
  REQUIRE(hidden.documents.size() == 1);
  REQUIRE(hidden.posts.indices.size() == 1);
  REQUIRE(hidden.tags.terms.size() == 1);
  REQUIRE(hidden.tags.terms[0].indices.size() == 1);

  const auto shown = kappan::site::build(config, documents, kappan::DraftPolicy::Include);
  REQUIRE(shown.documents.size() == 2);
  REQUIRE(shown.posts.indices.size() == 2);
  REQUIRE(shown.documents[shown.posts.indices[0]].front_matter.slug == "下書き");
}

TEST_CASE("site::build groups Japanese tags") {
  kappan::Config config;
  std::vector<kappan::Document> documents;
  documents.push_back(make_doc("/posts/a/", "a", false, day(2026, 1, 2), {"日本語", "絵文字"}));
  documents.push_back(make_doc("/posts/b/", "b", false, day(2026, 1, 1), {"日本語"}));

  const auto site = kappan::site::build(config, std::move(documents), kappan::DraftPolicy::Exclude);
  REQUIRE(site.tags.terms.size() == 2);
  REQUIRE(site.tags.terms[0].slug == "日本語");
  REQUIRE(site.tags.terms[0].permalink == "/tags/日本語/");
  REQUIRE(site.tags.terms[0].indices.size() == 2);
  REQUIRE(site.tags.terms[1].slug == "絵文字");
}

TEST_CASE("paginate splits eleven posts into two pages") {
  std::vector<std::size_t> indices(11);
  for (std::size_t i = 0; i < indices.size(); ++i) {
    indices[i] = i;
  }
  const auto pages = kappan::site::paginate(indices, 10);
  REQUIRE(pages.size() == 2);
  REQUIRE(pages[0].permalink == "/");
  REQUIRE(pages[0].indices.size() == 10);
  REQUIRE(pages[0].next == "/page/2/");
  REQUIRE(pages[1].permalink == "/page/2/");
  REQUIRE(pages[1].indices.size() == 1);
  REQUIRE(pages[1].prev == "/");
  REQUIRE(pages[1].next.empty());
}
