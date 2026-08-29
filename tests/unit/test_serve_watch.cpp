#include "content/build.hpp"
#include "serve/publish.hpp"
#include "serve/watch.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <format>
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

[[nodiscard]] std::filesystem::path make_japanese_site() {
  const auto dest = unique_temp("kappan-watch-src");
  std::filesystem::remove_all(dest);
  std::filesystem::copy(fixtures_dir() / "site-ja", dest, std::filesystem::copy_options::recursive);
  return dest;
}

[[nodiscard]] std::string generation_text(kappan::serve::GenerationStore &store,
                                          const std::filesystem::path &relative) {
  auto lease = store.acquire_read();
  REQUIRE(lease);
  auto bytes = lease->read_bytes(relative);
  REQUIRE(bytes);
  return {reinterpret_cast<const char *>(bytes->data()), bytes->size()};
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

TEST_CASE("WatchState retries only on new observe or source_changed", "[serve][watch]") {
  using kappan::serve::ChangeKind;
  using kappan::serve::WatchKind;

  const auto root = std::filesystem::temp_directory_path() / "kappan-serve-watch-state";
  std::filesystem::remove_all(root);

  write_file(root / "site.yaml", "title: WatchState\n");
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 初版\n");
  write_file(root / "templates" / "post.html", "<p>{{ content }}</p>\n");
  write_file(root / "static" / "a.css", "body{}\n");

  auto initial = kappan::serve::snapshot_source(root);
  REQUIRE(initial);

  kappan::serve::WatchState state(*initial);
  REQUIRE_FALSE(state.should_attempt());

  state.observe(*initial);
  REQUIRE_FALSE(state.should_attempt());

  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 変更1\n");
  auto content_v1 = kappan::serve::snapshot_source(root);
  REQUIRE(content_v1);
  state.observe(*content_v1);
  REQUIRE(state.should_attempt());

  auto attempt1 = state.begin_attempt();
  REQUIRE(attempt1.baseline == *initial);
  REQUIRE(attempt1.target == *content_v1);
  REQUIRE(attempt1.changes == kappan::serve::diff_snapshots(*initial, *content_v1));
  REQUIRE(kappan::serve::requires_full_publish(attempt1.changes));
  REQUIRE(has_change(attempt1.changes, WatchKind::Content, ChangeKind::Modified,
                     kappan::util::from_utf8("content/記事.md")));
  REQUIRE_FALSE(state.should_attempt());

  // stable failure: published は進まず、同じ snapshot では連続 retry しない
  REQUIRE_FALSE(state.should_attempt());

  // retry_pending: ファイルを増やさず同じ snapshot でも再試行する
  // （observed_ == attempted_ のため、retry_pending_ が無いと should_attempt は false）
  state.mark_source_changed(*content_v1);
  REQUIRE(state.should_attempt());
  auto retry_same = state.begin_attempt();
  REQUIRE(retry_same.baseline == *initial);
  REQUIRE(retry_same.target == *content_v1);
  REQUIRE_FALSE(state.should_attempt());

  // mid-build の追加保存: 失敗済み attempted ではなく published からの差分になる
  write_file(root / "content" / "extra.md", "# 追加\n");
  auto content_v2 = kappan::serve::snapshot_source(root);
  REQUIRE(content_v2);
  state.mark_source_changed(*content_v2);
  REQUIRE(state.should_attempt());

  auto attempt2 = state.begin_attempt();
  REQUIRE(attempt2.baseline == *initial);
  REQUIRE(attempt2.target == *content_v2);
  REQUIRE(attempt2.changes == kappan::serve::diff_snapshots(*initial, *content_v2));
  REQUIRE_FALSE(attempt2.changes == kappan::serve::diff_snapshots(*content_v1, *content_v2));
  REQUIRE(has_change(attempt2.changes, WatchKind::Content, ChangeKind::Modified,
                     kappan::util::from_utf8("content/記事.md")));
  REQUIRE(has_change(attempt2.changes, WatchKind::Content, ChangeKind::Added,
                     std::filesystem::path{"content"} / "extra.md"));
  REQUIRE(kappan::serve::requires_full_publish(attempt2.changes));
  REQUIRE_FALSE(state.should_attempt());

  // full build failure のあと Static だけ保存しても、失敗した Content 差分が残る
  write_file(root / "static" / "a.css", "body{color:red}\n");
  auto with_static = kappan::serve::snapshot_source(root);
  REQUIRE(with_static);
  state.observe(*with_static);
  REQUIRE(state.should_attempt());

  auto attempt3 = state.begin_attempt();
  REQUIRE(attempt3.baseline == *initial);
  REQUIRE(attempt3.target == *with_static);
  REQUIRE(attempt3.changes == kappan::serve::diff_snapshots(*initial, *with_static));
  REQUIRE(has_change(attempt3.changes, WatchKind::Content, ChangeKind::Modified,
                     kappan::util::from_utf8("content/記事.md")));
  REQUIRE(has_change(attempt3.changes, WatchKind::Content, ChangeKind::Added,
                     std::filesystem::path{"content"} / "extra.md"));
  REQUIRE(has_change(attempt3.changes, WatchKind::Static, ChangeKind::Modified,
                     std::filesystem::path{"static"} / "a.css"));
  REQUIRE(kappan::serve::requires_full_publish(attempt3.changes));

  // mark_activated のときだけ published が進む
  state.mark_activated(attempt3.target);
  REQUIRE_FALSE(state.should_attempt());

  state.observe(*with_static);
  REQUIRE_FALSE(state.should_attempt());

  write_file(root / "static" / "a.css", "body{color:blue}\n");
  auto static_only = kappan::serve::snapshot_source(root);
  REQUIRE(static_only);
  state.observe(*static_only);
  REQUIRE(state.should_attempt());

  auto attempt4 = state.begin_attempt();
  REQUIRE(attempt4.baseline == *with_static);
  REQUIRE(attempt4.target == *static_only);
  REQUIRE(attempt4.changes == kappan::serve::diff_snapshots(*with_static, *static_only));
  REQUIRE_FALSE(kappan::serve::requires_full_publish(attempt4.changes));

  state.mark_activated(attempt4.target);
  REQUIRE_FALSE(state.should_attempt());

  std::filesystem::remove_all(root);
}

TEST_CASE("WatchDebounce coalesces two saves into one fire after quiet", "[serve][watch]") {
  using ms = std::chrono::milliseconds;
  using clock = std::chrono::steady_clock;

  const auto root = unique_temp("kappan-serve-watch-debounce");
  std::filesystem::remove_all(root);
  write_file(root / "site.yaml", "title: 監視\n");
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 初版\n");

  auto snap_a = kappan::serve::snapshot_source(root);
  REQUIRE(snap_a);
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 保存1\n");
  auto snap_b = kappan::serve::snapshot_source(root);
  REQUIRE(snap_b);
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 保存2\n");
  auto snap_c = kappan::serve::snapshot_source(root);
  REQUIRE(snap_c);

  kappan::serve::WatchDebounce debounce{ms{150}};
  const auto t0 = clock::time_point{};
  REQUIRE_FALSE(debounce.quiet_elapsed(t0, *snap_a));
  REQUIRE_FALSE(debounce.quiet_elapsed(t0 + ms{10}, *snap_b));
  REQUIRE_FALSE(debounce.quiet_elapsed(t0 + ms{50}, *snap_c));
  REQUIRE_FALSE(debounce.quiet_elapsed(t0 + ms{199}, *snap_c));
  REQUIRE(debounce.quiet_elapsed(t0 + ms{200}, *snap_c));
  REQUIRE(debounce.quiet_elapsed(t0 + ms{201}, *snap_c));

  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 保存3\n");
  auto snap_d = kappan::serve::snapshot_source(root);
  REQUIRE(snap_d);
  REQUIRE_FALSE(debounce.quiet_elapsed(t0 + ms{210}, *snap_d));
  REQUIRE(debounce.quiet_elapsed(t0 + ms{360}, *snap_d));

  std::filesystem::remove_all(root);
}

TEST_CASE("WatchDebounce and WatchState attempt once after quiet elapses", "[serve][watch]") {
  using ms = std::chrono::milliseconds;
  using clock = std::chrono::steady_clock;

  const auto root = unique_temp("kappan-serve-watch-debounce-state");
  std::filesystem::remove_all(root);
  write_file(root / "site.yaml", "title: 監視\n");
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 初版\n");

  auto snap_a = kappan::serve::snapshot_source(root);
  REQUIRE(snap_a);
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 保存1\n");
  auto snap_b = kappan::serve::snapshot_source(root);
  REQUIRE(snap_b);
  write_file(root / "content" / kappan::util::from_utf8("記事.md"), "# 保存2\n");
  auto snap_c = kappan::serve::snapshot_source(root);
  REQUIRE(snap_c);

  kappan::serve::WatchState state{*snap_a};
  kappan::serve::WatchDebounce debounce{ms{150}};
  int attempts = 0;
  kappan::serve::SourceSnapshot fired;
  const auto t0 = clock::time_point{};
  const auto feed = [&](clock::time_point now, const kappan::serve::SourceSnapshot &snap) {
    state.observe(snap);
    if (state.should_attempt() && debounce.quiet_elapsed(now, snap)) {
      auto attempt = state.begin_attempt();
      fired = attempt.target;
      ++attempts;
      state.mark_activated(attempt.target);
    }
  };

  feed(t0, *snap_a);
  feed(t0 + ms{10}, *snap_b);
  feed(t0 + ms{50}, *snap_c);
  REQUIRE(attempts == 0);
  feed(t0 + ms{200}, *snap_c);
  REQUIRE(attempts == 1);
  REQUIRE(fired == *snap_c);
  feed(t0 + ms{250}, *snap_c);
  REQUIRE(attempts == 1);

  std::filesystem::remove_all(root);
}

TEST_CASE("apply_watch_attempt applies static-only once and activates target", "[serve][watch]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    ++builds;
    return kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);

  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());
  REQUIRE(builds == 1);
  REQUIRE(store.generation() == 1);

  kappan::serve::WatchState state{first.snapshot};
  const auto css_rel = kappan::util::from_utf8("static/css/日本語🐙.css");
  write_file(source / css_rel, "body{color:#333}\n");
  auto latest = kappan::serve::snapshot_source(source);
  REQUIRE(latest);
  state.observe(*latest);
  REQUIRE(state.should_attempt());

  auto attempt = state.begin_attempt();
  REQUIRE_FALSE(kappan::serve::requires_full_publish(attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, attempt, {.source = source});

  REQUIRE(builds == 1);
  REQUIRE(store.generation() == 2);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(generation_text(store, kappan::util::from_utf8("css/日本語🐙.css")) ==
          "body{color:#333}\n");

  write_file(source / "content" / "index.md", "---\ntitle: ホーム\n---\n# ホーム 🐙 改訂\n");
  auto after_content = kappan::serve::snapshot_source(source);
  REQUIRE(after_content);
  state.observe(*after_content);
  REQUIRE(state.should_attempt());
  auto next = state.begin_attempt();
  REQUIRE(next.baseline == *latest);

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_watch_attempt publishes full for content and mixed changes", "[serve][watch]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    ++builds;
    return kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);

  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());
  kappan::serve::WatchState state{first.snapshot};

  write_file(source / "content" / "index.md", "---\ntitle: ホーム\n---\n# ホーム 🐙 改訂\n");
  auto content_only = kappan::serve::snapshot_source(source);
  REQUIRE(content_only);
  state.observe(*content_only);
  auto content_attempt = state.begin_attempt();
  REQUIRE(kappan::serve::requires_full_publish(content_attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, content_attempt, {.source = source});

  REQUIRE(builds == 2);
  REQUIRE(store.generation() == 2);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(generation_text(store, "index.html").find("ホーム 🐙 改訂") != std::string::npos);

  write_file(source / "content" / "about.md", "---\ntitle: 概要\n---\n概要 改訂\n");
  write_file(source / "static" / "a.css", "body{color:red}\n");
  auto mixed = kappan::serve::snapshot_source(source);
  REQUIRE(mixed);
  state.observe(*mixed);
  auto mixed_attempt = state.begin_attempt();
  REQUIRE(kappan::serve::requires_full_publish(mixed_attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, mixed_attempt, {.source = source});

  REQUIRE(builds == 3);
  REQUIRE(store.generation() == 3);
  REQUIRE(generation_text(store, "about/index.html").find("概要 改訂") != std::string::npos);
  REQUIRE(generation_text(store, "a.css") == "body{color:red}\n");

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_watch_attempt keeps published on static failure", "[serve][watch]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);
  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());

  kappan::serve::WatchState state{first.snapshot};
  write_file(source / "static" / "index.html", "<p>衝突</p>\n");
  auto latest = kappan::serve::snapshot_source(source);
  REQUIRE(latest);
  state.observe(*latest);
  auto attempt = state.begin_attempt();
  REQUIRE_FALSE(kappan::serve::requires_full_publish(attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, attempt, {.source = source});

  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(generation_text(store, "index.html").find("ホーム 🐙") != std::string::npos);
  REQUIRE(generation_text(store, "index.html").find("衝突") == std::string::npos);

  write_file(source / "static" / "a.css", "body{}\n");
  auto next_snap = kappan::serve::snapshot_source(source);
  REQUIRE(next_snap);
  state.observe(*next_snap);
  auto next = state.begin_attempt();
  REQUIRE(next.baseline == first.snapshot);

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_watch_attempt keeps published on BuildFailed without retry", "[serve][watch]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    ++builds;
    return kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);
  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());
  REQUIRE(builds == 1);

  kappan::serve::WatchState state{first.snapshot};
  write_file(source / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md"),
             "---\ntitle: 壊した\ndate: 2026-13-01\n---\n壊れた\n");
  auto broken = kappan::serve::snapshot_source(source);
  REQUIRE(broken);
  state.observe(*broken);
  auto attempt = state.begin_attempt();
  REQUIRE(kappan::serve::requires_full_publish(attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, attempt, {.source = source});

  REQUIRE(builds == 2);
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(generation_text(store, "index.html").find("ホーム 🐙") != std::string::npos);

  state.observe(*broken);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(builds == 2);
  REQUIRE(store.generation() == 1);

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_watch_attempt retries immediately on SourceChanged", "[serve][watch]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    auto result =
        kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
    ++builds;
    if (builds == 2) {
      write_file(src / "content" / kappan::util::from_utf8("追加.md"),
                 "---\ntitle: 追加\n---\nbuild 中の保存\n");
    }
    return result;
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);
  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());
  REQUIRE(builds == 1);

  kappan::serve::WatchState state{first.snapshot};
  write_file(source / "content" / "index.md", "---\ntitle: ホーム\n---\n# ホーム 🐙 改訂\n");
  auto latest = kappan::serve::snapshot_source(source);
  REQUIRE(latest);
  state.observe(*latest);
  auto attempt = state.begin_attempt();
  kappan::serve::apply_watch_attempt(state, store, attempt, {.source = source});

  REQUIRE(builds == 3);
  REQUIRE(store.generation() == 3);
  REQUIRE_FALSE(state.should_attempt());
  REQUIRE(generation_text(store, "index.html").find("ホーム 🐙 改訂") != std::string::npos);

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_watch_attempt after full failure still full-publishes a static save",
          "[serve][watch]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    ++builds;
    return kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
  };
  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);
  const auto first = store.publish({.source = source});
  REQUIRE(first.ok());

  kappan::serve::WatchState state{first.snapshot};
  write_file(source / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md"),
             "---\ntitle: 壊した\ndate: 2026-13-01\n---\n壊れた\n");
  auto broken = kappan::serve::snapshot_source(source);
  REQUIRE(broken);
  state.observe(*broken);
  kappan::serve::apply_watch_attempt(state, store, state.begin_attempt(), {.source = source});
  REQUIRE(builds == 2);
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(state.should_attempt());

  write_file(source / "static" / "a.css", "body{color:red}\n");
  auto with_static = kappan::serve::snapshot_source(source);
  REQUIRE(with_static);
  state.observe(*with_static);
  REQUIRE(state.should_attempt());
  auto attempt = state.begin_attempt();
  REQUIRE(attempt.baseline == first.snapshot);
  REQUIRE(kappan::serve::requires_full_publish(attempt.changes));
  kappan::serve::apply_watch_attempt(state, store, attempt, {.source = source});

  REQUIRE(builds == 3);
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(state.should_attempt());

  std::filesystem::remove_all(source);
}
