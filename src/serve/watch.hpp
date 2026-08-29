#pragma once

#include <kappan/error.hpp>

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
};

[[nodiscard]] Result<SourceSnapshot> snapshot_source(const std::filesystem::path &source);
[[nodiscard]] std::vector<SourceChange> diff_snapshots(const SourceSnapshot &before,
                                                       const SourceSnapshot &after);
[[nodiscard]] bool requires_full_publish(std::span<const SourceChange> changes);

} // namespace kappan::serve
