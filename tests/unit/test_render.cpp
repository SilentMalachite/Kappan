#include <kappan/config.hpp>
#include <kappan/error.hpp>

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
  const auto page = engine->render(*document);
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
  const auto page = engine->render(*document);
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
  const auto page = engine->render(*document);
  REQUIRE(page);
  REQUIRE(page->html.find("上書き 見出し") != std::string::npos);
  REQUIRE(page->html.find("<article>") == std::string::npos);
  std::filesystem::remove_all(root);
}
