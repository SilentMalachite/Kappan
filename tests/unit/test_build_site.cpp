#include <kappan/error.hpp>

#include "content/build.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <format>
#include <fstream>
#include <string>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

std::string read_all(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

TEST_CASE("build_site writes pretty URLs for a Japanese blog") {
  const auto source = fixtures_dir() / "site-ja";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-ja-out";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 5);

  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("ホーム 🐙") != std::string::npos);
  REQUIRE(home.find("<html lang=\"ja\">") != std::string::npos);
  REQUIRE(home.find("<article>") != std::string::npos);
  REQUIRE_FALSE(home.starts_with("\xEF\xBB\xBF"));

  const auto about = read_all(out / "about" / "index.html");
  REQUIRE(about.find("概要の本文です") != std::string::npos);

  const auto post = read_all(out / kappan::util::from_utf8("posts") /
                             kappan::util::from_utf8("こんにちは") / "index.html");
  REQUIRE(post.find("最初の記事です") != std::string::npos);

  std::filesystem::remove_all(out);
}

TEST_CASE("build_site keeps good pages when one front matter is broken") {
  const auto source = fixtures_dir() / "site-bad-fm";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-partial-out";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.pages_written == 2);
  REQUIRE(result.errors.size() >= 1);
  REQUIRE(result.errors.front().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(std::filesystem::exists(out / "ok" / "index.html"));
  REQUIRE_FALSE(std::filesystem::exists(out / "posts" / "hello" / "index.html"));

  std::filesystem::remove_all(out);
}

TEST_CASE("build_site rejects a Markdown file as --source") {
  const auto file = fixtures_dir() / "ja_emoji.md";
  const auto out = std::filesystem::temp_directory_path() / "kappan-site-file-out";
  const auto result = kappan::content::build_site(file, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.size() == 1);
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Cli);
  REQUIRE(result.errors.front().message.find("サイトの根ディレクトリ") != std::string::npos);
}

TEST_CASE("build_site reports a missing site.yaml") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-empty-site";
  std::filesystem::create_directories(source);
  const auto result = kappan::content::build_site(source, source / "out");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Config);
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site reports a missing content directory") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-no-content";
  std::filesystem::create_directories(source);
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: コンテンツ無し\n";
  }
  const auto result = kappan::content::build_site(source, source / "out");
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Config);
  REQUIRE(result.errors.front().message.find("content") != std::string::npos);
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site skips underscore directories") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-skip-underscore";
  const auto content = source / "content";
  std::filesystem::create_directories(content / kappan::util::from_utf8("_下書き"));
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 下書き除外\n";
  }
  {
    std::ofstream out(content / kappan::util::from_utf8("見える.md"), std::ios::binary);
    out << "---\ntitle: 見える\n---\n公開する\n";
  }
  {
    std::ofstream out(content / kappan::util::from_utf8("_下書き") / "secret.md", std::ios::binary);
    out << "---\ntitle: 秘密\n---\n出てはいけない\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 2);
  REQUIRE(std::filesystem::exists(out / kappan::util::from_utf8("見える") / "index.html"));
  REQUIRE(std::filesystem::exists(out / "index.html"));
  REQUIRE_FALSE(std::filesystem::exists(out / kappan::util::from_utf8("秘密") / "index.html"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site aggregates permalink collisions") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-permalink-collision";
  const auto content = source / "content";
  std::filesystem::create_directories(content);
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 衝突\n";
  }
  {
    std::ofstream out(content / "a.md", std::ios::binary);
    out << "---\nslug: 同じ\n---\nA\n";
  }
  {
    std::ofstream out(content / "b.md", std::ios::binary);
    out << "---\nslug: 同じ\n---\nB\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.pages_written == 2);
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Path);
  REQUIRE(result.errors.front().message.find("permalink") != std::string::npos);
  REQUIRE(std::filesystem::exists(out / kappan::util::from_utf8("同じ") / "index.html"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site excludes drafts unless DraftPolicy::Include") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-draft-out";
  const auto content = source / "content";
  std::filesystem::create_directories(content);
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 下書き除外\n";
  }
  {
    std::ofstream out(content / kappan::util::from_utf8("下書き.md"), std::ios::binary);
    out << "---\ntitle: 下書き\ndraft: true\n---\n出てはいけない\n";
  }
  const auto out = source / "out";
  const auto hidden = kappan::content::build_site(source, out, kappan::DraftPolicy::Exclude);
  REQUIRE(hidden.ok());
  REQUIRE_FALSE(std::filesystem::exists(out / kappan::util::from_utf8("下書き") / "index.html"));

  const auto shown = kappan::content::build_site(source, out, kappan::DraftPolicy::Include);
  REQUIRE(shown.ok());
  REQUIRE(std::filesystem::exists(out / kappan::util::from_utf8("下書き") / "index.html"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site paginates eleven Japanese posts") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-eleven-posts";
  const auto posts = source / "content" / "posts";
  std::filesystem::create_directories(posts);
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 十一件\npagination:\n  posts_per_page: 10\n";
  }
  for (int i = 1; i <= 11; ++i) {
    const auto name = std::format("2026-01-{:02}-記事{:02}.md", i, i);
    std::ofstream out(posts / kappan::util::from_utf8(name), std::ios::binary);
    out << std::format("---\ntitle: 記事{:02}\ndate: 2026-01-{:02}\n---\n本文\n", i, i);
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(std::filesystem::exists(out / "index.html"));
  REQUIRE(std::filesystem::exists(out / "page" / "2" / "index.html"));
  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("記事11") != std::string::npos);
  REQUIRE(home.find("次へ") != std::string::npos);
  const auto page2 = read_all(out / "page" / "2" / "index.html");
  REQUIRE(page2.find("記事01") != std::string::npos);
  REQUIRE(page2.find("前へ") != std::string::npos);
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site does not wipe out on config error") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-keep-out";
  const auto out = std::filesystem::temp_directory_path() / "kappan-keep-out-dest";
  std::filesystem::remove_all(source);
  std::filesystem::remove_all(out);
  std::filesystem::create_directories(source);
  std::filesystem::create_directories(out);
  {
    std::ofstream keep(out / "keep.txt", std::ios::binary);
    keep << "残す\n";
  }
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Config);
  REQUIRE(std::filesystem::exists(out / "keep.txt"));
  std::filesystem::remove_all(source);
  std::filesystem::remove_all(out);
}

TEST_CASE("build_site copies static files and writes sitemap and feed") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-phase5-site";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content" / "posts");
  std::filesystem::create_directories(source / "static" / "images");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 活版\nurl: https://example.com\nlanguage: ja\ndescription: 説明 🐙\n";
  }
  {
    std::ofstream index(source / "content" / "index.md", std::ios::binary);
    index << "---\ntitle: ホーム\n---\n# ホーム\n";
  }
  {
    std::ofstream post(source / "content" / "posts" /
                           kappan::util::from_utf8("2026-01-01-こんにちは.md"),
                       std::ios::binary);
    post << "---\ntitle: こんにちは\ndate: 2026-01-01\n---\n本文です。\n";
  }
  {
    std::ofstream draft(source / "content" / "posts" /
                            kappan::util::from_utf8("2026-01-02-下書き.md"),
                        std::ios::binary);
    draft << "---\ntitle: 下書き\ndate: 2026-01-02\ndraft: true\n---\n秘密\n";
  }
  {
    std::ofstream svg(source / "static" / "images" / kappan::util::from_utf8("🐙.svg"),
                      std::ios::binary);
    svg << "<svg xmlns='http://www.w3.org/2000/svg'/>";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(std::filesystem::exists(out / "images" / kappan::util::from_utf8("🐙.svg")));
  const auto sitemap = read_all(out / "sitemap.xml");
  REQUIRE(sitemap.find("https://example.com/posts/こんにちは/") != std::string::npos);
  REQUIRE(sitemap.find("下書き") == std::string::npos);
  const auto feed = read_all(out / "feed.xml");
  REQUIRE(feed.find("<rss version=\"2.0\">") != std::string::npos);
  REQUIRE(feed.find("こんにちは") != std::string::npos);
  REQUIRE(feed.find("下書き") == std::string::npos);
  REQUIRE_FALSE(sitemap.starts_with("\xEF\xBB\xBF"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site omits sitemap and feed when url is empty") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-no-url";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: URL無し\n";
  }
  {
    std::ofstream index(source / "content" / "index.md", std::ios::binary);
    index << "---\ntitle: ホーム\n---\n# ホーム\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE_FALSE(std::filesystem::exists(out / "sitemap.xml"));
  REQUIRE_FALSE(std::filesystem::exists(out / "feed.xml"));
  std::filesystem::remove_all(source);
}

TEST_CASE("build_site reports static collision with generated HTML") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-static-html-collide";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content");
  std::filesystem::create_directories(source / "static" / "about");
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 衝突\n";
  }
  {
    std::ofstream about(source / "content" / "about.md", std::ios::binary);
    about << "---\ntitle: 概要\n---\n本文\n";
  }
  {
    std::ofstream stale(source / "static" / "about" / "index.html", std::ios::binary);
    stale << "static\n";
  }
  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.front().code == kappan::ErrorCode::Path);
  const auto html = read_all(out / "about" / "index.html");
  REQUIRE(html.find("本文") != std::string::npos);
  std::filesystem::remove_all(source);
}
