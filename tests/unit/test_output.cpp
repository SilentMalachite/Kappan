#include "output/assets.hpp"
#include "output/write.hpp"
#include "output/xml.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

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

TEST_CASE("prepare_out_dir wipes previous output when out is inside source") {
  const auto source = std::filesystem::temp_directory_path() / "kappan-out-child";
  const auto out = source / "out";
  std::filesystem::remove_all(source);
  std::filesystem::create_directories(out);
  {
    std::ofstream stale(out / "stale.html", std::ios::binary);
    stale << "古い\n";
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
