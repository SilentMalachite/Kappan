#include <kappan/config.hpp>
#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include "content/parse.hpp"
#include "render/engine.hpp"
#include "render/escape.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

kappan::Config load_ja_config() {
  const auto loaded = kappan::config::load(fixtures_dir() / "site-ja" / "site.yaml");
  REQUIRE(loaded);
  return *loaded;
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
