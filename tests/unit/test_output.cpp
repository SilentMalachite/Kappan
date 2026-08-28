#include "output/assets.hpp"
#include "output/write.hpp"
#include "output/xml.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include <catch2/catch_test_macros.hpp>

#include <set>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>

TEST_CASE("xml_escape converts XML special characters") {
  REQUIRE(kappan::output::xml_escape("A&B <c> \"'\"") == "A&amp;B &lt;c&gt; &quot;&apos;&quot;");
  REQUIRE(kappan::output::xml_escape("日本語 🐙") == "日本語 🐙");
}

TEST_CASE("join_url strips trailing slashes and keeps Japanese permalinks") {
  REQUIRE(kappan::output::join_url("https://example.com/", "/") == "https://example.com/");
  REQUIRE(kappan::output::join_url("https://example.com", "/posts/こんにちは/") ==
          "https://example.com/posts/こんにちは/");
  REQUIRE(kappan::output::join_url("https://example.com/blog/", "/about/") ==
          "https://example.com/blog/about/");
}

TEST_CASE("render_sitemap sorts permalinks and keeps Japanese loc") {
  using kappan::output::SitemapUrl;
  const auto day = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  const auto xml = kappan::output::render_sitemap(
      "https://example.com/", {{"/posts/こんにちは/", day}, {"/", std::nullopt}});
  REQUIRE_FALSE(xml.starts_with("\xEF\xBB\xBF"));
  REQUIRE(xml.find("xmlns=\"http://www.sitemaps.org/schemas/sitemap/0.9\"") != std::string::npos);
  const auto home = xml.find("<loc>https://example.com/</loc>");
  const auto post = xml.find("<loc>https://example.com/posts/こんにちは/</loc>");
  REQUIRE(home != std::string::npos);
  REQUIRE(post != std::string::npos);
  REQUIRE(home < post);
  REQUIRE(xml.find("<lastmod>2026-01-01</lastmod>") != std::string::npos);
  const auto home_url_end = xml.find("</url>", home);
  REQUIRE(xml.find("<lastmod>", home) > home_url_end);
}

TEST_CASE("prepare_out_dir rejects out equal to source without deleting it") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-out-same";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream keep(dir / "keep.txt", std::ios::binary);
    keep << "残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(dir, dir);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Cli);
  REQUIRE(result.error().message.find("同じ") != std::string::npos);
  REQUIRE(std::filesystem::exists(dir / "keep.txt"));
  std::filesystem::remove_all(dir);
}

TEST_CASE("prepare_out_dir rejects source inside out without deleting source") {
  const auto out = std::filesystem::temp_directory_path() / "kappan-out-parent";
  const auto source = out / "site";
  std::filesystem::remove_all(out);
  std::filesystem::create_directories(source);
  {
    std::ofstream keep(source / "site.yaml", std::ios::binary);
    keep << "title: 残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(source, out);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Cli);
  REQUIRE(result.error().message.find("消す") != std::string::npos);
  REQUIRE(std::filesystem::exists(source / "site.yaml"));
  std::filesystem::remove_all(out);
}

TEST_CASE("prepare_out_dir wipes a previous kappan output when out is inside source") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-out-child";
  const auto out = source / "out";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(out);
  {
    std::ofstream stale(out / "stale.html", std::ios::binary);
    stale << "古い\n";
  }
  {
    // 前回の kappan ビルドが残した印。これが無い非空ディレクトリは消さない（ADR-0007）。
    std::ofstream marker(out / std::filesystem::path{kappan::output::kOutMarker}, std::ios::binary);
    marker << "kappan output directory\n";
  }
  {
    std::ofstream yaml(source / "site.yaml", std::ios::binary);
    yaml << "title: 残す\n";
  }
  const auto result = kappan::output::prepare_out_dir(source, out);
  REQUIRE(result);
  REQUIRE(std::filesystem::is_directory(out));
  REQUIRE_FALSE(std::filesystem::exists(out / "stale.html"));
  REQUIRE(std::filesystem::exists(source / "site.yaml"));
  std::filesystem::remove_all(source);
}

TEST_CASE("claim_output reports a Japanese path collision") {
  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> errors;
  const auto first = std::filesystem::path{"content"} / "a.md";
  const auto second =
      std::filesystem::path{"static"} / kappan::util::from_utf8("画像") / "index.html";
  REQUIRE(kappan::output::claim_output(claimed, "about/index.html", first, errors));
  REQUIRE_FALSE(kappan::output::claim_output(claimed, "about/index.html", second, errors));
  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE(errors.front().message.find("about/index.html") != std::string::npos);
}

TEST_CASE("copy_static copies Japanese filenames and raw bytes") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-copy";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir / "images");
  std::filesystem::create_directories(static_dir / kappan::util::from_utf8("_隠し"));
  std::filesystem::create_directories(out);
  {
    std::ofstream file(static_dir / "images" / kappan::util::from_utf8("🐙.svg"), std::ios::binary);
    file << "<svg xmlns='http://www.w3.org/2000/svg'/>";
  }
  {
    std::ofstream bin(static_dir / "blob.bin", std::ios::binary);
    const char bytes[] = {'\0', '\x01', '\xFF', '\xFE'};
    bin.write(bytes, 4);
  }
  {
    std::ofstream hidden(static_dir / kappan::util::from_utf8("_隠し") / "nope.css",
                         std::ios::binary);
    hidden << "body{}\n";
  }
  kappan::output::ClaimedOutputs claimed;
  const auto errors = kappan::output::copy_static(static_dir, out, claimed);
  REQUIRE(errors.empty());
  const auto copied = out / "images" / kappan::util::from_utf8("🐙.svg");
  REQUIRE(std::filesystem::exists(copied));
  std::ifstream in(copied, std::ios::binary);
  const std::string got{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  REQUIRE(got.find("svg") != std::string::npos);
  std::ifstream bin_in(out / "blob.bin", std::ios::binary);
  const std::string raw{std::istreambuf_iterator<char>(bin_in), std::istreambuf_iterator<char>()};
  REQUIRE(raw.size() == 4);
  REQUIRE(static_cast<unsigned char>(raw[2]) == 0xFF);
  REQUIRE_FALSE(std::filesystem::exists(out / kappan::util::from_utf8("_隠し") / "nope.css"));
  std::filesystem::remove_all(root);
}

TEST_CASE("copy_static ignores a missing directory") {
  kappan::output::ClaimedOutputs claimed;
  const auto errors =
      kappan::output::copy_static(std::filesystem::temp_directory_path() / "kappan-no-static-dir",
                                  std::filesystem::temp_directory_path(), claimed);
  REQUIRE(errors.empty());
}

TEST_CASE("copy_static skips files that collide with claimed outputs") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-collide";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir);
  std::filesystem::create_directories(out);
  {
    std::ofstream file(static_dir / "index.html", std::ios::binary);
    file << "static\n";
  }
  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> claim_errors;
  REQUIRE(kappan::output::claim_output(claimed, "index.html", root / "content" / "index.md",
                                       claim_errors));
  const auto errors = kappan::output::copy_static(static_dir, out, claimed);
  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE_FALSE(std::filesystem::exists(out / "index.html"));
  std::filesystem::remove_all(root);
}

TEST_CASE("render_feed lists posts only with RFC 822 dates") {
  kappan::Config config;
  config.title = "活版ブログ";
  config.url = "https://example.com";
  config.language = "ja";
  config.description = "日本語と絵文字 🐙 を含むサイト";

  kappan::Document post;
  post.permalink = "/posts/こんにちは/";
  post.front_matter.title = "こんにちは";
  post.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  post.body_html = "<p>最初の記事です。</p>\n";

  kappan::Document page;
  page.permalink = "/about/";
  page.front_matter.title = "概要";
  page.body_html = "<p>概要</p>\n";

  kappan::Document draft;
  draft.permalink = "/posts/下書き/";
  draft.front_matter.title = "下書き";
  draft.front_matter.draft = true;
  draft.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 2};

  auto site = kappan::site::build(config, {post, page, draft}, kappan::DraftPolicy::Exclude);
  const std::set<std::string> written{"/posts/こんにちは/", "/about/"};
  const auto xml = kappan::output::render_feed(site, written);
  REQUIRE(xml.find("<rss version=\"2.0\">") != std::string::npos);
  REQUIRE(xml.find("<link>https://example.com/</link>") != std::string::npos);
  REQUIRE(xml.find("日本語と絵文字 🐙 を含むサイト") != std::string::npos);
  REQUIRE(xml.find("<title>こんにちは</title>") != std::string::npos);
  REQUIRE(xml.find("<guid isPermaLink=\"true\">https://example.com/posts/こんにちは/</guid>") !=
          std::string::npos);
  REQUIRE(xml.find("Thu, 01 Jan 2026 00:00:00 +0000") != std::string::npos);
  REQUIRE(xml.find("&lt;p&gt;最初の記事です。&lt;/p&gt;") != std::string::npos);
  REQUIRE(xml.find("概要") == std::string::npos);
  REQUIRE(xml.find("下書き") == std::string::npos);
}

TEST_CASE("render_feed uses title when site description is empty") {
  kappan::Config config;
  config.title = "タイトルだけ";
  config.url = "https://example.com";
  auto site = kappan::site::build(config, {}, kappan::DraftPolicy::Exclude);
  const auto xml = kappan::output::render_feed(site, {});
  REQUIRE(xml.find("<description>タイトルだけ</description>") != std::string::npos);
  REQUIRE(xml.find("<item>") == std::string::npos);
}

TEST_CASE("claim_output rejects output paths that escape the output directory") {
  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> errors;
  const auto source = std::filesystem::path{"content"} / "evil.md";

  REQUIRE_FALSE(kappan::output::claim_output(claimed, "posts/../index.html", source, errors));
  REQUIRE_FALSE(kappan::output::claim_output(claimed, "../index.html", source, errors));
  REQUIRE_FALSE(kappan::output::claim_output(claimed, "/etc/passwd", source, errors));
  REQUIRE(errors.size() == 3);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE(errors.front().message.find("--out の外") != std::string::npos);
  REQUIRE(claimed.empty());

  REQUIRE(kappan::output::claim_output(claimed, "posts/..記事/index.html", source, errors));
  REQUIRE(errors.size() == 3);
}

TEST_CASE("claim_destination rejects a destination that already exists") {
  const auto dir = std::filesystem::temp_directory_path() / "kappan-claim-destination";
  std::filesystem::remove_all(dir);
  std::filesystem::create_directories(dir);
  {
    std::ofstream taken(dir / "a.html", std::ios::binary);
    taken << "先客\n";
  }
  std::vector<kappan::Error> errors;
  const auto source = std::filesystem::path{"content"} / "b.md";

  REQUIRE_FALSE(kappan::output::claim_destination(dir / "a.html", source, errors));
  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Path);
  REQUIRE(errors.front().message.find("既に書いています") != std::string::npos);

  REQUIRE(kappan::output::claim_destination(dir / "b.html", source, errors));
  REQUIRE(errors.size() == 1);

  std::filesystem::remove_all(dir);
}

#ifndef _WIN32
TEST_CASE("copy_static reports a scan error instead of stopping silently") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-scan-error";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir / "locked");
  std::filesystem::create_directories(out);
  {
    std::ofstream inner(static_dir / "locked" / "a.txt", std::ios::binary);
    inner << "a\n";
  }
  {
    std::ofstream top(static_dir / "top.txt", std::ios::binary);
    top << "t\n";
  }
  std::filesystem::permissions(static_dir / "locked", std::filesystem::perms::none);

  // root は権限を無視するので、このテストの前提（走査が失敗すること）が成立しない。
  std::error_code probe_ec;
  const std::filesystem::directory_iterator probe(static_dir / "locked", probe_ec);
  if (!probe_ec) {
    std::filesystem::permissions(static_dir / "locked", std::filesystem::perms::owner_all);
    std::filesystem::remove_all(root);
    SUCCEED("権限が効かない環境ではスキップする");
    return;
  }

  kappan::output::ClaimedOutputs claimed;
  const auto errors = kappan::output::copy_static(static_dir, out, claimed,
                                                  std::filesystem::directory_options::none);

  std::filesystem::permissions(static_dir / "locked", std::filesystem::perms::owner_all);

  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Io);
  REQUIRE(errors.front().message.find("走査できません") != std::string::npos);

  std::filesystem::remove_all(root);
}
#endif

#ifndef _WIN32
TEST_CASE("copy_static reports an unresolvable symlink without throwing") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-symlink";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir);
  std::filesystem::create_directories(out);
  {
    std::ofstream top(static_dir / "top.txt", std::ios::binary);
    top << "t\n";
  }
  // 自分を指すループ。stat が ELOOP で失敗し、種別を判定できない。
  std::filesystem::create_symlink("loop", static_dir / "loop");
  // 行き先なし。stat は not_found を返すだけなのでエラーではない（従来どおり黙って飛ばす）。
  std::filesystem::create_symlink("nowhere", static_dir / "dangling");

  kappan::output::ClaimedOutputs claimed;
  std::vector<kappan::Error> errors;
  REQUIRE_NOTHROW(errors = kappan::output::copy_static(static_dir, out, claimed));

  REQUIRE(errors.size() == 1);
  REQUIRE(errors.front().code == kappan::ErrorCode::Io);
  REQUIRE(errors.front().message.find("種別を判定できません") != std::string::npos);
  // 1 件の失敗でビルド全体を止めない（AGENTS.md §6）
  REQUIRE(std::filesystem::exists(out / "top.txt"));

  std::filesystem::remove_all(root);
}
#endif

TEST_CASE("xml_escape drops characters that XML 1.0 forbids") {
  using kappan::output::xml_escape;
  // 文字参照にしても不正なので、置換ではなく除去する
  REQUIRE(xml_escape("前"
                     "\x0c"
                     "後") == "前後");
  REQUIRE(xml_escape("\x01"
                     "\x1f") == "");
  REQUIRE(xml_escape(std::string("a\0b", 3)) == "ab");
  // タブ・LF・CR は許される
  REQUIRE(xml_escape("a\tb\nc\rd") == "a\tb\nc\rd");
  // 0x7F は XML 1.0 では正当な文字なので落とさない
  REQUIRE(xml_escape("a"
                     "\x7f"
                     "b") == "a"
                             "\x7f"
                             "b");
  // 日本語・絵文字のバイトを壊さない
  REQUIRE(xml_escape("日本語 🐙") == "日本語 🐙");
}

TEST_CASE("source_inside_out treats an empty relative path as unrelated") {
  using kappan::output::source_inside_out;
  // Windows のドライブ跨ぎ（--source C:\site --out D:\out）では root_name が違うため
  // lexically_relative が空を返す。無関係であって「同じ」でも「内側」でもない。
  REQUIRE_FALSE(source_inside_out(std::filesystem::path{}));
  // out の内側 = --out がソースを消す位置
  REQUIRE(source_inside_out(std::filesystem::path{"site"}));
  REQUIRE(source_inside_out(std::filesystem::path{"a"} / "b"));
  // out の外
  REQUIRE_FALSE(source_inside_out(std::filesystem::path{".."} / "site"));
  REQUIRE_FALSE(source_inside_out(std::filesystem::path{"/tmp"} / "site"));
}

TEST_CASE("render_feed omits posts that were never written") {
  kappan::Config config;
  config.title = "書き出し済みだけ";
  config.url = "https://example.com";

  kappan::Document ok;
  ok.permalink = "/posts/書けた/";
  ok.front_matter.title = "書けた";
  ok.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};

  kappan::Document broken;
  broken.permalink = "/posts/書けなかった/";
  broken.front_matter.title = "書けなかった";
  broken.front_matter.date = std::chrono::sys_days{std::chrono::year{2026} / 1 / 2};

  auto site = kappan::site::build(config, {ok, broken}, kappan::DraftPolicy::Exclude);
  const std::set<std::string> written{"/posts/書けた/"};
  const auto xml = kappan::output::render_feed(site, written);

  REQUIRE(xml.find("書けた") != std::string::npos);
  REQUIRE(xml.find("書けなかった") == std::string::npos);
}

TEST_CASE("render_sitemap gives lastmod a timezone designator") {
  using kappan::output::SitemapUrl;
  const auto midnight = std::chrono::sys_days{std::chrono::year{2026} / 1 / 1};
  const auto timed = midnight + std::chrono::hours{9} + std::chrono::minutes{30};
  const auto xml =
      kappan::output::render_sitemap("https://example.com", {{"/a/", midnight}, {"/b/", timed}});
  // 日付のみは complete date 形式。TZD を付けない。
  REQUIRE(xml.find("<lastmod>2026-01-01</lastmod>") != std::string::npos);
  // 日時は W3C Datetime として TZD が必須。値は UTC なので Z。
  REQUIRE(xml.find("<lastmod>2026-01-01T09:30:00Z</lastmod>") != std::string::npos);
  REQUIRE(xml.find("<lastmod>2026-01-01T09:30:00<") == std::string::npos);
}

TEST_CASE("copy_static skips dot files and dot directories") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-static-dotfiles";
  std::filesystem::remove_all(root);
  const auto static_dir = root / "static";
  const auto out = root / "out";
  std::filesystem::create_directories(static_dir / "css");
  std::filesystem::create_directories(static_dir / ".git");
  std::filesystem::create_directories(out);
  for (const auto &rel : {"top.txt", ".DS_Store", "css/site.css", "css/.hidden", ".git/config"}) {
    std::ofstream file(static_dir / rel, std::ios::binary);
    file << "x\n";
  }

  kappan::output::ClaimedOutputs claimed;
  const auto errors = kappan::output::copy_static(static_dir, out, claimed);
  REQUIRE(errors.empty());

  REQUIRE(std::filesystem::exists(out / "top.txt"));
  REQUIRE(std::filesystem::exists(out / "css" / "site.css"));
  REQUIRE_FALSE(std::filesystem::exists(out / ".DS_Store"));
  REQUIRE_FALSE(std::filesystem::exists(out / "css" / ".hidden"));
  REQUIRE_FALSE(std::filesystem::exists(out / ".git"));

  std::filesystem::remove_all(root);
}

TEST_CASE("prepare_out_dir refuses a non-empty directory that kappan did not write") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-out-guard";
  std::filesystem::remove_all(root);
  const auto source = root / "site";
  const auto out = root / "precious";
  std::filesystem::create_directories(source);
  std::filesystem::create_directories(out / "photos");
  {
    std::ofstream keep(out / "photos" / "wedding.jpg", std::ios::binary);
    keep << "かけがえのない\n";
  }

  const auto refused = kappan::output::prepare_out_dir(source, out);
  REQUIRE_FALSE(refused);
  REQUIRE(refused.error().code == kappan::ErrorCode::Cli);
  REQUIRE(refused.error().message.find("--force") != std::string::npos);
  // 何も消さないこと。これがこの ADR の目的。
  REQUIRE(std::filesystem::exists(out / "photos" / "wedding.jpg"));

  // --force なら消す
  const auto forced =
      kappan::output::prepare_out_dir(source, out, kappan::output::OutDirPolicy::Force);
  REQUIRE(forced);
  REQUIRE_FALSE(std::filesystem::exists(out / "photos" / "wedding.jpg"));

  std::filesystem::remove_all(root);
}

TEST_CASE("prepare_out_dir accepts an empty directory and leaves its marker") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-out-marker";
  std::filesystem::remove_all(root);
  const auto source = root / "site";
  const auto out = root / "out";
  std::filesystem::create_directories(source);
  std::filesystem::create_directories(out);

  REQUIRE(kappan::output::prepare_out_dir(source, out));
  const auto marker = out / std::filesystem::path{kappan::output::kOutMarker};
  REQUIRE(std::filesystem::exists(marker));

  // 印があるので 2 回目以降は素通りする（毎回 --force が要らない）
  {
    std::ofstream page(out / "index.html", std::ios::binary);
    page << "前回の出力\n";
  }
  REQUIRE(kappan::output::prepare_out_dir(source, out));
  REQUIRE_FALSE(std::filesystem::exists(out / "index.html"));
  REQUIRE(std::filesystem::exists(marker));

  std::filesystem::remove_all(root);
}
