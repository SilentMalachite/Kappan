#include "serve/watch.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {

void write_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

[[nodiscard]] std::filesystem::path make_watch_fixture() {
  const auto root = std::filesystem::temp_directory_path() / "kappan-serve-watch-rules";
  std::filesystem::remove_all(root);

  write_file(root / "site.yaml", "title: 監視\n");
  write_file(root / "other.yaml", "ignored: true\n");
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 記事\n");
  write_file(root / "content" / ".draft" / kappan::util::from_utf8("下書き.md"), "# 下書き\n");
  write_file(root / "content" / "_private" / kappan::util::from_utf8("秘密.md"), "# 秘密\n");
  write_file(root / "templates" / "post.html", "<p>{{ content }}</p>\n");
  write_file(root / "templates" / "nested" / "page.html", "<p>nested</p>\n");
  write_file(root / "static" / "images" / kappan::util::from_utf8("🐙.svg"), "<svg/>\n");
  write_file(root / "static" / ".DS_Store", "meta\n");
  write_file(root / "static" / "_private" / "file.txt", "nope\n");
  return root;
}

[[nodiscard]] bool has_change(const std::vector<kappan::serve::SourceChange> &changes,
                              kappan::serve::WatchKind watch_kind,
                              kappan::serve::ChangeKind change_kind,
                              const std::filesystem::path &relative) {
  return std::ranges::any_of(changes, [&](const kappan::serve::SourceChange &change) {
    return change.watch_kind == watch_kind && change.change_kind == change_kind &&
           change.relative == relative;
  });
}

} // namespace

TEST_CASE("snapshot_source follows pipeline watch rules", "[serve][watch]") {
  const auto root = make_watch_fixture();
  const auto snap = kappan::serve::snapshot_source(root);
  REQUIRE(snap);
  REQUIRE(snap->entries.size() == 5);

  const auto site_key = std::string{"site.yaml"};
  const auto article_key =
      kappan::util::to_generic_utf8(kappan::util::from_utf8("content/記事.md"));
  const auto draft_key =
      kappan::util::to_generic_utf8(kappan::util::from_utf8("content/.draft/下書き.md"));
  const auto template_key = std::string{"templates/post.html"};
  const auto static_key =
      kappan::util::to_generic_utf8(kappan::util::from_utf8("static/images/🐙.svg"));

  REQUIRE(snap->entries.contains(site_key));
  REQUIRE(snap->entries.contains(article_key));
  REQUIRE(snap->entries.contains(draft_key));
  REQUIRE(snap->entries.contains(template_key));
  REQUIRE(snap->entries.contains(static_key));

  REQUIRE_FALSE(snap->entries.contains("other.yaml"));
  REQUIRE_FALSE(snap->entries.contains(
      kappan::util::to_generic_utf8(kappan::util::from_utf8("content/_private/秘密.md"))));
  REQUIRE_FALSE(snap->entries.contains("templates/nested/page.html"));
  REQUIRE_FALSE(snap->entries.contains("static/.DS_Store"));
  REQUIRE_FALSE(snap->entries.contains("static/_private/file.txt"));

  REQUIRE(snap->entries.at(site_key).watch_kind == kappan::serve::WatchKind::Config);
  REQUIRE(snap->entries.at(article_key).watch_kind == kappan::serve::WatchKind::Content);
  REQUIRE(snap->entries.at(draft_key).watch_kind == kappan::serve::WatchKind::Content);
  REQUIRE(snap->entries.at(template_key).watch_kind == kappan::serve::WatchKind::Template);
  REQUIRE(snap->entries.at(static_key).watch_kind == kappan::serve::WatchKind::Static);

  REQUIRE(snap->entries.at(site_key).digest.has_value());
  REQUIRE(snap->entries.at(article_key).digest.has_value());
  REQUIRE(snap->entries.at(template_key).digest.has_value());
  REQUIRE_FALSE(snap->entries.at(static_key).digest.has_value());

  REQUIRE(snap->entries.at(article_key).relative == kappan::util::from_utf8("content/記事.md"));
  REQUIRE(snap->entries.at(static_key).relative == kappan::util::from_utf8("static/images/🐙.svg"));
  REQUIRE(article_key == "content/記事.md");
  REQUIRE(static_key == "static/images/🐙.svg");

  std::filesystem::remove_all(root);
}

TEST_CASE("diff_snapshots detects add, remove, and Japanese rename", "[serve][watch]") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-serve-watch-diff";
  std::filesystem::remove_all(root);

  write_file(root / "site.yaml", "title: A\n");
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "abcd\n");
  write_file(root / "templates" / "post.html", "<p>a</p>\n");
  write_file(root / "static" / "a.txt", "static-a\n");

  auto before = kappan::serve::snapshot_source(root);
  REQUIRE(before);

  write_file(root / "content" / "new.md", "# new\n");
  std::filesystem::remove(root / "static" / "a.txt");
  std::filesystem::rename(root / "content" / kappan::util::from_utf8("記事.md"),
                          root / "content" / kappan::util::from_utf8("改名.md"));

  auto after = kappan::serve::snapshot_source(root);
  REQUIRE(after);

  const auto changes = kappan::serve::diff_snapshots(*before, *after);
  REQUIRE(has_change(changes, kappan::serve::WatchKind::Content, kappan::serve::ChangeKind::Added,
                     std::filesystem::path{"content"} / "new.md"));
  REQUIRE(has_change(changes, kappan::serve::WatchKind::Content, kappan::serve::ChangeKind::Removed,
                     kappan::util::from_utf8("content/記事.md")));
  REQUIRE(has_change(changes, kappan::serve::WatchKind::Content, kappan::serve::ChangeKind::Added,
                     kappan::util::from_utf8("content/改名.md")));
  REQUIRE(has_change(changes, kappan::serve::WatchKind::Static, kappan::serve::ChangeKind::Removed,
                     std::filesystem::path{"static"} / "a.txt"));
  REQUIRE(kappan::serve::requires_full_publish(changes));

  std::filesystem::remove_all(root);
}

TEST_CASE("diff_snapshots detects same-size content change via digest", "[serve][watch]") {
  const auto root = std::filesystem::temp_directory_path() / "kappan-serve-watch-digest";
  std::filesystem::remove_all(root);

  write_file(root / "site.yaml", "title: A\n");
  write_file(root / "content" / "note.md", "abcd\n");

  auto before = kappan::serve::snapshot_source(root);
  REQUIRE(before);
  const auto key = std::string{"content/note.md"};
  const auto old_mtime = before->entries.at(key).modified;
  const auto old_digest = before->entries.at(key).digest;
  REQUIRE(old_digest.has_value());

  write_file(root / "content" / "note.md", "efgh\n");
  std::filesystem::last_write_time(root / "content" / "note.md", old_mtime);

  auto after = kappan::serve::snapshot_source(root);
  REQUIRE(after);
  REQUIRE(after->entries.at(key).size == before->entries.at(key).size);
  // file_time_type を Catch の << に渡すと libc++ で曖昧になるため bool 化する。
  const bool same_mtime = after->entries.at(key).modified == old_mtime;
  REQUIRE(same_mtime);
  REQUIRE(after->entries.at(key).digest != old_digest);

  const auto changes = kappan::serve::diff_snapshots(*before, *after);
  REQUIRE(changes.size() == 1);
  REQUIRE(has_change(changes, kappan::serve::WatchKind::Content,
                     kappan::serve::ChangeKind::Modified,
                     std::filesystem::path{"content"} / "note.md"));

  std::filesystem::remove_all(root);
}

TEST_CASE("requires_full_publish is true for non-static changes only", "[serve][watch]") {
  using kappan::serve::ChangeKind;
  using kappan::serve::SourceChange;
  using kappan::serve::WatchKind;

  const std::vector<SourceChange> content_change{
      {WatchKind::Content, ChangeKind::Modified, std::filesystem::path{"content"} / "a.md"},
  };
  const std::vector<SourceChange> template_change{
      {WatchKind::Template, ChangeKind::Added, std::filesystem::path{"templates"} / "post.html"},
  };
  const std::vector<SourceChange> config_change{
      {WatchKind::Config, ChangeKind::Modified, std::filesystem::path{"site.yaml"}},
  };
  const std::vector<SourceChange> static_only{
      {WatchKind::Static, ChangeKind::Modified, std::filesystem::path{"static"} / "a.css"},
      {WatchKind::Static, ChangeKind::Added, std::filesystem::path{"static"} / "b.css"},
  };
  const std::vector<SourceChange> mixed{
      {WatchKind::Static, ChangeKind::Modified, std::filesystem::path{"static"} / "a.css"},
      {WatchKind::Content, ChangeKind::Added, std::filesystem::path{"content"} / "b.md"},
  };

  REQUIRE(kappan::serve::requires_full_publish(content_change));
  REQUIRE(kappan::serve::requires_full_publish(template_change));
  REQUIRE(kappan::serve::requires_full_publish(config_change));
  REQUIRE_FALSE(kappan::serve::requires_full_publish(static_only));
  REQUIRE(kappan::serve::requires_full_publish(mixed));
  REQUIRE_FALSE(kappan::serve::requires_full_publish({}));
}
