#pragma once

#include <kappan/error.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace kappan::serve {

enum class WatchKind { Config, Content, Template, Static };
enum class ChangeKind { Added, Modified, Removed };

struct SourceEntry {
  WatchKind watch_kind;
  std::filesystem::path relative;
  std::uintmax_t size = 0;
  std::filesystem::file_time_type modified;
  std::optional<std::uint64_t> digest;
  [[nodiscard]] bool operator==(const SourceEntry &) const = default;
};

struct SourceSnapshot {
  std::map<std::string, SourceEntry> entries;
  [[nodiscard]] bool operator==(const SourceSnapshot &) const = default;
};

struct SourceChange {
  WatchKind watch_kind;
  ChangeKind change_kind;
  std::filesystem::path relative;
  [[nodiscard]] bool operator==(const SourceChange &) const = default;
};

[[nodiscard]] Result<SourceSnapshot> snapshot_source(const std::filesystem::path &source);
[[nodiscard]] std::vector<SourceChange> diff_snapshots(const SourceSnapshot &before,
                                                       const SourceSnapshot &after);
[[nodiscard]] bool requires_full_publish(std::span<const SourceChange> changes);

struct WatchAttempt {
  SourceSnapshot baseline;
  SourceSnapshot target;
  std::vector<SourceChange> changes;
};

class WatchState {
public:
  explicit WatchState(SourceSnapshot initial);
  void observe(SourceSnapshot latest);
  [[nodiscard]] bool should_attempt() const;
  [[nodiscard]] WatchAttempt begin_attempt();
  void mark_activated(const SourceSnapshot &published);
  void mark_source_changed(SourceSnapshot latest);

private:
  SourceSnapshot published_;
  SourceSnapshot observed_;
  SourceSnapshot attempted_;
  bool retry_pending_ = false;
};

class WatchDebounce {
public:
  explicit WatchDebounce(std::chrono::milliseconds quiet_period);
  [[nodiscard]] bool quiet_elapsed(std::chrono::steady_clock::time_point now,
                                   const SourceSnapshot &observed);

private:
  std::chrono::milliseconds quiet_period_;
  std::optional<SourceSnapshot> last_seen_;
  std::optional<std::chrono::steady_clock::time_point> last_change_;
};

class GenerationStore;
struct PublishOptions;

void apply_watch_attempt(WatchState &state, GenerationStore &store, const WatchAttempt &attempt,
                         const PublishOptions &options);

} // namespace kappan::serve
