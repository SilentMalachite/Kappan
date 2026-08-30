#include <kappan/config.hpp>
#include <kappan/document.hpp>
#include <kappan/error.hpp>

#include "content/parse.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

kappan::Config test_config(const std::filesystem::path &root) {
  kappan::Config config;
  config.title = "テスト";
  config.source_root = root;
  config.content_dir = root / "content";
  return config;
}

} // namespace

TEST_CASE("parse_document reads Japanese front matter and tags") {
  const auto root = fixtures_dir() / "site-ja";
  const auto source =
      root / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md");
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "こんにちは");
  REQUIRE(result->front_matter.layout == "post");
  REQUIRE(result->front_matter.slug == "こんにちは");
  REQUIRE(result->front_matter.tags.size() == 2);
  REQUIRE(result->front_matter.tags[0] == "日本語");
  REQUIRE(result->permalink == "/posts/" + result->front_matter.slug + "/");
  REQUIRE(result->body_html.find("最初の記事です") != std::string::npos);
  REQUIRE(result->front_matter.date);
  REQUIRE(*result->front_matter.date == std::chrono::sys_days{std::chrono::year{2026} / 1 / 1});
}

TEST_CASE("parse_document defaults when front matter is absent") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-none";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("説明.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "# 見出しだけ\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "説明");
  REQUIRE(result->front_matter.layout == "page");
  REQUIRE(result->permalink == "/説明/");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reports a bad date with a file line") {
  const auto root = fixtures_dir() / "site-bad-fm";
  const auto source = root / "content" / "posts" / "2026-01-01-hello.md";
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line == 3);
  REQUIRE(result.error().message.find("date") != std::string::npos);
  REQUIRE(result.error().message.find("2026-13-01") != std::string::npos);
}

TEST_CASE("parse_document reports broken YAML without throwing") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-broken";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "broken.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: [\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document accepts CRLF front matter") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-crlf";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("改行.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\r\ntitle: 改行\r\n---\r\n本文\r\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.title == "改行");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document maps index.md to the site root") {
  const auto root = fixtures_dir() / "site-ja";
  const auto source = root / "content" / "index.md";
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->permalink == "/");
  REQUIRE(result->front_matter.layout == "index");
  REQUIRE(result->output_path == std::filesystem::path{"index.html"});
}

TEST_CASE("parse_document reports unclosed front matter") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-unclosed";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("閉じなし.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 閉じなし\n本文だけ\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line == 1);
  REQUIRE(result.error().message.find("閉じ") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document uses the filename date when front matter has none") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-filename-date";
  const auto content = root / "content" / "posts";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("2026-02-03-名前.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 名前\n---\nファイル名の日付\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.layout == "post");
  REQUIRE(result->front_matter.slug == "名前");
  REQUIRE(result->front_matter.date);
  REQUIRE(*result->front_matter.date == std::chrono::sys_days{std::chrono::year{2026} / 2 / 3});
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document prefers front matter date over the filename") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-date-override";
  const auto content = root / "content" / "posts";
  std::filesystem::create_directories(content);
  const auto source = content / "2026-01-01-hello.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 上書き\ndate: 2026-12-31\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.date);
  REQUIRE(*result->front_matter.date == std::chrono::sys_days{std::chrono::year{2026} / 12 / 31});
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document slugifies an explicit slug") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-explicit-slug";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "page.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 見出し\nslug: カスタム 🐙\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.slug == "カスタム-🐙");
  REQUIRE(result->permalink == "/カスタム-🐙/");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document falls back to title when the stem is not a slug") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-slug-title";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "---.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: こんにちは 世界\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.slug == "こんにちは-世界");
  REQUIRE(result->permalink == "/こんにちは-世界/");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document keeps a usable untitled stem") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-slug-untitled";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "untitled.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: こんにちは\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.slug == "untitled");
  REQUIRE(result->permalink == "/untitled/");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document keeps draft true") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-draft";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / kappan::util::from_utf8("下書き.md");
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: 下書き\ndraft: true\n---\nまだ公開しない\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.draft);
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reports non-sequence tags") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-tags";
  const auto content = root / "content";
  std::filesystem::create_directories(content);
  const auto source = content / "tags.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\ntitle: タグ\ntags: 日本語\n---\n本文\n";
  }
  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  REQUIRE(result.error().message.find("tags") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reads landing image and sections") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-landing";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: 日本語LP 🐙\n"
           "layout: landing\n"
           "description: LPの説明\n"
           "image: /images/og.svg\n"
           "sections:\n"
           "  - type: hero\n"
           "    eyebrow: 活版\n"
           "    title: 速い静的サイト\n"
           "    text: 日本語もそのまま扱います。\n"
           "    image: /images/hero.svg\n"
           "    actions:\n"
           "      - label: 詳しく見る\n"
           "        href: '#features'\n"
           "  - type: features\n"
           "    items:\n"
           "      - title: UTF-8\n"
           "        text: かな・漢字・絵文字を保持\n"
           "        icon: sparkle\n"
           "---\n"
           "本文\n";
  }

  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE(result);
  REQUIRE(result->front_matter.layout == "landing");
  REQUIRE(result->front_matter.image == "/images/og.svg");
  REQUIRE(result->front_matter.sections.size() == 2);
  REQUIRE(result->front_matter.sections[0].type == "hero");
  REQUIRE(result->front_matter.sections[0].actions.size() == 1);
  REQUIRE(result->front_matter.sections[0].actions[0].label == "詳しく見る");
  REQUIRE(result->front_matter.sections[1].items.size() == 1);
  REQUIRE(result->front_matter.sections[1].items[0].text == "かな・漢字・絵文字を保持");
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reports invalid landing sections with a file line") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-fm-landing-bad";
  const auto content = root / "content";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(content);
  const auto source = content / "index.md";
  {
    std::ofstream out(source, std::ios::binary);
    out << "---\n"
           "title: 壊れたLP\n"
           "layout: landing\n"
           "sections:\n"
           "  - type: hero\n"
           "    actions: 開く\n"
           "---\n"
           "本文\n";
  }

  const auto result = kappan::content::parse_document(source, test_config(root));
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line == 6);
  REQUIRE(result.error().message.find("sections.actions") != std::string::npos);
  std::filesystem::remove_all(root);
}

TEST_CASE("parse_document reports every invalid landing section shape") {
  struct Case {
    const char *name;   // 一時ディレクトリ名
    const char *yaml;   // front matter の 2 行目以降（1 行目は '---'）
    int line;           // 期待するファイル行番号
    const char *key;    // メッセージに出るキー
    const char *reason; // メッセージに出る理由
  };
  const std::array<Case, 10> cases{{
      {"seq", "title: LP\nlayout: landing\nsections: ヒーロー\n", 4, "'sections'",
       "マップの配列である必要があります"},
      {"elem", "title: LP\nlayout: landing\nsections:\n  - ヒーロー\n", 5, "'sections'",
       "要素はマップである必要があります"},
      {"field", "title: LP\nlayout: landing\nsections:\n  - type: hero\n    title: [a]\n", 6,
       "'sections.title'", "文字列である必要があります"},
      {"actions-seq", "title: LP\nlayout: landing\nsections:\n  - type: hero\n    actions: 開く\n",
       6, "'sections.actions'", "マップの配列である必要があります"},
      {"actions-elem",
       "title: LP\nlayout: landing\nsections:\n  - type: hero\n    actions:\n      - 開く\n", 7,
       "'sections.actions'", "要素はマップである必要があります"},
      {"actions-label",
       "title: LP\nlayout: landing\nsections:\n  - type: hero\n    actions:\n      - label: [a]\n",
       7, "'sections.actions.label'", "文字列である必要があります"},
      {"actions-href",
       "title: LP\nlayout: landing\nsections:\n  - type: hero\n    actions:\n      - label: 開く\n"
       "        href: [a]\n",
       8, "'sections.actions.href'", "文字列である必要があります"},
      {"items-seq", "title: LP\nlayout: landing\nsections:\n  - type: features\n    items: 項目\n",
       6, "'sections.items'", "マップの配列である必要があります"},
      {"items-elem",
       "title: LP\nlayout: landing\nsections:\n  - type: features\n    items:\n      - 項目\n", 7,
       "'sections.items'", "要素はマップである必要があります"},
      {"items-icon",
       "title: LP\nlayout: landing\nsections:\n  - type: features\n    items:\n      - icon: [a]\n",
       7, "'sections.items.icon'", "文字列である必要があります"},
  }};

  for (const auto &item : cases) {
    CAPTURE(item.name);
    const auto root =
        std::filesystem::temp_directory_path() / (std::string{"kappan-fm-sections-"} + item.name);
    const auto content = root / "content";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(content);
    const auto source = content / "index.md";
    {
      std::ofstream out(source, std::ios::binary);
      out << "---\n" << item.yaml << "---\n本文\n";
    }

    const auto result = kappan::content::parse_document(source, test_config(root));
    REQUIRE_FALSE(result);
    REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
    REQUIRE(result.error().where == source);
    REQUIRE(result.error().line.has_value());
    REQUIRE(*result.error().line == item.line);
    REQUIRE(result.error().message.find(item.key) != std::string::npos);
    REQUIRE(result.error().message.find(item.reason) != std::string::npos);
    std::filesystem::remove_all(root);
  }
}
