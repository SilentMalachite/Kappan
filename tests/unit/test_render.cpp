#include <kappan/config.hpp>
#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include "content/parse.hpp"
#include "render/context.hpp"
#include "render/engine.hpp"
#include "render/escape.hpp"
#include "site/paginate.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

kappan::Config load_ja_config() {
  const auto loaded = kappan::config::load(fixtures_dir() / "site-ja" / "site.yaml");
  REQUIRE(loaded);
  return *loaded;
}

kappan::Config listing_config() {
  kappan::Config config;
  config.title = "一覧サイト";
  config.language = "ja";
  config.description = "一覧の説明";
  config.source_root = std::filesystem::path{"fixtures"} / "generated-home";
  config.content_dir = config.source_root / "content";
  return config;
}

kappan::Document post_document(std::string title, std::string slug,
                               std::vector<std::string> tags = {}) {
  kappan::Document document;
  document.source = std::filesystem::path{"content/posts"} / (slug + ".md");
  document.front_matter.title = std::move(title);
  document.front_matter.layout = "post";
  document.front_matter.slug = slug;
  document.front_matter.tags = std::move(tags);
  document.body_html = "<p>日本語の本文🐙</p>\n";
  document.output_path = std::filesystem::path{"posts"} / slug / "index.html";
  document.permalink = "/posts/" + slug + "/";
  return document;
}

kappan::Document page_document(std::string title, std::string layout, std::string slug,
                               std::string permalink) {
  kappan::Document document;
  document.source = std::filesystem::path{"content"} / (slug + ".md");
  document.front_matter.title = std::move(title);
  document.front_matter.layout = std::move(layout);
  document.front_matter.slug = std::move(slug);
  document.body_html = "<p>明示ページの本文</p>\n";
  document.output_path = kappan::util::output_from_permalink(permalink);
  document.permalink = std::move(permalink);
  return document;
}

void require_exact_title_and_og(const std::string &html, const std::string &expected) {
  const auto title = "<title>" + expected + "</title>";
  const auto og = "<meta property=\"og:title\" content=\"" + expected + "\">";
  INFO("期待する title 要素: " << title);
  INFO("期待する og:title 要素: " << og);
  INFO("実際の HTML: " << html);
  REQUIRE(html.find(title) != std::string::npos);
  REQUIRE(html.find(og) != std::string::npos);
}

} // namespace

TEST_CASE("html_escape keeps Japanese and encodes markup") {
  REQUIRE(kappan::render::html_escape("こんにちは & <世界>") == "こんにちは &amp; &lt;世界&gt;");
}

TEST_CASE("Engine renders a Japanese post with base and post templates") {
  const auto root = fixtures_dir() / "site-ja";
  const auto source =
      root / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md");
  const auto document = kappan::content::parse_document(source, load_ja_config());
  REQUIRE(document);

  auto engine = kappan::render::Engine::load(load_ja_config());
  REQUIRE(engine);
  auto site = kappan::site::build(load_ja_config(), {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  REQUIRE(page->output_path == document->output_path);
  REQUIRE(page->html.find("<!DOCTYPE html>") != std::string::npos);
  REQUIRE(page->html.find("<html lang=\"ja\">") != std::string::npos);
  REQUIRE(page->html.find("<meta charset=\"utf-8\">") != std::string::npos);
  REQUIRE(page->html.find("こんにちは — 活版ブログ") != std::string::npos);
  REQUIRE(page->html.find("<article>") != std::string::npos);
  REQUIRE(page->html.find("最初の記事です") != std::string::npos);
  REQUIRE(page->html.find("datetime=\"2026-01-01\"") != std::string::npos);
  REQUIRE_FALSE(page->html.starts_with("\xEF\xBB\xBF"));
}

TEST_CASE("Engine reports a missing layout template") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-missing-layout";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  {
    std::ofstream out(root / "site.yaml", std::ios::binary);
    out << "title: 欠け\n";
  }
  const auto source = content / kappan::util::from_utf8("欠ける.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 欠ける\nlayout: missing\n---\n本文\n";
  }
  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE_FALSE(page);
  REQUIRE(page.error().code == kappan::ErrorCode::Template);
  REQUIRE(page.error().message.find("missing.html") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("Engine lets site templates override the embedded post layout") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-override-theme";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  std::filesystem::create_directories(root / "templates");
  {
    std::ofstream out(root / "site.yaml", std::ios::binary);
    out << "title: 上書きサイト\nlanguage: ja\n";
  }
  {
    std::ofstream out(root / "templates" / "post.html", std::ios::binary);
    out << "<p>上書き {{ page.title }}</p>\n";
  }
  const auto source = content / kappan::util::from_utf8("記事.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 見出し\nlayout: post\n---\n本文\n";
  }
  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  REQUIRE(page->html.find("上書き 見出し") != std::string::npos);
  REQUIRE(page->html.find("<article>") == std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("html_escape drops forbidden control characters too") {
  REQUIRE(kappan::render::html_escape("前"
                                      "\x0c"
                                      "後") == "前後");
  REQUIRE(kappan::render::html_escape("a\tb\nc") == "a\tb\nc");
  REQUIRE(kappan::render::html_escape("日本語 🐙") == "日本語 🐙");
}

TEST_CASE("Engine renders a landing page with sections and OGP") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-render-landing";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  std::filesystem::create_directories(root / "templates");
  {
    std::ofstream yaml(root / "site.yaml", std::ios::binary);
    yaml << "title: 活版LP\nurl: https://example.com\nlanguage: ja\ndescription: サイト説明\n";
  }
  {
    std::ofstream tmpl(root / "templates" / "landing.html", std::ios::binary);
    tmpl << "<p data-og-title=\"{{ page.og.title }}\"></p>\n"
            "<p data-page-image=\"{{ page.image }}\"></p>\n"
            "<p data-og-url=\"{{ page.og.url }}\"></p>\n"
            "<p data-og-image=\"{{ page.og.image }}\"></p>\n"
            "{% for section in page.sections %}\n"
            "<section data-type=\"{{ section.type }}\"><h2>{{ section.title }}</h2>"
            "{% for action in section.actions %}<a href=\"{{ action.href }}\">{{ action.label "
            "}}</a>{% endfor %}"
            "</section>\n"
            "{% endfor %}\n"
            "{{ page.content }}\n";
  }
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: 日本語LP 🐙\n"
           "layout: landing\n"
           "description: LP説明\n"
           "image: /images/og.svg\n"
           "sections:\n"
           "  - type: hero\n"
           "    title: '<強い見出し>'\n"
           "    actions:\n"
           "      - label: 開く\n"
           "        href: '#start'\n"
           "---\n"
           "本文 <strong>HTML</strong>\n";
  }

  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  REQUIRE(page->html.find("data-og-title=\"日本語LP 🐙 — 活版LP\"") != std::string::npos);
  REQUIRE(page->html.find("data-page-image=\"/images/og.svg\"") != std::string::npos);
  REQUIRE(page->html.find("data-og-url=\"https://example.com/\"") != std::string::npos);
  REQUIRE(page->html.find("data-og-image=\"https://example.com/images/og.svg\"") !=
          std::string::npos);
  REQUIRE(page->html.find("<section data-type=\"hero\">") != std::string::npos);
  REQUIRE(page->html.find("&lt;強い見出し&gt;") != std::string::npos);
  REQUIRE(page->html.find("<strong>HTML</strong>") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("Engine omits og:url and relative og:image when site url is empty") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-render-landing-no-url";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  std::filesystem::create_directories(root / "templates");
  {
    std::ofstream yaml(root / "site.yaml", std::ios::binary);
    yaml << "title: URLなし\nlanguage: ja\ndescription: サイト説明\n";
  }
  {
    std::ofstream tmpl(root / "templates" / "landing.html", std::ios::binary);
    tmpl << "<p data-og-title=\"{{ page.og.title }}\"></p>\n"
            "<p data-og-description=\"{{ page.og.description }}\"></p>\n"
            "<p data-og-url=\"{{ page.og.url }}\"></p>\n"
            "<p data-og-image=\"{{ page.og.image }}\"></p>\n";
  }
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: 共有なし\n"
           "layout: landing\n"
           "description: ページ説明\n"
           "image: /images/og.svg\n"
           "---\n"
           "本文\n";
  }

  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  REQUIRE(page->html.find("data-og-title=\"共有なし — URLなし\"") != std::string::npos);
  REQUIRE(page->html.find("data-og-description=\"ページ説明\"") != std::string::npos);
  REQUIRE(page->html.find("data-og-url=\"\"") != std::string::npos);
  REQUIRE(page->html.find("data-og-image=\"\"") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("Engine renders landing with the embedded default template") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-render-embedded-landing";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  {
    std::ofstream yaml(root / "site.yaml", std::ios::binary);
    yaml << "title: 活版LP\nurl: https://example.com\nlanguage: ja\n";
  }
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: LP\n"
           "layout: landing\n"
           "sections:\n"
           "  - type: hero\n"
           "    title: ヒーロー\n"
           "    text: 本文\n"
           "---\n"
           "補足\n";
  }
  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  REQUIRE(page->html.find("ヒーロー") != std::string::npos);
  REQUIRE(page->html.find("og:type") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("Embedded base omits og:url and og:image meta when site url is empty") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-render-embedded-no-url";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  {
    std::ofstream yaml(root / "site.yaml", std::ios::binary);
    yaml << "title: URLなし\nlanguage: ja\ndescription: サイト説明\n";
  }
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: 共有なし\n"
           "layout: landing\n"
           "description: ページ説明\n"
           "image: /images/og.svg\n"
           "sections:\n"
           "  - type: hero\n"
           "    title: ヒーロー\n"
           "---\n"
           "本文\n";
  }
  const auto config = kappan::config::load(root / "site.yaml");
  REQUIRE(config);
  const auto document = kappan::content::parse_document(source, *config);
  REQUIRE(document);
  auto engine = kappan::render::Engine::load(*config);
  REQUIRE(engine);
  auto site = kappan::site::build(*config, {*document}, kappan::DraftPolicy::Include);
  const auto page = engine->render(site, site.documents.front());
  REQUIRE(page);
  // 値のある meta は出る。
  REQUIRE(page->html.find("<meta property=\"og:title\" content=\"共有なし — URLなし\">") !=
          std::string::npos);
  REQUIRE(page->html.find("<meta property=\"og:type\" content=\"website\">") != std::string::npos);
  REQUIRE(page->html.find("<meta property=\"og:description\" content=\"ページ説明\">") !=
          std::string::npos);
  // 絶対化できないものは meta 要素ごと出さない。空 content を書かない。
  REQUIRE(page->html.find("og:url") == std::string::npos);
  REQUIRE(page->html.find("og:image") == std::string::npos);
  REQUIRE(page->html.find("twitter:card") == std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("generated home emits the site title once in HTML and OGP") {
  auto config = listing_config();
  const auto site = kappan::site::build(config, {post_document("記事🐙", "article-octopus")},
                                        kappan::DraftPolicy::Include);
  const auto pages = kappan::site::paginate(site.posts.indices, site.config.posts_per_page);
  REQUIRE(pages.size() == 1);

  const auto context = kappan::render::make_listing_context(site, pages.front());
  REQUIRE(context["page"]["title"] == "一覧サイト");
  REQUIRE(context["page"]["generated_listing"] == true);
  REQUIRE(context["page"]["og"]["title"] == "一覧サイト");

  auto engine = kappan::render::Engine::load(config);
  REQUIRE(engine);
  const auto rendered = engine->render_listing(site, 1);
  REQUIRE(rendered);
  require_exact_title_and_og(rendered->html, "一覧サイト");
  REQUIRE(rendered->html.find("一覧サイト — 一覧サイト") == std::string::npos);
}

TEST_CASE("generated listing page 2 appends the site title in HTML and OGP") {
  const std::vector<std::pair<std::string, std::string>> title_cases = {
      {"一覧サイト", "ページ 2 — 一覧サイト"},
      {"ページ 2", "ページ 2 — ページ 2"},
  };

  for (const auto &[site_title, expected_title] : title_cases) {
    DYNAMIC_SECTION("site title: " << site_title) {
      auto config = listing_config();
      config.title = site_title;
      config.posts_per_page = 1;
      const auto site = kappan::site::build(
          config, {post_document("新しい記事🐙", "newer"), post_document("古い記事🐙", "older")},
          kappan::DraftPolicy::Include);
      const auto pages = kappan::site::paginate(site.posts.indices, site.config.posts_per_page);
      REQUIRE(pages.size() == 2);

      const auto context = kappan::render::make_listing_context(site, pages[1]);
      REQUIRE(context["page"]["title"] == "ページ 2");
      REQUIRE(context["page"]["generated_listing"] == true);
      REQUIRE(context["page"]["og"]["title"] == expected_title);

      auto engine = kappan::render::Engine::load(config);
      REQUIRE(engine);
      const auto rendered = engine->render_listing(site, 2);
      REQUIRE(rendered);
      require_exact_title_and_og(rendered->html, expected_title);
    }
  }
}

TEST_CASE("explicit index and normal post keep the site title suffix") {
  auto config = listing_config();
  auto home = page_document("ホーム", "index", "index", "/");
  auto post = post_document("記事", "article");
  const auto site = kappan::site::build(config, {home, post}, kappan::DraftPolicy::Include);
  auto engine = kappan::render::Engine::load(config);
  REQUIRE(engine);

  const auto home_context = kappan::render::make_context(site, site.documents[0], nullptr);
  REQUIRE(home_context["page"]["generated_listing"] == false);
  const auto rendered_home = engine->render(site, site.documents[0]);
  REQUIRE(rendered_home);
  require_exact_title_and_og(rendered_home->html, "ホーム — 一覧サイト");

  const auto post_context = kappan::render::make_context(site, site.documents[1], nullptr);
  REQUIRE(post_context["page"]["generated_listing"] == false);
  const auto rendered_post = engine->render(site, site.documents[1]);
  REQUIRE(rendered_post);
  require_exact_title_and_og(rendered_post->html, "記事 — 一覧サイト");
}

TEST_CASE("explicit pages and tag keep the suffix when title equals the site title") {
  auto config = listing_config();
  std::vector<kappan::Document> documents = {
      page_document("一覧サイト", "index", "index", "/"),
      post_document("一覧サイト", "same-post", {"一覧サイト"}),
      page_document("一覧サイト", "page", "same-page", "/same-page/"),
      page_document("一覧サイト", "landing", "same-landing", "/same-landing/"),
  };
  const auto site = kappan::site::build(config, std::move(documents), kappan::DraftPolicy::Include);
  auto engine = kappan::render::Engine::load(config);
  REQUIRE(engine);

  for (std::size_t index = 0; index < site.documents.size(); ++index) {
    const auto &document = site.documents[index];
    INFO("layout: " << document.front_matter.layout);
    const auto context = kappan::render::make_context(site, document, nullptr);
    REQUIRE(context["page"]["generated_listing"] == false);
    const auto rendered = engine->render(site, document);
    REQUIRE(rendered);
    require_exact_title_and_og(rendered->html, "一覧サイト — 一覧サイト");
  }

  REQUIRE(site.tags.terms.size() == 1);
  const auto tag_context = kappan::render::make_tag_context(site, site.tags.terms.front());
  REQUIRE(tag_context["page"]["generated_listing"] == false);
  const auto rendered_tag = engine->render_tag(site, site.tags.terms.front().slug);
  REQUIRE(rendered_tag);
  require_exact_title_and_og(rendered_tag->html, "一覧サイト — 一覧サイト");
}
