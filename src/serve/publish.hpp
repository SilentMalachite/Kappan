#pragma once

#include "content/build.hpp"
#include "serve/watch.hpp"

#include <kappan/error.hpp>
#include <kappan/site.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace kappan::serve {

using ByteBuffer = std::vector<std::byte>;
using SiteBuilder = std::function<content::BuildResult(
    const std::filesystem::path &source, const std::filesystem::path &out_dir, DraftPolicy drafts)>;

enum class PublishStatus { Activated, BuildFailed, SourceChanged };

struct PublishOptions {
  std::filesystem::path source;
  DraftPolicy drafts = DraftPolicy::Exclude;
};

struct PublishAttempt {
  PublishStatus status = PublishStatus::BuildFailed;
  int pages_written = 0;
  std::vector<Error> errors;
  SourceSnapshot snapshot; // post-build; set on Activated and SourceChanged
  [[nodiscard]] bool ok() const { return status == PublishStatus::Activated; }
  [[nodiscard]] bool retry_required() const { return status == PublishStatus::SourceChanged; }
};

class GenerationReadLease {
public:
  GenerationReadLease(GenerationReadLease &&) noexcept;
  GenerationReadLease &operator=(GenerationReadLease &&) noexcept;
  ~GenerationReadLease();
  GenerationReadLease(const GenerationReadLease &) = delete;
  GenerationReadLease &operator=(const GenerationReadLease &) = delete;

  [[nodiscard]] const std::filesystem::path &root() const;
  [[nodiscard]] std::uint64_t generation() const;
  [[nodiscard]] Result<ByteBuffer> read_bytes(const std::filesystem::path &relative) const;

private:
  friend class GenerationStore;
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit GenerationReadLease(std::unique_ptr<Impl> impl);
};

class GenerationStore {
public:
  [[nodiscard]] static Result<GenerationStore> create();
  [[nodiscard]] static Result<GenerationStore> create(SiteBuilder builder);
  GenerationStore(GenerationStore &&) noexcept;
  GenerationStore &operator=(GenerationStore &&) noexcept;
  ~GenerationStore();
  GenerationStore(const GenerationStore &) = delete;
  GenerationStore &operator=(const GenerationStore &) = delete;

  [[nodiscard]] PublishAttempt publish(const PublishOptions &options);
  [[nodiscard]] std::vector<Error> apply_static(std::span<const SourceChange> changes,
                                                const std::filesystem::path &static_dir);
  [[nodiscard]] Result<GenerationReadLease> acquire_read() const;
  [[nodiscard]] std::uint64_t generation() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  explicit GenerationStore(std::unique_ptr<Impl> impl);
};

} // namespace kappan::serve
