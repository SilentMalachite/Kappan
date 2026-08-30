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

std::filesystem::path repo_root() {
  return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string read_all(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// 出力先の衝突のうち、大文字小文字の畳み込みで起きるものは
// ファイルシステム依存（APFS / Windows で起き、ext4 では起きない）。
bool case_insensitive_filesystem(const std::filesystem::path &dir) {
  const auto probe = dir / "kappan-case-probe";
  const auto other = dir / "KAPPAN-CASE-PROBE";
  std::error_code ec;
  std::filesystem::remove(probe, ec);
  std::filesystem::remove(other, ec);
  {
    std::ofstream file(probe, std::ios::binary);
    file << "x";
  }
  const bool folded = std::filesystem::exists(other, ec);
  std::filesystem::remove(probe, ec);
  return folded;
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

TEST_CASE("build_site writes Windows-safe slug directories") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-windows-safe-slugs";
  const auto posts = source / "content" / "posts";
  const auto out = source / "out";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(posts);
  {
    std::ofstream file(source / "site.yaml", std::ios::binary);
    file << "title: Windows slug 検証\n";
  }
  {
    std::ofstream file(posts / "first.md", std::ios::binary);
    file << "---\ntitle: Windows 予約名 🐙\nslug: CON\nlayout: post\n---\n本文\n";
  }
  {
    std::ofstream file(posts / "second.md", std::ios::binary);
    file << "---\ntitle: 末尾ドット\nslug: LPT9.\nlayout: post\n---\n本文\n";
  }

  const auto result = kappan::content::build_site(source, out);

  REQUIRE(result.ok());
  REQUIRE(std::filesystem::exists(out / "posts" / "_con" / "index.html"));
  REQUIRE(std::filesystem::exists(out / "posts" / "_lpt9" / "index.html"));
  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("/posts/_con/") != std::string::npos);
  REQUIRE(home.find("/posts/_lpt9/") != std::string::npos);

  std::filesystem::remove_all(source);
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

TEST_CASE("build_site aggregates non-map front matter errors") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-site-non-map-roots";
  const auto content = source / "content";
  const auto out = source / "out";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(content);
  {
    std::ofstream file(source / "site.yaml", std::ios::binary);
    file << "title: ルート型検証\n";
  }
  {
    std::ofstream file(content / "good.md", std::ios::binary);
    file << "---\ntitle: 正常ページ\nslug: 正常\n---\n日本語の本文 🐙\n";
  }
  {
    std::ofstream file(content / "bad-scalar.md", std::ios::binary);
    file << "---\ntitle\n---\n本文\n";
  }
  {
    std::ofstream file(content / "bad-sequence.md", std::ios::binary);
    file << "---\n- title\n- 配列\n---\n本文\n";
  }
  {
    std::ofstream file(content / "bad-null.md", std::ios::binary);
    file << "---\n~\n---\n本文\n";
  }

  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE(result.errors.size() == 3);
  for (const auto &error : result.errors) {
    REQUIRE(error.code == kappan::ErrorCode::FrontMatter);
    REQUIRE(error.where.has_value());
    REQUIRE(error.where->parent_path() == content);
    REQUIRE(error.line.has_value());
    REQUIRE(*error.line == 2);
  }
  REQUIRE(std::filesystem::exists(out / kappan::util::from_utf8("正常") / "index.html"));
  REQUIRE(result.pages_written == 2);
  std::filesystem::remove_all(source);
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

TEST_CASE("build_site inserts permalinks before the base URL query and fragment") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-url-query-fragment";
  const auto posts = source / "content" / "posts";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(posts);
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: URL結合\n"
            "url: \"https://sub.example.com/blog?q=日本語#先頭\"\n";
  }
  {
    std::ofstream home(source / "content" / "index.md", std::ios::binary);
    home << "---\ntitle: ホーム\n---\nホーム本文\n";
  }
  {
    std::ofstream post(posts / kappan::util::from_utf8("2026-01-01-こんにちは.md"),
                       std::ios::binary);
    post << "---\ntitle: こんにちは\ndate: 2026-01-01\n---\n記事本文\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());

  const std::string root_url = "https://sub.example.com/blog/?q=日本語#先頭";
  const std::string post_url = "https://sub.example.com/blog/posts/こんにちは/?q=日本語#先頭";
  const auto sitemap = read_all(out / "sitemap.xml");
  CHECK(sitemap.find("<loc>" + root_url + "</loc>") != std::string::npos);
  CHECK(sitemap.find("<loc>" + post_url + "</loc>") != std::string::npos);

  const auto feed = read_all(out / "feed.xml");
  CHECK(feed.find("<link>" + root_url + "</link>") != std::string::npos);
  CHECK(feed.find("<link>" + post_url + "</link>") != std::string::npos);

  const auto home = read_all(out / "index.html");
  CHECK(home.find("<meta property=\"og:url\" content=\"" + root_url + "\">") != std::string::npos);
  const auto post = read_all(out / "posts" / kappan::util::from_utf8("こんにちは") / "index.html");
  CHECK(post.find("<meta property=\"og:url\" content=\"" + post_url + "\">") != std::string::npos);

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

TEST_CASE("build_site does not let a dot slug escape the output directory") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-dot-slug";
  std::filesystem::remove_all(source);
  const auto content = source / "content";
  std::filesystem::create_directories(content / "posts");
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 脱出テスト\n";
  }
  {
    std::ofstream out(content / "index.md", std::ios::binary);
    out << "---\ntitle: ホーム\n---\nホーム本文\n";
  }
  {
    std::ofstream out(content / "posts" / "evil.md", std::ios::binary);
    out << "---\ntitle: 侵入\nslug: \"..\"\n---\n侵入本文\n";
  }
  {
    std::ofstream out(content / "escape.md", std::ios::binary);
    out << "---\ntitle: 脱出\nslug: \"..\"\n---\n脱出本文\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);

  REQUIRE(result.ok());

  // ホームは自分の内容のまま。記事の本文で置き換わっていない。
  // 記事タイトルが一覧のリンクとして載るのは正しい挙動なので、本文で判定する。
  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("<title>ホーム") != std::string::npos);
  REQUIRE(home.find("ホーム本文") != std::string::npos);
  REQUIRE(home.find("侵入本文") == std::string::npos);

  // ".." は untitled に落ち、--out の外へは何も書かれない。
  REQUIRE(std::filesystem::exists(out / "posts" / "untitled" / "index.html"));
  REQUIRE(std::filesystem::exists(out / "untitled" / "index.html"));
  REQUIRE_FALSE(std::filesystem::exists(source / "index.html"));

  std::filesystem::remove_all(source);
}

TEST_CASE("build_site does not silently overwrite a case-folded output path") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-case-collision";
  std::filesystem::remove_all(source);
  const auto posts = source / "content" / "posts";
  std::filesystem::create_directories(posts);
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 大小文字\n";
  }
  // slugify が小文字化するのは ASCII の A-Z だけ。キリル文字の大文字・小文字は
  // 別の claim キーになるが、APFS / Windows では同じファイルに解決される。
  {
    std::ofstream out(posts / "upper.md", std::ios::binary);
    out << "---\ntitle: 大文字\nslug: \"Аbc\"\ndate: 2026-01-05\n---\n大文字本文\n";
  }
  {
    std::ofstream out(posts / "lower.md", std::ios::binary);
    out << "---\ntitle: 小文字\nslug: \"аbc\"\ndate: 2026-01-06\n---\n小文字本文\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);

  if (case_insensitive_filesystem(std::filesystem::temp_directory_path())) {
    // 片方しか書けない。黙って消えるのではなく報告されること。
    REQUIRE_FALSE(result.ok());
    bool reported = false;
    for (const auto &error : result.errors) {
      if (error.code == kappan::ErrorCode::Path &&
          error.message.find("既に書いています") != std::string::npos) {
        reported = true;
      }
    }
    REQUIRE(reported);
  } else {
    // 大文字小文字を区別するファイルシステムでは衝突しないので、両方書ける。
    REQUIRE(result.ok());
    REQUIRE(std::filesystem::exists(out / "posts" / kappan::util::from_utf8("Аbc") / "index.html"));
    REQUIRE(std::filesystem::exists(out / "posts" / kappan::util::from_utf8("аbc") / "index.html"));
  }

  std::filesystem::remove_all(source);
}

TEST_CASE("build_site reports a case-folded collision between static and HTML") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-static-case-collision";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content");
  std::filesystem::create_directories(source / "static" / "About");
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 畳み込み\n";
  }
  {
    std::ofstream out(source / "content" / "about.md", std::ios::binary);
    out << "---\ntitle: 概要\nslug: about\n---\nHTML 本文\n";
  }
  {
    std::ofstream out(source / "static" / "About" / "index.html", std::ios::binary);
    out << "static がここを上書きしたら負け\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);

  if (case_insensitive_filesystem(std::filesystem::temp_directory_path())) {
    REQUIRE_FALSE(result.ok());
    bool reported = false;
    for (const auto &error : result.errors) {
      if (error.code == kappan::ErrorCode::Path &&
          error.message.find("既に書いています") != std::string::npos) {
        reported = true;
      }
    }
    REQUIRE(reported);
    // HTML が生き残っていること
    REQUIRE(read_all(out / "about" / "index.html").find("HTML 本文") != std::string::npos);
  } else {
    REQUIRE(result.ok());
    REQUIRE(std::filesystem::exists(out / "About" / "index.html"));
  }

  std::filesystem::remove_all(source);
}

TEST_CASE("build_site keeps feed.xml parseable when a post body has a control character") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-control-char";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content" / "posts");
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 制御文字\nurl: https://example.com\n";
  }
  {
    std::ofstream out(source / "content" / "posts" / "ctrl.md", std::ios::binary);
    out << "---\ntitle: 制御文字\nslug: ctrl\ndate: 2026-01-03\n---\n"
        << "前" << '\x0c' << "後\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());

  const auto feed = read_all(out / "feed.xml");
  REQUIRE(feed.find('\x0c') == std::string::npos);
  REQUIRE(feed.find("前後") != std::string::npos);

  std::filesystem::remove_all(source);
}

TEST_CASE("build_site keeps feed.xml and sitemap.xml agreeing when a post fails to render") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-feed-agreement";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(source / "content" / "posts");
  std::filesystem::create_directories(source / "templates");
  {
    std::ofstream out(source / "site.yaml", std::ios::binary);
    out << "title: 一致\nurl: https://example.com\n";
  }
  {
    std::ofstream out(source / "content" / "posts" / "ok.md", std::ios::binary);
    out << "---\ntitle: 書けた\nslug: ok\ndate: 2026-01-01\n---\n本文\n";
  }
  {
    std::ofstream out(source / "content" / "posts" / "broken.md", std::ios::binary);
    out << "---\ntitle: 書けない\nslug: broken\ndate: 2026-01-02\nlayout: broken\n---\n本文\n";
  }
  {
    // パースは通るがレンダリング時に落ちる（未定義変数）
    std::ofstream out(source / "templates" / "broken.html", std::ios::binary);
    out << "<p>{{ definitely_undefined_variable }}</p>\n";
  }

  const auto out = source / "out";
  const auto result = kappan::content::build_site(source, out);
  REQUIRE_FALSE(result.ok());
  REQUIRE_FALSE(std::filesystem::exists(out / "posts" / "broken" / "index.html"));

  const auto sitemap = read_all(out / "sitemap.xml");
  const auto feed = read_all(out / "feed.xml");
  // 書き出せなかった記事は両方から消える。片方だけに残ると購読者へ 404 を配ることになる。
  REQUIRE(sitemap.find("/posts/broken/") == std::string::npos);
  REQUIRE(feed.find("/posts/broken/") == std::string::npos);
  REQUIRE(feed.find("/posts/ok/") != std::string::npos);

  std::filesystem::remove_all(source);
}

TEST_CASE("build_site writes the landing example with OGP") {
  const auto source = repo_root() / "examples" / "landing";
  const auto out = std::filesystem::temp_directory_path() / "kappan-landing-example-out";
  std::filesystem::remove_all(out);

  const auto result = kappan::content::build_site(source, out);
  REQUIRE(result.ok());
  REQUIRE(result.pages_written == 1);
  const auto home = read_all(out / "index.html");
  REQUIRE(home.find("<meta property=\"og:title\"") != std::string::npos);
  REQUIRE(home.find("<meta property=\"og:image\"") != std::string::npos);
  REQUIRE(home.find("日本語LP") != std::string::npos);
  REQUIRE(home.find("🐙") != std::string::npos);
  REQUIRE(std::filesystem::exists(out / "images" / "og.svg"));
  REQUIRE(std::filesystem::exists(out / "css" / "site.css"));
  std::filesystem::remove_all(out);
}
