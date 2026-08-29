#include "output/write.hpp"
#include "serve/http.hpp"
#include "serve/publish.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

void write_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

std::atomic<int> g_temp_seq{0};

[[nodiscard]] std::filesystem::path unique_temp(std::string_view prefix) {
  return std::filesystem::temp_directory_path() /
         std::format("{}-{}-{}", prefix,
                     std::chrono::steady_clock::now().time_since_epoch().count(),
                     g_temp_seq.fetch_add(1));
}

[[nodiscard]] std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

void write_bytes(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

constexpr std::byte kWasmBytes[] = {
    std::byte{0x00}, std::byte{0x61}, std::byte{0x73}, std::byte{0x6d},
    std::byte{0xff}, std::byte{0x00}, std::byte{0xff},
};

[[nodiscard]] std::vector<std::byte> wasm_payload() {
  return {std::begin(kWasmBytes), std::end(kWasmBytes)};
}

[[nodiscard]] std::filesystem::path make_japanese_site() {
  const auto dest = unique_temp("kappan-http-src");
  std::filesystem::remove_all(dest);
  std::filesystem::copy(fixtures_dir() / "site-ja", dest, std::filesystem::copy_options::recursive);
  write_bytes(dest / "static" / "bin" / "sample.wasm", kWasmBytes);
  write_file(dest / "static" / "images" / kappan::util::from_utf8("🐙.svg"), "<svg/>\n");
  return dest;
}

[[nodiscard]] std::vector<std::byte> as_bytes(const std::string &text) {
  std::vector<std::byte> out(text.size());
  for (std::size_t i = 0; i < text.size(); ++i) {
    out[i] = static_cast<std::byte>(static_cast<unsigned char>(text[i]));
  }
  return out;
}

void require_not_internal_error(const httplib::Result &res) {
  REQUIRE(res);
  REQUIRE(res->status != 500);
  REQUIRE(res->body.find("/Users") == std::string::npos);
  REQUIRE(res->body.find("generation-") == std::string::npos);
  REQUIRE(res->body.find("std::") == std::string::npos);
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

TEST_CASE("HttpServer serves Japanese pages, emoji assets, query, and wasm", "[serve][http]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);
  REQUIRE(store.publish({.source = source}).ok());

  auto started = kappan::serve::HttpServer::start(store, {.host = "127.0.0.1", .port = 0});
  REQUIRE(started);
  auto server = std::move(*started);
  REQUIRE(server.port() != 0);
  REQUIRE(server.running());

  httplib::Client cli{"127.0.0.1", static_cast<int>(server.port())};
  cli.set_connection_timeout(2, 0);
  cli.set_read_timeout(2, 0);

  const auto home = cli.Get("/");
  REQUIRE(home);
  REQUIRE(home->status == 200);
  REQUIRE(home->body.find("ホーム 🐙") != std::string::npos);
  REQUIRE(home->get_header_value("Content-Type").find("text/html") != std::string::npos);
  REQUIRE(home->get_header_value("Cache-Control") == "no-cache");

  const auto queried = cli.Get("/index.html?v=1");
  REQUIRE(queried);
  REQUIRE(queried->status == 200);
  REQUIRE(queried->body.find("ホーム 🐙") != std::string::npos);

  const auto ja = cli.Get("/posts/こんにちは/");
  REQUIRE(ja);
  REQUIRE(ja->status == 200);
  REQUIRE(ja->body.find("最初の記事です") != std::string::npos);

  const auto emoji = cli.Get("/images/🐙.svg");
  REQUIRE(emoji);
  REQUIRE(emoji->status == 200);
  REQUIRE(emoji->body.find("<svg") != std::string::npos);
  REQUIRE(emoji->get_header_value("Content-Type").find("image/svg+xml") != std::string::npos);

  const auto wasm = cli.Get("/bin/sample.wasm");
  REQUIRE(wasm);
  REQUIRE(wasm->status == 200);
  REQUIRE(wasm->get_header_value("Content-Type") == "application/wasm");
  REQUIRE(wasm->get_header_value("Content-Length") == std::to_string(wasm_payload().size()));
  REQUIRE(as_bytes(wasm->body) == wasm_payload());

  const auto head_home = cli.Head("/");
  REQUIRE(head_home);
  REQUIRE(head_home->status == home->status);
  REQUIRE(head_home->get_header_value("Content-Length") ==
          home->get_header_value("Content-Length"));
  REQUIRE(head_home->body.empty());

  const auto head_wasm = cli.Head("/bin/sample.wasm");
  REQUIRE(head_wasm);
  REQUIRE(head_wasm->status == 200);
  REQUIRE(head_wasm->get_header_value("Content-Length") == std::to_string(wasm_payload().size()));
  REQUIRE(head_wasm->body.empty());

  const auto about_redirect = cli.Get("/about");
  REQUIRE(about_redirect);
  REQUIRE((about_redirect->status == 301 || about_redirect->status == 302));
  REQUIRE(about_redirect->get_header_value("Location") == "/about/");
  REQUIRE(about_redirect->get_header_value("Cache-Control") == "no-cache");

  server.request_stop();
  server.request_stop();
  const auto waited = server.wait();
  REQUIRE(waited);
  REQUIRE_FALSE(server.running());
  std::filesystem::remove_all(source);
}

TEST_CASE("HttpServer rejects traversal, bad encoding, marker, and POST", "[serve][http]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);
  REQUIRE(store.publish({.source = source}).ok());

  auto started = kappan::serve::HttpServer::start(store, {.port = 0});
  REQUIRE(started);
  auto server = std::move(*started);

  httplib::Client cli{"127.0.0.1", static_cast<int>(server.port())};
  cli.set_connection_timeout(2, 0);
  cli.set_read_timeout(2, 0);
  cli.set_path_encode(false);

  const auto encoded_dotdot = cli.Get("/%2e%2e/site.yaml");
  require_not_internal_error(encoded_dotdot);
  REQUIRE(encoded_dotdot->status == 400);

  const auto bad_percent = cli.Get("/%ZZ");
  require_not_internal_error(bad_percent);
  REQUIRE(bad_percent->status == 400);

  const auto backslash = cli.Get("/%5c");
  require_not_internal_error(backslash);
  REQUIRE(backslash->status == 400);

  const auto double_encoded = cli.Get("/%252e%252e/site.yaml");
  require_not_internal_error(double_encoded);
  REQUIRE(double_encoded->status == 404);

  const auto marker = cli.Get("/.kappan-out");
  require_not_internal_error(marker);
  REQUIRE(marker->status == 404);

  const auto posted = cli.Post("/");
  require_not_internal_error(posted);
  REQUIRE(posted->status == 405);
  REQUIRE(posted->get_header_value("Allow") == "GET, HEAD");

  const auto put = cli.Put("/", "", "text/plain");
  require_not_internal_error(put);
  REQUIRE(put->status == 405);
  REQUIRE(put->get_header_value("Allow") == "GET, HEAD");

  server.request_stop();
  REQUIRE(server.wait());
  std::filesystem::remove_all(source);
}

TEST_CASE("HttpServer serves decoded paths longer than 256 bytes", "[serve][http]") {
  // httplib RegexMatcher refuses paths longer than CPPHTTPLIB_REGEX_ROUTE_PATH_MAX_LENGTH (256).
  // Split across segments so each filename stays within the 255-byte FS limit.
  std::string seg1_utf8;
  std::string seg2_utf8;
  for (int i = 0; i < 80; ++i) {
    seg1_utf8 += "あ";
  }
  for (int i = 0; i < 10; ++i) {
    seg2_utf8 += "い";
  }
  const auto seg1 = kappan::util::from_utf8(seg1_utf8);
  const auto seg2 = kappan::util::from_utf8(seg2_utf8);
  const auto decoded_path = std::format("/{}/{}/", seg1_utf8, seg2_utf8);
  REQUIRE(decoded_path.size() > 256);

  const auto source = make_japanese_site();
  const std::string body = "<p>長い日本語パス 🐙</p>\n";
  auto builder = [&](const std::filesystem::path &, const std::filesystem::path &out,
                     kappan::DraftPolicy) {
    write_file(out / seg1 / seg2 / "index.html", body);
    return kappan::content::BuildResult{.pages_written = 1};
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);
  REQUIRE(store.publish({.source = source}).ok());

  auto started = kappan::serve::HttpServer::start(store, {.host = "127.0.0.1", .port = 0});
  REQUIRE(started);
  auto server = std::move(*started);

  httplib::Client cli{"127.0.0.1", static_cast<int>(server.port())};
  cli.set_connection_timeout(2, 0);
  cli.set_read_timeout(2, 0);

  const auto get = cli.Get(decoded_path);
  REQUIRE(get);
  REQUIRE(get->status == 200);
  REQUIRE(get->body == body);
  REQUIRE(get->get_header_value("Content-Type").find("text/html") != std::string::npos);

  const auto head = cli.Head(decoded_path);
  REQUIRE(head);
  REQUIRE(head->status == 200);
  REQUIRE(head->get_header_value("Content-Length") == get->get_header_value("Content-Length"));
  REQUIRE(head->body.empty());

  server.request_stop();
  REQUIRE(server.wait());
  std::filesystem::remove_all(source);
}

TEST_CASE("HttpServer bind failure includes host and port", "[serve][http]") {
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);
  const kappan::serve::HttpServerOptions options{.host = "256.256.256.256", .port = 8080};
  const auto started = kappan::serve::HttpServer::start(store, options);
  REQUIRE_FALSE(started);
  REQUIRE(started.error().code == kappan::ErrorCode::Io);
  REQUIRE(started.error().message.find("256.256.256.256") != std::string::npos);
  REQUIRE(started.error().message.find("8080") != std::string::npos);
}
