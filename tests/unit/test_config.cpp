#include <kappan/config.hpp>
#include <kappan/error.hpp>

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

} // namespace

TEST_CASE("load reads Japanese site.yaml and ignores unknown keys") {
  const auto path = fixtures_dir() / "site-ja" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->title == "活版ブログ");
  REQUIRE(result->url == "https://example.com");
  REQUIRE(result->language == "ja");
  REQUIRE(result->description.find("🐙") != std::string::npos);
}

TEST_CASE("load reports a missing title with a line number") {
  const auto path = fixtures_dir() / "site-bad-config" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  REQUIRE(result.error().message.find("title") != std::string::npos);
}

TEST_CASE("load reports broken YAML without throwing") {
  const auto path = fixtures_dir() / "site-broken-yaml" / "site.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  REQUIRE(*result.error().line >= 1);
}

TEST_CASE("load reports a missing site.yaml") {
  const auto path = fixtures_dir() / "site-ja" / "missing.yaml";
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  const bool mentions_file = result.error().message.find("site.yaml") != std::string::npos ||
                             result.error().message.find("missing.yaml") != std::string::npos;
  REQUIRE(mentions_file);
}

TEST_CASE("load accepts CRLF site.yaml") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-crlf.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: CRLFサイト\r\nlanguage: ja\r\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->title == "CRLFサイト");
  std::filesystem::remove(path);
}

TEST_CASE("load reads pagination posts_per_page") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-paginate.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: ページ\npagination:\n  posts_per_page: 2\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->posts_per_page == 2);
  std::filesystem::remove(path);
}

TEST_CASE("load rejects a negative posts_per_page") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-paginate-bad.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: ページ\npagination:\n  posts_per_page: -1\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove(path);
}

TEST_CASE("load rejects a sequence title") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-seq.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title:\n  - a\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE_FALSE(result);
  REQUIRE(result.error().code == kappan::ErrorCode::Config);
  REQUIRE(result.error().line.has_value());
  std::filesystem::remove(path);
}

TEST_CASE("load accepts an empty url") {
  const auto path = std::filesystem::temp_directory_path() / "kappan-site-url-empty.yaml";
  {
    std::ofstream out(path, std::ios::binary);
    out << "title: URLなし\n";
  }
  const auto result = kappan::config::load(path);
  REQUIRE(result);
  REQUIRE(result->url.empty());
  std::filesystem::remove(path);
}

TEST_CASE("load rejects a url that is not an absolute http URL") {
  struct Case {
    std::string name;
    std::string yaml_url;
  };
  std::vector<Case> cases{{"kappan-site-url-bare", "example.com"},
                          {"kappan-site-url-relative", "/blog"},
                          {"kappan-site-url-scheme-only", "https://"},
                          {"kappan-site-url-no-host", "https:///blog"},
                          {"kappan-site-url-query-without-host", "https://?q=1"},
                          {"kappan-site-url-fragment-without-host", "https://#top"},
                          {"kappan-site-url-other-scheme", "ftp://example.com"},
                          {"kappan-site-url-empty-userinfo", "https://@/path"},
                          {"kappan-site-url-userinfo", "https://user@example.com"},
                          {"kappan-site-url-space", "https://exa mple.com"},
                          {"kappan-site-url-tab", "https://exa\\tmple.com"},
                          {"kappan-site-url-control", "https://exa\\u001fmple.com"},
                          {"kappan-site-url-del", "https://exa\\u007fmple.com"},
                          {"kappan-site-url-empty-port", "https://example.com:"},
                          {"kappan-site-url-named-port", "https://example.com:http"},
                          {"kappan-site-url-zero-port", "https://example.com:0"},
                          {"kappan-site-url-large-port", "https://example.com:65536"},
                          {"kappan-site-url-invalid-ipv4", "https://256.1.1.1"},
                          {"kappan-site-url-unclosed-ipv6", "https://[2001:db8::1"},
                          {"kappan-site-url-empty-ipv6", "https://[]"},
                          {"kappan-site-url-double-compress-ipv6", "https://[1::2::3]"},
                          {"kappan-site-url-empty-hextets-ipv6", "https://[::::]"},
                          {"kappan-site-url-invalid-ipv4-tail", "https://[192.0.2.1::]"},
                          {"kappan-site-url-ipv6-zone", "https://[fe80::1%eth0]"},
                          {"kappan-site-url-empty-dns-label", "https://example..com"},
                          {"kappan-site-url-leading-dns-hyphen", "https://-example.com"}};
  const std::string label_64(64, 'a');
  cases.push_back({"kappan-site-url-long-dns-label", "https://" + label_64 + ".example"});
  const std::string host_254 = std::string(63, 'a') + "." + std::string(63, 'b') + "." +
                               std::string(63, 'c') + "." + std::string(62, 'd');
  cases.push_back({"kappan-site-url-long-dns-name", "https://" + host_254});
  cases.push_back({"kappan-site-url-long-dns-name-dot", "https://" + host_254 + "."});

  for (const auto &item : cases) {
    CAPTURE(item.yaml_url);
    const auto path = std::filesystem::temp_directory_path() / (item.name + ".yaml");
    {
      std::ofstream out(path, std::ios::binary);
      out << "title: サイト\nurl: \"" << item.yaml_url << "\"\n";
    }
    const auto result = kappan::config::load(path);
    CHECK_FALSE(result);
    if (!result) {
      CHECK(result.error().code == kappan::ErrorCode::Config);
      REQUIRE(result.error().where.has_value());
      CHECK(*result.error().where == path);
      REQUIRE(result.error().line.has_value());
      CHECK(*result.error().line == 2);
      CHECK(result.error().message.find("url") != std::string::npos);
    }
    std::filesystem::remove(path);
  }
}

TEST_CASE("load accepts http and https absolute urls") {
  const std::string label_63(63, 'a');
  const std::string host_253 = std::string(63, 'a') + "." + std::string(63, 'b') + "." +
                               std::string(63, 'c') + "." + std::string(61, 'd');
  const std::vector<std::string> cases{
      "https://example.com",
      "http://example.com:8080",
      "https://sub.example.com/blog?q=日本語#先頭",
      "https://127.0.0.1:65535",
      "https://[2001:db8::1]",
      "https://[::1]:443/path",
      "https://[::ffff:192.0.2.1]",
      "https://example.com.",
      "https://Example.COM/Blog/",
      "https://example.com/%2F?q=%2526#%23",
      "https://123",
      "https://123.456",
      "https://" + label_63 + ".example",
      "https://" + host_253,
      "https://" + host_253 + ".",
  };
  for (const auto &url : cases) {
    CAPTURE(url);
    const auto path = std::filesystem::temp_directory_path() / "kappan-site-url-ok.yaml";
    {
      std::ofstream out(path, std::ios::binary);
      out << "title: サイト\nurl: \"" << url << "\"\n";
    }
    const auto result = kappan::config::load(path);
    REQUIRE(result);
    REQUIRE(result->url == url);
    std::filesystem::remove(path);
  }
}
