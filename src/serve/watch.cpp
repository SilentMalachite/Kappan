#include "serve/watch.hpp"

#include "serve/publish.hpp"
#include "util/path.hpp"

#include <spdlog/spdlog.h>

#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

namespace kappan::serve {
namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

[[nodiscard]] Error io_error(const std::filesystem::path &where, std::string message) {
  return make_error(ErrorCode::Io, std::move(message), where);
}

[[nodiscard]] std::uint64_t fnv1a64(std::string_view bytes) {
  std::uint64_t hash = kFnvOffsetBasis;
  for (const char byte : bytes) {
    hash ^= static_cast<unsigned char>(byte);
    hash *= kFnvPrime;
  }
  return hash;
}

[[nodiscard]] Result<std::string> read_bytes(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return tl::unexpected(
        io_error(path, std::format("{}: 読み込めません", util::to_generic_utf8(path))));
  }
  return std::string{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

[[nodiscard]] Result<SourceEntry> make_entry(WatchKind kind, const std::filesystem::path &absolute,
                                             const std::filesystem::path &relative,
                                             bool with_digest) {
  std::error_code ec;
  const auto size = std::filesystem::file_size(absolute, ec);
  if (ec) {
    return tl::unexpected(
        io_error(absolute, std::format("{}: サイズを取得できません: {}",
                                       util::to_generic_utf8(absolute), ec.message())));
  }
  const auto modified = std::filesystem::last_write_time(absolute, ec);
  if (ec) {
    return tl::unexpected(
        io_error(absolute, std::format("{}: 更新時刻を取得できません: {}",
                                       util::to_generic_utf8(absolute), ec.message())));
  }

  SourceEntry entry{
      .watch_kind = kind,
      .relative = relative,
      .size = size,
      .modified = modified,
      .digest = std::nullopt,
  };
  if (with_digest) {
    auto bytes = read_bytes(absolute);
    if (!bytes) {
      return tl::unexpected(bytes.error());
    }
    entry.digest = fnv1a64(*bytes);
  }
  return entry;
}

[[nodiscard]] Result<void> insert_entry(SourceSnapshot &snap, WatchKind kind,
                                        const std::filesystem::path &absolute,
                                        const std::filesystem::path &relative, bool with_digest) {
  auto entry = make_entry(kind, absolute, relative, with_digest);
  if (!entry) {
    return tl::unexpected(entry.error());
  }
  snap.entries.emplace(util::to_generic_utf8(relative), std::move(*entry));
  return {};
}

[[nodiscard]] Result<void> add_site_yaml(SourceSnapshot &snap,
                                         const std::filesystem::path &source) {
  const auto path = source / "site.yaml";
  std::error_code ec;
  const auto st = std::filesystem::status(path, ec);
  if (!std::filesystem::status_known(st)) {
    return tl::unexpected(
        io_error(path, std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(path),
                                   ec ? ec.message() : std::string{"種別が不明です"})));
  }
  if (!std::filesystem::exists(st)) {
    return {};
  }
  if (!std::filesystem::is_regular_file(st)) {
    return {};
  }
  return insert_entry(snap, WatchKind::Config, path, std::filesystem::path{"site.yaml"}, true);
}

[[nodiscard]] Result<void> add_content(SourceSnapshot &snap, const std::filesystem::path &source) {
  const auto content_dir = source / "content";
  std::error_code ec;
  const auto dir_status = std::filesystem::status(content_dir, ec);
  if (!std::filesystem::status_known(dir_status)) {
    return tl::unexpected(
        io_error(content_dir, std::format("{}: 走査できません: {}",
                                          util::to_generic_utf8(content_dir), ec.message())));
  }
  if (!std::filesystem::exists(dir_status) || !std::filesystem::is_directory(dir_status)) {
    return {};
  }

  const auto options = std::filesystem::directory_options::skip_permission_denied;
  auto it = std::filesystem::recursive_directory_iterator(content_dir, options, ec);
  if (ec) {
    return tl::unexpected(
        io_error(content_dir, std::format("{}: 走査できません: {}",
                                          util::to_generic_utf8(content_dir), ec.message())));
  }

  const std::filesystem::recursive_directory_iterator end;
  while (it != end) {
    std::error_code type_ec;
    const auto entry_status = it->status(type_ec);
    if (!std::filesystem::status_known(entry_status)) {
      return tl::unexpected(io_error(
          it->path(), std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(it->path()),
                                  type_ec ? type_ec.message() : std::string{"種別が不明です"})));
    }

    const auto name = util::to_utf8(it->path().filename());
    if (std::filesystem::is_directory(entry_status) && name.starts_with('_')) {
      it.disable_recursion_pending();
    } else if (std::filesystem::is_regular_file(entry_status) && it->path().extension() == ".md") {
      const auto rel = std::filesystem::relative(it->path(), source, ec);
      if (ec || rel.empty() || rel == ".") {
        return tl::unexpected(
            io_error(it->path(), std::format("{}: 相対パスを求められません: {}",
                                             util::to_generic_utf8(it->path()),
                                             ec ? ec.message() : std::string{"空です"})));
      }
      auto inserted = insert_entry(snap, WatchKind::Content, it->path(), rel, true);
      if (!inserted) {
        return inserted;
      }
    }

    it.increment(ec);
    if (ec) {
      return tl::unexpected(
          io_error(content_dir, std::format("{}: 走査できません: {}",
                                            util::to_generic_utf8(content_dir), ec.message())));
    }
  }
  return {};
}

[[nodiscard]] Result<void> add_templates(SourceSnapshot &snap,
                                         const std::filesystem::path &source) {
  const auto templates_dir = source / "templates";
  std::error_code ec;
  if (!std::filesystem::is_directory(templates_dir, ec)) {
    return {};
  }

  auto it = std::filesystem::directory_iterator(templates_dir, ec);
  if (ec) {
    return tl::unexpected(
        io_error(templates_dir, std::format("{}: テンプレートを走査できません: {}",
                                            util::to_generic_utf8(templates_dir), ec.message())));
  }

  const std::filesystem::directory_iterator end;
  while (it != end) {
    std::error_code type_ec;
    const auto entry_status = it->status(type_ec);
    if (!std::filesystem::status_known(entry_status)) {
      return tl::unexpected(io_error(
          it->path(), std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(it->path()),
                                  type_ec ? type_ec.message() : std::string{"種別が不明です"})));
    }
    if (std::filesystem::is_regular_file(entry_status) && it->path().extension() == ".html") {
      const auto rel = std::filesystem::path{"templates"} / it->path().filename();
      auto inserted = insert_entry(snap, WatchKind::Template, it->path(), rel, true);
      if (!inserted) {
        return inserted;
      }
    }

    it.increment(ec);
    if (ec) {
      return tl::unexpected(
          io_error(templates_dir, std::format("{}: テンプレートを走査できません: {}",
                                              util::to_generic_utf8(templates_dir), ec.message())));
    }
  }
  return {};
}

[[nodiscard]] Result<void> add_static(SourceSnapshot &snap, const std::filesystem::path &source) {
  const auto static_dir = source / "static";
  std::error_code ec;
  const auto dir_status = std::filesystem::status(static_dir, ec);
  if (!std::filesystem::status_known(dir_status)) {
    return tl::unexpected(
        io_error(static_dir, std::format("{}: 走査できません: {}",
                                         util::to_generic_utf8(static_dir), ec.message())));
  }
  if (!std::filesystem::exists(dir_status) || !std::filesystem::is_directory(dir_status)) {
    return {};
  }

  const auto options = std::filesystem::directory_options::skip_permission_denied;
  auto it = std::filesystem::recursive_directory_iterator(static_dir, options, ec);
  if (ec) {
    return tl::unexpected(
        io_error(static_dir, std::format("{}: 走査できません: {}",
                                         util::to_generic_utf8(static_dir), ec.message())));
  }

  const std::filesystem::recursive_directory_iterator end;
  while (it != end) {
    std::error_code type_ec;
    const auto entry_status = it->status(type_ec);
    if (!std::filesystem::status_known(entry_status)) {
      return tl::unexpected(io_error(
          it->path(), std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(it->path()),
                                  type_ec ? type_ec.message() : std::string{"種別が不明です"})));
    }

    const auto name = util::to_utf8(it->path().filename());
    const bool is_dir = std::filesystem::is_directory(entry_status);
    if (name.starts_with('.') || (is_dir && name.starts_with('_'))) {
      if (is_dir) {
        it.disable_recursion_pending();
      }
    } else if (std::filesystem::is_regular_file(entry_status)) {
      const auto rel = std::filesystem::relative(it->path(), source, ec);
      if (ec || rel.empty() || rel == ".") {
        return tl::unexpected(
            io_error(it->path(), std::format("{}: 相対パスを求められません: {}",
                                             util::to_generic_utf8(it->path()),
                                             ec ? ec.message() : std::string{"空です"})));
      }
      auto inserted = insert_entry(snap, WatchKind::Static, it->path(), rel, false);
      if (!inserted) {
        return inserted;
      }
    }

    it.increment(ec);
    if (ec) {
      return tl::unexpected(
          io_error(static_dir, std::format("{}: 走査できません: {}",
                                           util::to_generic_utf8(static_dir), ec.message())));
    }
  }
  return {};
}

} // namespace

Result<SourceSnapshot> snapshot_source(const std::filesystem::path &source) {
  std::error_code ec;
  const auto st = std::filesystem::status(source, ec);
  if (!std::filesystem::status_known(st)) {
    return tl::unexpected(
        io_error(source, std::format("{}: 走査できません: {}", util::to_generic_utf8(source),
                                     ec ? ec.message() : std::string{"種別が不明です"})));
  }
  if (!std::filesystem::exists(st) || !std::filesystem::is_directory(st)) {
    return tl::unexpected(io_error(
        source, std::format("{}: ソースディレクトリがありません", util::to_generic_utf8(source))));
  }

  SourceSnapshot snap;
  if (auto r = add_site_yaml(snap, source); !r) {
    return tl::unexpected(r.error());
  }
  if (auto r = add_content(snap, source); !r) {
    return tl::unexpected(r.error());
  }
  if (auto r = add_templates(snap, source); !r) {
    return tl::unexpected(r.error());
  }
  if (auto r = add_static(snap, source); !r) {
    return tl::unexpected(r.error());
  }
  return snap;
}

std::vector<SourceChange> diff_snapshots(const SourceSnapshot &before,
                                         const SourceSnapshot &after) {
  std::vector<SourceChange> changes;

  for (const auto &[key, entry] : before.entries) {
    const auto it = after.entries.find(key);
    if (it == after.entries.end()) {
      changes.push_back(SourceChange{entry.watch_kind, ChangeKind::Removed, entry.relative});
    } else if (it->second != entry) {
      changes.push_back(
          SourceChange{it->second.watch_kind, ChangeKind::Modified, it->second.relative});
    }
  }
  for (const auto &[key, entry] : after.entries) {
    if (!before.entries.contains(key)) {
      changes.push_back(SourceChange{entry.watch_kind, ChangeKind::Added, entry.relative});
    }
  }
  return changes;
}

bool requires_full_publish(std::span<const SourceChange> changes) {
  for (const auto &change : changes) {
    if (change.watch_kind != WatchKind::Static) {
      return true;
    }
  }
  return false;
}

WatchState::WatchState(SourceSnapshot initial)
    : published_(std::move(initial)), observed_(published_), attempted_(published_) {}

void WatchState::observe(SourceSnapshot latest) { observed_ = std::move(latest); }

bool WatchState::should_attempt() const {
  return observed_ != published_ && (retry_pending_ || observed_ != attempted_);
}

WatchAttempt WatchState::begin_attempt() {
  attempted_ = observed_;
  retry_pending_ = false;
  return WatchAttempt{
      .baseline = published_,
      .target = attempted_,
      .changes = diff_snapshots(published_, attempted_),
  };
}

void WatchState::mark_activated(const SourceSnapshot &published) { published_ = published; }

void WatchState::mark_source_changed(SourceSnapshot latest) {
  observed_ = std::move(latest);
  retry_pending_ = true;
}

WatchDebounce::WatchDebounce(std::chrono::milliseconds quiet_period)
    : quiet_period_(quiet_period) {}

bool WatchDebounce::quiet_elapsed(std::chrono::steady_clock::time_point now,
                                  const SourceSnapshot &observed) {
  if (!last_seen_.has_value()) {
    last_seen_ = observed;
    return false;
  }
  if (observed != *last_seen_) {
    last_seen_ = observed;
    last_change_ = now;
  }
  if (!last_change_.has_value()) {
    return false;
  }
  return now - *last_change_ >= quiet_period_;
}

void apply_watch_attempt(WatchState &state, GenerationStore &store, const WatchAttempt &attempt,
                         const PublishOptions &options) {
  WatchAttempt current = attempt;
  for (;;) {
    if (!requires_full_publish(current.changes)) {
      const auto errors = store.apply_static(current.changes, options.source / "static");
      if (errors.empty()) {
        state.mark_activated(current.target);
      } else {
        for (const auto &error : errors) {
          spdlog::error("{}", error.message);
        }
      }
      return;
    }

    const auto result = store.publish(options);
    if (result.status == PublishStatus::Activated) {
      state.mark_activated(result.snapshot);
      return;
    }
    if (result.status == PublishStatus::BuildFailed) {
      for (const auto &error : result.errors) {
        spdlog::error("{}", error.message);
      }
      return;
    }

    auto latest = snapshot_source(options.source);
    if (!latest) {
      spdlog::error("{}", latest.error().message);
      return;
    }
    state.mark_source_changed(std::move(*latest));
    if (!state.should_attempt()) {
      return;
    }
    current = state.begin_attempt();
  }
}

} // namespace kappan::serve
