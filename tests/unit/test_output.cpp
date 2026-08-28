#include "output/write.hpp"
#include "output/xml.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

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
