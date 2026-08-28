#include "content/scaffold.hpp"

#include "util/path.hpp"
#include "util/utf8.hpp"

#include <kappan/embedded_theme.hpp>

#include <format>
#include <string_view>
#include <utility>

namespace kappan::content {
namespace {

constexpr std::string_view kSiteYaml = R"(title: 活版サイト
language: ja
description: kappan new が作った日本語サイト
)";

constexpr std::string_view kIndexMd = R"(---
title: ホーム
---
# ホーム 🐙

活版で作ったサイトです。
)";

constexpr std::string_view kPostMd = R"(---
title: こんにちは
date: 2026-01-01
layout: post
tags: [日本語]
---
最初の記事です。
)";

[[nodiscard]] bool is_empty_directory(const std::filesystem::path &dir) {
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return true;
  }
  if (!std::filesystem::is_directory(dir, ec)) {
    return false;
  }
  return std::filesystem::directory_iterator(dir, ec) == std::filesystem::directory_iterator();
}

} // namespace

Result<void> create_site(const std::filesystem::path &dir) {
  if (!is_empty_directory(dir)) {
    return tl::unexpected(make_error(
        ErrorCode::Cli,
        std::format("{}: 空でないディレクトリにはサイトを作れません", util::to_generic_utf8(dir)),
        dir));
  }

  const std::pair<std::filesystem::path, std::string_view> files[] = {
      {dir / "site.yaml", kSiteYaml},
      {dir / "content" / "index.md", kIndexMd},
      {dir / "content" / "posts" / util::from_utf8("2026-01-01-こんにちは.md"), kPostMd},
      {dir / "templates" / "base.html", render::embedded::base_html},
      {dir / "templates" / "post.html", render::embedded::post_html},
      {dir / "templates" / "page.html", render::embedded::page_html},
      {dir / "templates" / "index.html", render::embedded::index_html},
      {dir / "templates" / "tag.html", render::embedded::tag_html},
  };

  for (const auto &[path, text] : files) {
    auto written = util::write_utf8_file(path, text);
    if (!written) {
      return tl::unexpected(written.error());
    }
  }
  return {};
}

} // namespace kappan::content
