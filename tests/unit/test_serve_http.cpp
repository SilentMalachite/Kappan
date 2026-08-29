#include "output/write.hpp"
#include "serve/http.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

namespace {

void write_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

[[nodiscard]] std::filesystem::path unique_temp(std::string_view prefix) {
  return std::filesystem::temp_directory_path() /
         std::format("{}-{}", prefix, std::chrono::steady_clock::now().time_since_epoch().count());
}

struct TempRoot {
  std::filesystem::path path;

  TempRoot() : path(unique_temp("kappan-http-resolve")) {
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    write_file(path / "index.html", "<p>home</p>\n");
    write_file(path / "about" / "index.html", "<p>about</p>\n");
    write_file(path / kappan::util::from_utf8("こんにちは") / "index.html", "<p>こんにちは</p>\n");
    write_file(path / kappan::util::from_utf8("🐙.svg"), "<svg/>\n");
    write_file(path / std::filesystem::path{std::string(kappan::output::kOutMarker)},
               "kappan output directory\n");
  }

  ~TempRoot() { std::filesystem::remove_all(path); }

  TempRoot(const TempRoot &) = delete;
  TempRoot &operator=(const TempRoot &) = delete;
};

[[nodiscard]] std::string file_of(const kappan::serve::ResolvedRequest &resolved) {
  return kappan::util::to_generic_utf8(resolved.file);
}

} // namespace

TEST_CASE("resolve_request_path maps pretty URLs and Japanese files", "[serve][http]") {
  const TempRoot root;
  const auto ja = std::string{"%E3%81%93%E3%82%93%E3%81%AB%E3%81%A1%E3%81%AF"};
  const auto emoji = std::string{"%F0%9F%90%99"};

  const auto home = kappan::serve::resolve_request_path(root.path, "/");
  REQUIRE(home);
  REQUIRE(home->kind == kappan::serve::ResolveKind::File);
  REQUIRE(file_of(*home) == "index.html");

  const auto about_dir = kappan::serve::resolve_request_path(root.path, "/about/");
  REQUIRE(about_dir);
  REQUIRE(about_dir->kind == kappan::serve::ResolveKind::File);
  REQUIRE(file_of(*about_dir) == "about/index.html");

  const auto about_redirect = kappan::serve::resolve_request_path(root.path, "/about");
  REQUIRE(about_redirect);
  REQUIRE(about_redirect->kind == kappan::serve::ResolveKind::Redirect);
  REQUIRE(about_redirect->location == "/about/");

  const auto ja_page = kappan::serve::resolve_request_path(root.path, std::format("/{}/", ja));
  REQUIRE(ja_page);
  REQUIRE(ja_page->kind == kappan::serve::ResolveKind::File);
  REQUIRE(file_of(*ja_page) == "こんにちは/index.html");

  const auto ja_redirect = kappan::serve::resolve_request_path(root.path, std::format("/{}", ja));
  REQUIRE(ja_redirect);
  REQUIRE(ja_redirect->kind == kappan::serve::ResolveKind::Redirect);
  REQUIRE(ja_redirect->location == std::format("/{}/", ja));

  const auto emoji_file =
      kappan::serve::resolve_request_path(root.path, std::format("/{}.svg", emoji));
  REQUIRE(emoji_file);
  REQUIRE(emoji_file->kind == kappan::serve::ResolveKind::File);
  REQUIRE(file_of(*emoji_file) == "🐙.svg");

  const auto queried = kappan::serve::resolve_request_path(root.path, "/index.html?v=1");
  REQUIRE(queried);
  REQUIRE(queried->kind == kappan::serve::ResolveKind::File);
  REQUIRE(file_of(*queried) == "index.html");
}

TEST_CASE("resolve_request_path rejects traversal and hides the out marker", "[serve][http]") {
  const TempRoot root;

  const auto encoded_dotdot = kappan::serve::resolve_request_path(root.path, "/%2e%2e/site.yaml");
  REQUIRE_FALSE(encoded_dotdot);
  REQUIRE(encoded_dotdot.error().code == kappan::ErrorCode::Path);

  const auto double_encoded =
      kappan::serve::resolve_request_path(root.path, "/%252e%252e/site.yaml");
  REQUIRE(double_encoded);
  REQUIRE(double_encoded->kind == kappan::serve::ResolveKind::NotFound);

  const auto bad_percent = kappan::serve::resolve_request_path(root.path, "/%ZZ");
  REQUIRE_FALSE(bad_percent);
  REQUIRE(bad_percent.error().code == kappan::ErrorCode::Path);

  std::string with_nul{"/index.html"};
  with_nul.insert(1, 1, '\0');
  const auto raw_nul = kappan::serve::resolve_request_path(root.path, with_nul);
  REQUIRE_FALSE(raw_nul);
  REQUIRE(raw_nul.error().code == kappan::ErrorCode::Path);

  const auto decoded_nul = kappan::serve::resolve_request_path(root.path, "/%00");
  REQUIRE_FALSE(decoded_nul);
  REQUIRE(decoded_nul.error().code == kappan::ErrorCode::Path);

  const auto backslash = kappan::serve::resolve_request_path(root.path, "/%5c");
  REQUIRE_FALSE(backslash);
  REQUIRE(backslash.error().code == kappan::ErrorCode::Path);

  const auto marker = kappan::serve::resolve_request_path(root.path, "/.kappan-out");
  REQUIRE(marker);
  REQUIRE(marker->kind == kappan::serve::ResolveKind::NotFound);

  const auto nested_marker = kappan::serve::resolve_request_path(root.path, "/about/.kappan-out");
  REQUIRE(nested_marker);
  REQUIRE(nested_marker->kind == kappan::serve::ResolveKind::NotFound);

  const auto drive = kappan::serve::resolve_request_path(root.path, "/C:/Windows");
  REQUIRE_FALSE(drive);
  REQUIRE(drive.error().code == kappan::ErrorCode::Path);
}

TEST_CASE("content_type_for maps known extensions and UTF-8 charset", "[serve][http]") {
  using kappan::serve::content_type_for;
  namespace fs = std::filesystem;

  REQUIRE(content_type_for(fs::path{"page.HTML"}) == "text/html; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"site.css"}) == "text/css; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"app.js"}) == "text/javascript; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"data.json"}) == "application/json; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"feed.xml"}) == "application/xml; charset=utf-8");
  REQUIRE(content_type_for(kappan::util::from_utf8("🐙.svg")) == "image/svg+xml; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"note.txt"}) == "text/plain; charset=utf-8");
  REQUIRE(content_type_for(fs::path{"pic.png"}) == "image/png");
  REQUIRE(content_type_for(fs::path{"pic.jpg"}) == "image/jpeg");
  REQUIRE(content_type_for(fs::path{"pic.jpeg"}) == "image/jpeg");
  REQUIRE(content_type_for(fs::path{"pic.gif"}) == "image/gif");
  REQUIRE(content_type_for(fs::path{"pic.webp"}) == "image/webp");
  REQUIRE(content_type_for(fs::path{"favicon.ico"}) == "image/x-icon");
  REQUIRE(content_type_for(fs::path{"app.wasm"}) == "application/wasm");
  REQUIRE(content_type_for(fs::path{"blob.bin"}) == "application/octet-stream");
}
