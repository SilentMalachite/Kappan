#include "serve/run.hpp"
#include "util/path.hpp"

#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>
#include <httplib.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
#include <set>
#include <string>
#include <string_view>

namespace {

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

[[nodiscard]] std::filesystem::path copy_fixture(std::string_view prefix,
                                                 const std::filesystem::path &name) {
  const auto dest = unique_temp(prefix);
  std::filesystem::remove_all(dest);
  std::filesystem::copy(fixtures_dir() / name, dest, std::filesystem::copy_options::recursive);
  return dest;
}

[[nodiscard]] std::filesystem::path make_japanese_site() {
  return copy_fixture("kappan-serve-src", "site-ja");
}

[[nodiscard]] std::filesystem::path make_bad_front_matter_site() {
  return copy_fixture("kappan-serve-bad", "site-bad-fm");
}

[[nodiscard]] bool is_store_workspace_name(std::string_view name) {
  constexpr std::string_view kPrefix = "kappan-";
  if (!name.starts_with(kPrefix) || name.size() != kPrefix.size() + 32) {
    return false;
  }
  for (const char ch : name.substr(kPrefix.size())) {
    const bool hex = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f');
    if (!hex) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::set<std::filesystem::path> store_workspaces() {
  std::set<std::filesystem::path> dirs;
  std::error_code ec;
  const auto tmp = std::filesystem::temp_directory_path(ec);
  if (ec) {
    return dirs;
  }
  for (const auto &entry : std::filesystem::directory_iterator(tmp, ec)) {
    std::error_code dir_ec;
    if (!entry.is_directory(dir_ec)) {
      continue;
    }
    if (is_store_workspace_name(kappan::util::to_utf8(entry.path().filename()))) {
      dirs.insert(entry.path());
    }
  }
  return dirs;
}

[[nodiscard]] std::filesystem::path new_workspace(const std::set<std::filesystem::path> &before) {
  for (const auto &dir : store_workspaces()) {
    if (!before.contains(dir)) {
      return dir;
    }
  }
  return {};
}

} // namespace

TEST_CASE("ServeSession serves a Japanese site and reclaims the workspace", "[serve][run]") {
  const auto source = make_japanese_site();
  const auto before = store_workspaces();
  std::filesystem::path workspace;
  std::uint16_t port = 0;

  {
    auto started = kappan::serve::ServeSession::start({.source = source, .port = 0});
    REQUIRE(started);
    auto session = std::move(*started);
    port = session.port();
    REQUIRE(port != 0);
    REQUIRE(session.running());
    REQUIRE_FALSE(std::filesystem::exists(source / "_site"));

    workspace = new_workspace(before);
    REQUIRE_FALSE(workspace.empty());
    REQUIRE(std::filesystem::exists(workspace));

    httplib::Client cli{"127.0.0.1", static_cast<int>(port)};
    cli.set_connection_timeout(2, 0);
    cli.set_read_timeout(2, 0);

    const auto home = cli.Get("/");
    REQUIRE(home);
    REQUIRE(home->status == 200);
    REQUIRE(home->body.find("ホーム 🐙") != std::string::npos);

    const auto ja = cli.Get("/posts/こんにちは/");
    REQUIRE(ja);
    REQUIRE(ja->status == 200);
    REQUIRE(ja->body.find("最初の記事です") != std::string::npos);

    session.request_stop();
    session.request_stop();
    const auto waited = session.wait();
    REQUIRE(waited);
    REQUIRE_FALSE(session.running());

    httplib::Client stopped{"127.0.0.1", static_cast<int>(port)};
    stopped.set_connection_timeout(1, 0);
    const auto refused = stopped.Get("/");
    REQUIRE_FALSE(refused);
  }

  REQUIRE_FALSE(std::filesystem::exists(workspace));
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));
  std::filesystem::remove_all(source);
}

TEST_CASE("ServeSession does not bind when the first publish fails", "[serve][run]") {
  const auto source = make_bad_front_matter_site();
  const auto before = store_workspaces();

  const auto started = kappan::serve::ServeSession::start({.source = source, .port = 0});
  REQUIRE_FALSE(started);
  REQUIRE(started.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(started.error().message.find("date") != std::string::npos);
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));

  for (const auto &dir : store_workspaces()) {
    REQUIRE(before.contains(dir));
  }
  std::filesystem::remove_all(source);
}

TEST_CASE("ServeSession bind failure includes host and port", "[serve][run]") {
  const auto source = make_japanese_site();
  const auto before = store_workspaces();
  const kappan::serve::ServeOptions options{
      .source = source, .host = "256.256.256.256", .port = 8080};

  const auto started = kappan::serve::ServeSession::start(options);
  REQUIRE_FALSE(started);
  REQUIRE(started.error().code == kappan::ErrorCode::Io);
  REQUIRE(started.error().message.find("256.256.256.256") != std::string::npos);
  REQUIRE(started.error().message.find("8080") != std::string::npos);
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));

  for (const auto &dir : store_workspaces()) {
    REQUIRE(before.contains(dir));
  }
  std::filesystem::remove_all(source);
}

TEST_CASE("run returns the first publish error without listening", "[serve][run]") {
  const auto source = make_bad_front_matter_site();
  const auto result = kappan::serve::run({.source = source, .port = 0});
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(result.error().message.find("date") != std::string::npos);
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));
  std::filesystem::remove_all(source);
}

TEST_CASE("run returns Io when bind fails", "[serve][run]") {
  const auto source = make_japanese_site();
  const auto result =
      kappan::serve::run({.source = source, .host = "256.256.256.256", .port = 8080});
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Io);
  REQUIRE(result.error().message.find("256.256.256.256") != std::string::npos);
  REQUIRE(result.error().message.find("8080") != std::string::npos);
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));
  std::filesystem::remove_all(source);
}
