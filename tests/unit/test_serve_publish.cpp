#include "content/build.hpp"
#include "output/write.hpp"
#include "serve/publish.hpp"
#include "util/path.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::atomic<int> g_temp_seq{0};

[[nodiscard]] std::filesystem::path fixtures_dir() {
  return std::filesystem::path(__FILE__).parent_path().parent_path() / "fixtures";
}

[[nodiscard]] std::filesystem::path unique_temp(std::string_view prefix) {
  return std::filesystem::temp_directory_path() /
         std::format("{}-{}-{}", prefix,
                     std::chrono::steady_clock::now().time_since_epoch().count(),
                     g_temp_seq.fetch_add(1));
}

void write_file(const std::filesystem::path &path, std::string_view content) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(content.data(), static_cast<std::streamsize>(content.size()));
}

void write_bytes(const std::filesystem::path &path, std::span<const std::byte> bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out.write(reinterpret_cast<const char *>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
}

[[nodiscard]] std::string as_text(const kappan::serve::ByteBuffer &bytes) {
  return {reinterpret_cast<const char *>(bytes.data()), bytes.size()};
}

[[nodiscard]] int count_generation_dirs(const std::filesystem::path &workspace) {
  int count = 0;
  std::error_code ec;
  for (const auto &entry : std::filesystem::directory_iterator(workspace, ec)) {
    std::error_code type_ec;
    if (!entry.is_directory(type_ec)) {
      continue;
    }
    const auto name = kappan::util::to_utf8(entry.path().filename());
    if (name.starts_with("generation-")) {
      ++count;
    }
  }
  return count;
}

constexpr std::byte kWasmBytes[] = {
    std::byte{0x00}, std::byte{0x61}, std::byte{0x73}, std::byte{0x6d},
    std::byte{0xff}, std::byte{0x00}, std::byte{0xff},
};

[[nodiscard]] std::vector<std::byte> wasm_payload() {
  return {std::begin(kWasmBytes), std::end(kWasmBytes)};
}

[[nodiscard]] std::filesystem::path make_japanese_site() {
  const auto dest = unique_temp("kappan-publish-src");
  std::filesystem::remove_all(dest);
  std::filesystem::copy(fixtures_dir() / "site-ja", dest, std::filesystem::copy_options::recursive);
  write_bytes(dest / "static" / "bin" / "sample.wasm", kWasmBytes);
  return dest;
}

[[nodiscard]] kappan::serve::SourceChange static_change(kappan::serve::ChangeKind kind,
                                                        std::string_view relative_utf8) {
  return {
      .watch_kind = kappan::serve::WatchKind::Static,
      .change_kind = kind,
      .relative = kappan::util::from_utf8(relative_utf8),
  };
}

} // namespace

TEST_CASE("acquire_read without a generation returns Io", "[serve][publish]") {
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  const auto acquired = store.acquire_read();
  REQUIRE_FALSE(acquired);
  REQUIRE(acquired.error().code == kappan::ErrorCode::Io);
  REQUIRE(acquired.error().message.find("生成世代") != std::string::npos);
}

TEST_CASE("publish activates a Japanese site and keeps it on build failure", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  const auto first = store.publish({.source = source});
  REQUIRE(first.status == kappan::serve::PublishStatus::Activated);
  REQUIRE(first.ok());
  REQUIRE_FALSE(first.retry_required());
  REQUIRE(first.errors.empty());
  REQUIRE_FALSE(first.snapshot.entries.empty());
  REQUIRE(first.pages_written == 5);
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));

  auto acquired = store.acquire_read();
  REQUIRE(acquired);
  REQUIRE(acquired->generation() == 1);

  const auto index = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index);
  const auto index_html = as_text(*index);
  REQUIRE(index_html.find("ホーム 🐙") != std::string::npos);
  REQUIRE_FALSE(index_html.starts_with("\xEF\xBB\xBF"));

  const auto post = acquired->read_bytes(kappan::util::from_utf8("posts/こんにちは/index.html"));
  REQUIRE(post);
  REQUIRE(as_text(*post).find("最初の記事です") != std::string::npos);

  const auto wasm = acquired->read_bytes(std::filesystem::path{"bin"} / "sample.wasm");
  REQUIRE(wasm);
  REQUIRE(*wasm == wasm_payload());

  const auto workspace = acquired->root().parent_path();
  REQUIRE(count_generation_dirs(workspace) == 1);
  REQUIRE(workspace != source);

  write_file(source / "content" / "posts" / kappan::util::from_utf8("2026-01-01-こんにちは.md"),
             "---\ntitle: 壊した\ndate: 2026-13-01\n---\n壊れた\n");

  const auto failed = store.publish({.source = source});
  REQUIRE(failed.status == kappan::serve::PublishStatus::BuildFailed);
  REQUIRE_FALSE(failed.ok());
  REQUIRE_FALSE(failed.retry_required());
  REQUIRE_FALSE(failed.errors.empty());
  REQUIRE(failed.snapshot.entries.empty());
  REQUIRE(failed.errors.front().code == kappan::ErrorCode::FrontMatter);
  REQUIRE(store.generation() == 1);
  REQUIRE(count_generation_dirs(workspace) == 1);

  const auto index_again = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index_again);
  REQUIRE(*index_again == *index);
  REQUIRE(std::filesystem::exists(acquired->root() / "index.html"));
  REQUIRE_FALSE(std::filesystem::exists(source / "_site"));

  std::filesystem::remove_all(source);
}

TEST_CASE("publish reports SourceChanged when source mutates during build", "[serve][publish]") {
  const auto source = make_japanese_site();
  int builds = 0;
  auto builder = [&](const std::filesystem::path &src, const std::filesystem::path &out,
                     kappan::DraftPolicy drafts) {
    auto result =
        kappan::content::build_site(src, out, drafts, kappan::output::OutDirPolicy::Refuse);
    ++builds;
    if (builds >= 2) {
      write_file(src / "content" / kappan::util::from_utf8("追加.md"),
                 "---\ntitle: 追加\n---\nbuild 中の保存\n");
    }
    return result;
  };

  auto created = kappan::serve::GenerationStore::create(std::move(builder));
  REQUIRE(created);
  auto store = std::move(*created);

  const auto first = store.publish({.source = source});
  REQUIRE(first.status == kappan::serve::PublishStatus::Activated);
  REQUIRE_FALSE(first.snapshot.entries.empty());
  REQUIRE(store.generation() == 1);

  auto acquired = store.acquire_read();
  REQUIRE(acquired);
  const auto index = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index);
  const auto workspace = acquired->root().parent_path();
  const auto gen1_root = acquired->root();

  const auto changed = store.publish({.source = source});
  REQUIRE(changed.status == kappan::serve::PublishStatus::SourceChanged);
  REQUIRE(changed.retry_required());
  REQUIRE_FALSE(changed.ok());
  REQUIRE_FALSE(changed.snapshot.entries.empty());
  REQUIRE(changed.snapshot != first.snapshot);
  REQUIRE(store.generation() == 1);
  REQUIRE(count_generation_dirs(workspace) == 1);
  REQUIRE(std::filesystem::exists(gen1_root / "index.html"));

  const auto index_again = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index_again);
  REQUIRE(*index_again == *index);

  std::filesystem::remove_all(source);
}

TEST_CASE("read lease keeps the previous generation until it is dropped", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  REQUIRE(store.publish({.source = source}).ok());

  auto first_acquired = store.acquire_read();
  REQUIRE(first_acquired);
  std::optional<kappan::serve::GenerationReadLease> held{std::move(*first_acquired)};
  REQUIRE(held->generation() == 1);
  const auto gen1_root = held->root();
  const auto gen1_index = held->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(gen1_index);
  REQUIRE(as_text(*gen1_index).find("ホーム 🐙") != std::string::npos);

  write_file(source / "content" / "index.md", "---\ntitle: ホーム改訂\n---\n# ホーム 🐙 改訂\n");

  const auto second = store.publish({.source = source});
  REQUIRE(second.status == kappan::serve::PublishStatus::Activated);
  REQUIRE(store.generation() == 2);
  REQUIRE(std::filesystem::exists(gen1_root));
  REQUIRE(std::filesystem::exists(gen1_root / "index.html"));

  const auto still_gen1 = held->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(still_gen1);
  REQUIRE(*still_gen1 == *gen1_index);
  REQUIRE(as_text(*still_gen1).find("改訂") == std::string::npos);

  auto second_acquired = store.acquire_read();
  REQUIRE(second_acquired);
  REQUIRE(second_acquired->generation() == 2);
  const auto gen2_index = second_acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(gen2_index);
  REQUIRE(as_text(*gen2_index).find("ホーム 🐙 改訂") != std::string::npos);
  REQUIRE(*gen2_index != *gen1_index);

  held.reset();
  REQUIRE_FALSE(std::filesystem::exists(gen1_root));
  REQUIRE(std::filesystem::exists(second_acquired->root()));

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_static adds and updates Japanese emoji assets", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  REQUIRE(store.publish({.source = source}).ok());
  REQUIRE(store.generation() == 1);

  const auto svg_rel = kappan::util::from_utf8("static/images/日本語🐙.svg");
  const auto out_rel = kappan::util::from_utf8("images/日本語🐙.svg");
  write_file(source / svg_rel, "<svg id=\"初版\"/>\n");

  const auto added = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Added, "static/images/日本語🐙.svg"),
      },
      source / "static");
  REQUIRE(added.empty());
  REQUIRE(store.generation() == 2);

  {
    auto acquired = store.acquire_read();
    REQUIRE(acquired);
    REQUIRE(acquired->generation() == 2);
    const auto svg = acquired->read_bytes(out_rel);
    REQUIRE(svg);
    REQUIRE(as_text(*svg) == "<svg id=\"初版\"/>\n");
    REQUIRE(std::filesystem::exists(acquired->root() / out_rel));
  }

  write_file(source / svg_rel, "<svg id=\"改訂🐙\"/>\n");
  const auto updated = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Modified, "static/images/日本語🐙.svg"),
      },
      source / "static");
  REQUIRE(updated.empty());
  REQUIRE(store.generation() == 3);

  auto acquired = store.acquire_read();
  REQUIRE(acquired);
  REQUIRE(acquired->generation() == 3);
  const auto svg = acquired->read_bytes(out_rel);
  REQUIRE(svg);
  REQUIRE(as_text(*svg) == "<svg id=\"改訂🐙\"/>\n");

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_static deletes owned static outputs only", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  REQUIRE(store.publish({.source = source}).ok());
  REQUIRE(store.generation() == 1);

  std::filesystem::path gen_root;
  {
    auto acquired = store.acquire_read();
    REQUIRE(acquired);
    gen_root = acquired->root();
    const auto wasm = acquired->read_bytes(std::filesystem::path{"bin"} / "sample.wasm");
    REQUIRE(wasm);
    REQUIRE(*wasm == wasm_payload());
  }

  const auto removed = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Removed, "static/bin/sample.wasm"),
      },
      source / "static");
  REQUIRE(removed.empty());
  REQUIRE(store.generation() == 2);
  REQUIRE_FALSE(std::filesystem::exists(gen_root / "bin" / "sample.wasm"));
  REQUIRE_FALSE(std::filesystem::exists(gen_root / "bin"));
  REQUIRE(std::filesystem::exists(gen_root / "index.html"));

  const auto unowned = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Removed, "static/index.html"),
      },
      source / "static");
  REQUIRE_FALSE(unowned.empty());
  REQUIRE(unowned.front().code == kappan::ErrorCode::Path);
  REQUIRE(store.generation() == 2);
  REQUIRE(std::filesystem::exists(gen_root / "index.html"));

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_static rejects generated page collisions without writing", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  REQUIRE(store.publish({.source = source}).ok());
  REQUIRE(store.generation() == 1);

  kappan::serve::ByteBuffer index_before;
  kappan::serve::ByteBuffer wasm_before;
  std::filesystem::path gen_root;
  {
    auto acquired = store.acquire_read();
    REQUIRE(acquired);
    gen_root = acquired->root();
    auto index = acquired->read_bytes(std::filesystem::path{"index.html"});
    REQUIRE(index);
    index_before = std::move(*index);
    auto wasm = acquired->read_bytes(std::filesystem::path{"bin"} / "sample.wasm");
    REQUIRE(wasm);
    wasm_before = std::move(*wasm);
  }

  write_file(source / "static" / "index.html", "<p>衝突</p>\n");
  const auto collided = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Added, "static/index.html"),
      },
      source / "static");
  REQUIRE_FALSE(collided.empty());
  REQUIRE(collided.front().code == kappan::ErrorCode::Path);
  REQUIRE(store.generation() == 1);

  write_file(source / "static" / "bin" / "sample.wasm", "壊す");
  write_file(source / "static" / kappan::util::from_utf8("images/日本語🐙.svg"), "<svg/>\n");
  const auto mixed = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Modified, "static/bin/sample.wasm"),
          static_change(kappan::serve::ChangeKind::Added, "static/images/日本語🐙.svg"),
          static_change(kappan::serve::ChangeKind::Added, "static/index.html"),
      },
      source / "static");
  REQUIRE_FALSE(mixed.empty());
  REQUIRE(mixed.front().code == kappan::ErrorCode::Path);
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(std::filesystem::exists(gen_root / kappan::util::from_utf8("images/日本語🐙.svg")));

  auto acquired = store.acquire_read();
  REQUIRE(acquired);
  REQUIRE(acquired->generation() == 1);
  const auto index_after = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index_after);
  REQUIRE(*index_after == index_before);
  const auto wasm_after = acquired->read_bytes(std::filesystem::path{"bin"} / "sample.wasm");
  REQUIRE(wasm_after);
  REQUIRE(*wasm_after == wasm_before);

  std::filesystem::remove_all(source);
}

TEST_CASE("apply_static rolls back earlier writes when a later copy fails", "[serve][publish]") {
  const auto source = make_japanese_site();
  auto created = kappan::serve::GenerationStore::create();
  REQUIRE(created);
  auto store = std::move(*created);

  REQUIRE(store.publish({.source = source}).ok());
  REQUIRE(store.generation() == 1);

  std::filesystem::path gen_root;
  kappan::serve::ByteBuffer index_before;
  {
    auto acquired = store.acquire_read();
    REQUIRE(acquired);
    gen_root = acquired->root();
    auto index = acquired->read_bytes(std::filesystem::path{"index.html"});
    REQUIRE(index);
    index_before = std::move(*index);
  }

  const auto svg_out = kappan::util::from_utf8("images/日本語🐙.svg");
  write_file(source / kappan::util::from_utf8("static/images/日本語🐙.svg"),
             "<svg id=\"途中\"/>\n");
  write_file(source / "static" / "index.html" / "nested.txt", "nested\n");

  const auto failed = store.apply_static(
      std::vector<kappan::serve::SourceChange>{
          static_change(kappan::serve::ChangeKind::Added, "static/images/日本語🐙.svg"),
          static_change(kappan::serve::ChangeKind::Added, "static/index.html/nested.txt"),
      },
      source / "static");
  REQUIRE_FALSE(failed.empty());
  REQUIRE(store.generation() == 1);
  REQUIRE_FALSE(std::filesystem::exists(gen_root / svg_out));
  REQUIRE(std::filesystem::is_regular_file(gen_root / "index.html"));

  auto acquired = store.acquire_read();
  REQUIRE(acquired);
  REQUIRE(acquired->generation() == 1);
  const auto index_after = acquired->read_bytes(std::filesystem::path{"index.html"});
  REQUIRE(index_after);
  REQUIRE(*index_after == index_before);

  std::filesystem::remove_all(source);
}
