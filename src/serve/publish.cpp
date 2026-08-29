#include "serve/publish.hpp"

#include "serve/watch.hpp"
#include "util/path.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <random>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace kappan::serve {
namespace {

struct SessionWorkspace {
  std::filesystem::path root;

  explicit SessionWorkspace(std::filesystem::path root_dir) : root(std::move(root_dir)) {}
  SessionWorkspace(const SessionWorkspace &) = delete;
  SessionWorkspace &operator=(const SessionWorkspace &) = delete;
  SessionWorkspace(SessionWorkspace &&) = delete;
  SessionWorkspace &operator=(SessionWorkspace &&) = delete;
  ~SessionWorkspace() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }
};

struct Generation {
  std::shared_ptr<SessionWorkspace> session;
  std::filesystem::path root;
  std::uint64_t number = 0;
  std::set<std::string> static_owned;
  mutable std::shared_mutex mutex;

  Generation() = default;
  Generation(const Generation &) = delete;
  Generation &operator=(const Generation &) = delete;
  Generation(Generation &&) = delete;
  Generation &operator=(Generation &&) = delete;
  ~Generation() {
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
  }
};

struct GenerationSlot {
  std::uint64_t number = 0;
  std::filesystem::path dir;
};

[[nodiscard]] Error io_error(const std::filesystem::path &where, std::string message) {
  return make_error(ErrorCode::Io, std::move(message), where);
}

[[nodiscard]] std::string random_token() {
  std::random_device device;
  std::uniform_int_distribution<std::uint32_t> dist;
  return std::format("kappan-{:08x}{:08x}{:08x}{:08x}", dist(device), dist(device), dist(device),
                     dist(device));
}

[[nodiscard]] Result<std::shared_ptr<SessionWorkspace>> create_session_workspace() {
  std::error_code ec;
  const auto tmp = std::filesystem::temp_directory_path(ec);
  if (ec) {
    return tl::unexpected(make_error(
        ErrorCode::Io, std::format("一時ディレクトリを取得できません: {}", ec.message())));
  }

  constexpr int kMaxAttempts = 64;
  for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
    const auto dir = tmp / random_token();
    std::error_code create_ec;
    if (std::filesystem::create_directory(dir, create_ec)) {
      return std::make_shared<SessionWorkspace>(dir);
    }
    if (create_ec && create_ec != std::errc::file_exists) {
      return tl::unexpected(
          io_error(dir, std::format("{}: セッション作業ディレクトリを作成できません: {}",
                                    util::to_generic_utf8(dir), create_ec.message())));
    }
  }

  return tl::unexpected(make_error(ErrorCode::Io, "セッション作業ディレクトリを作成できません"));
}

[[nodiscard]] Result<std::filesystem::path>
make_generation_dir(const std::filesystem::path &workspace, std::uint64_t number) {
  auto dir = workspace / std::format("generation-{}", number);
  std::error_code ec;
  if (!std::filesystem::create_directory(dir, ec)) {
    const auto detail = ec ? ec.message() : std::string{"既に存在します"};
    return tl::unexpected(io_error(dir, std::format("{}: 生成世代ディレクトリを作成できません: {}",
                                                    util::to_generic_utf8(dir), detail)));
  }
  return dir;
}

void reclaim_path(const std::filesystem::path &dir) noexcept {
  std::error_code ec;
  std::filesystem::remove_all(dir, ec);
}

[[nodiscard]] bool contains_dotdot(const std::filesystem::path &rel) {
  for (const auto &part : rel) {
    if (part == "..") {
      return true;
    }
  }
  return false;
}

[[nodiscard]] Result<std::filesystem::path> weakly_absolute(const std::filesystem::path &path) {
  std::error_code ec;
  const auto absolute = std::filesystem::absolute(path, ec);
  if (ec) {
    return tl::unexpected(io_error(path, std::format("{}: パスを解決できません: {}",
                                                     util::to_generic_utf8(path), ec.message())));
  }
  const auto canonical = std::filesystem::weakly_canonical(absolute, ec);
  if (ec) {
    return tl::unexpected(io_error(path, std::format("{}: パスを解決できません: {}",
                                                     util::to_generic_utf8(path), ec.message())));
  }
  return canonical;
}

[[nodiscard]] bool escapes_root(const std::filesystem::path &root,
                                const std::filesystem::path &relative) {
  if (relative.empty() || relative.has_root_path() || contains_dotdot(relative)) {
    return true;
  }
  const auto root_abs = weakly_absolute(root);
  const auto dest_abs = weakly_absolute(root / relative);
  if (!root_abs || !dest_abs) {
    return true;
  }
  const auto rel = dest_abs->lexically_relative(*root_abs);
  return rel.empty() || rel.has_root_path() || contains_dotdot(rel);
}

[[nodiscard]] Result<ByteBuffer> read_all_bytes(const std::filesystem::path &path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return tl::unexpected(
        io_error(path, std::format("{}: 読み込めません", util::to_generic_utf8(path))));
  }
  const std::string raw{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
  ByteBuffer bytes(raw.size());
  std::ranges::transform(raw, bytes.begin(), [](char c) {
    return static_cast<std::byte>(static_cast<unsigned char>(c));
  });
  return bytes;
}

[[nodiscard]] SiteBuilder default_site_builder() {
  return [](const std::filesystem::path &source, const std::filesystem::path &out_dir,
            DraftPolicy drafts) {
    return content::build_site(source, out_dir, drafts, output::OutDirPolicy::Refuse);
  };
}

[[nodiscard]] PublishAttempt reject_generation(PublishStatus status, int pages_written,
                                               std::vector<Error> errors,
                                               const std::filesystem::path &dir,
                                               SourceSnapshot snapshot = {}) {
  reclaim_path(dir);
  return PublishAttempt{status, pages_written, std::move(errors), std::move(snapshot)};
}

[[nodiscard]] Error path_error(const std::filesystem::path &where, std::string message) {
  return make_error(ErrorCode::Path, std::move(message), where);
}

[[nodiscard]] bool is_out_marker_key(std::string_view key) {
  return key == ".kappan-out" || key.ends_with("/.kappan-out");
}

[[nodiscard]] std::optional<std::filesystem::path>
static_to_output_relative(const std::filesystem::path &relative) {
  const auto generic = util::to_generic_utf8(relative);
  constexpr std::string_view kPrefix{"static/"};
  if (!generic.starts_with(kPrefix)) {
    return std::nullopt;
  }
  auto out = util::from_utf8(generic.substr(kPrefix.size()));
  if (out.empty() || out.has_root_path() || contains_dotdot(out) || out == ".") {
    return std::nullopt;
  }
  return out;
}

[[nodiscard]] std::set<std::string> static_ownership_from(const SourceSnapshot &snapshot) {
  std::set<std::string> owned;
  for (const auto &item : snapshot.entries) {
    const auto &entry = item.second;
    if (entry.watch_kind != WatchKind::Static) {
      continue;
    }
    const auto output = static_to_output_relative(entry.relative);
    if (!output) {
      continue;
    }
    auto key = util::to_generic_utf8(*output);
    if (is_out_marker_key(key)) {
      continue;
    }
    owned.insert(std::move(key));
  }
  return owned;
}

struct StaticPlan {
  ChangeKind change_kind = ChangeKind::Modified;
  std::filesystem::path relative;
  std::filesystem::path output_relative;
  std::string key;
  std::filesystem::path source;
  std::filesystem::path dest;
  bool dest_existed = false;
};

struct AppliedStatic {
  std::filesystem::path dest;
  std::filesystem::path temp;
  std::filesystem::path backup;
  bool placed = false;
};

void prune_empty_ancestors(const std::filesystem::path &root, std::filesystem::path dir) noexcept {
  std::error_code ec;
  while (true) {
    const auto root_abs = weakly_absolute(root);
    const auto dir_abs = weakly_absolute(dir);
    if (!root_abs || !dir_abs || *dir_abs == *root_abs) {
      return;
    }
    const auto rel = dir_abs->lexically_relative(*root_abs);
    if (rel.empty() || rel == "." || rel.has_root_path() || contains_dotdot(rel)) {
      return;
    }
    if (!std::filesystem::is_directory(dir, ec) || ec) {
      return;
    }
    if (!std::filesystem::is_empty(dir, ec) || ec) {
      return;
    }
    std::filesystem::remove(dir, ec);
    if (ec) {
      return;
    }
    dir = dir.parent_path();
  }
}

void rollback_applied(std::span<AppliedStatic> applied) noexcept {
  std::error_code ec;
  for (auto it = applied.rbegin(); it != applied.rend(); ++it) {
    if (it->placed) {
      std::filesystem::remove(it->dest, ec);
    }
    if (!it->backup.empty()) {
      std::filesystem::rename(it->backup, it->dest, ec);
    }
    if (!it->temp.empty()) {
      std::filesystem::remove(it->temp, ec);
    }
  }
}

void discard_backups(std::span<AppliedStatic> applied) noexcept {
  std::error_code ec;
  for (auto &op : applied) {
    if (!op.backup.empty()) {
      std::filesystem::remove(op.backup, ec);
    }
  }
}

[[nodiscard]] Result<std::filesystem::file_status> query_status(const std::filesystem::path &path) {
  std::error_code ec;
  auto status = std::filesystem::status(path, ec);
  if (!std::filesystem::status_known(status)) {
    return tl::unexpected(
        io_error(path, std::format("{}: 種別を判定できません: {}", util::to_generic_utf8(path),
                                   ec ? ec.message() : std::string{"種別が不明です"})));
  }
  return status;
}

[[nodiscard]] Result<void> apply_write(const StaticPlan &plan, AppliedStatic &applied) {
  applied.dest = plan.dest;
  std::error_code ec;
  const auto parent = plan.dest.parent_path();
  std::filesystem::create_directories(parent, ec);
  if (ec) {
    return tl::unexpected(
        io_error(parent, std::format("{}: 出力先を作成できません: {}",
                                     util::to_generic_utf8(parent), ec.message())));
  }

  applied.temp = parent / std::format(".kappan-tmp-{}", random_token());
  std::filesystem::copy_file(plan.source, applied.temp, ec);
  if (ec) {
    return tl::unexpected(
        io_error(plan.source, std::format("{}: コピーできません: {}",
                                          util::to_generic_utf8(plan.source), ec.message())));
  }

  if (plan.dest_existed) {
    applied.backup = parent / std::format(".kappan-bak-{}", random_token());
    std::filesystem::rename(plan.dest, applied.backup, ec);
    if (ec) {
      return tl::unexpected(
          io_error(plan.dest, std::format("{}: 退避できません: {}",
                                          util::to_generic_utf8(plan.dest), ec.message())));
    }
  }

  std::filesystem::rename(applied.temp, plan.dest, ec);
  if (ec) {
    return tl::unexpected(
        io_error(plan.dest, std::format("{}: 配置できません: {}", util::to_generic_utf8(plan.dest),
                                        ec.message())));
  }
  applied.temp.clear();
  applied.placed = true;
  return {};
}

[[nodiscard]] Result<void> apply_remove(const StaticPlan &plan, AppliedStatic &applied) {
  applied.dest = plan.dest;
  if (!plan.dest_existed) {
    applied.placed = true;
    return {};
  }
  std::error_code ec;
  applied.backup = plan.dest.parent_path() / std::format(".kappan-bak-{}", random_token());
  std::filesystem::rename(plan.dest, applied.backup, ec);
  if (ec) {
    return tl::unexpected(
        io_error(plan.dest, std::format("{}: 削除できません: {}", util::to_generic_utf8(plan.dest),
                                        ec.message())));
  }
  applied.placed = true;
  return {};
}

void plan_change(const SourceChange &change, const std::filesystem::path &static_dir,
                 const Generation &generation, std::set<std::string> &seen,
                 std::vector<StaticPlan> &plans, std::vector<Error> &errors) {
  if (change.watch_kind != WatchKind::Static) {
    errors.push_back(
        path_error(change.relative, std::format("{}: static 以外の変更は適用できません",
                                                util::to_generic_utf8(change.relative))));
    return;
  }

  const auto output = static_to_output_relative(change.relative);
  if (!output || escapes_root(generation.root, *output) || escapes_root(static_dir, *output)) {
    errors.push_back(
        path_error(change.relative, std::format("{}: static の出力先が生成世代の外です",
                                                util::to_generic_utf8(change.relative))));
    return;
  }

  StaticPlan plan;
  plan.change_kind = change.change_kind;
  plan.relative = change.relative;
  plan.output_relative = *output;
  plan.key = util::to_generic_utf8(*output);
  plan.dest = generation.root / plan.output_relative;
  plan.source = static_dir / plan.output_relative;

  if (is_out_marker_key(plan.key) || !seen.insert(plan.key).second) {
    errors.push_back(
        path_error(change.relative, std::format("{}: static の出力先が生成ページと衝突します",
                                                util::to_generic_utf8(change.relative))));
    return;
  }

  auto dest_st = query_status(plan.dest);
  if (!dest_st) {
    errors.push_back(dest_st.error());
    return;
  }
  plan.dest_existed = std::filesystem::exists(*dest_st);
  const bool owned = generation.static_owned.contains(plan.key);
  const bool generated = plan.dest_existed && !owned;

  if (change.change_kind == ChangeKind::Removed) {
    if (!owned) {
      errors.push_back(
          path_error(change.relative, std::format("{}: 所有していない出力は削除できません",
                                                  util::to_generic_utf8(change.relative))));
      return;
    }
    plans.push_back(std::move(plan));
    return;
  }

  if (generated || (plan.dest_existed && !std::filesystem::is_regular_file(*dest_st))) {
    errors.push_back(
        path_error(change.relative, std::format("{}: static の出力先が生成ページと衝突します",
                                                util::to_generic_utf8(change.relative))));
    return;
  }

  auto src_st = query_status(plan.source);
  if (!src_st) {
    errors.push_back(src_st.error());
    return;
  }
  if (!std::filesystem::is_regular_file(*src_st)) {
    errors.push_back(io_error(plan.source, std::format("{}: 通常ファイルではありません",
                                                       util::to_generic_utf8(plan.source))));
    return;
  }
  plans.push_back(std::move(plan));
}

} // namespace

struct GenerationReadLease::Impl {
  std::shared_ptr<Generation> generation;
  std::shared_lock<std::shared_mutex> read_lock;
};

struct GenerationStore::Impl {
  std::shared_ptr<SessionWorkspace> session;
  std::shared_ptr<Generation> current;
  std::uint64_t generation = 0;
  std::uint64_t next_id = 1;
  mutable std::shared_mutex state_mutex;
  std::mutex publish_mutex;
  SiteBuilder builder;

  [[nodiscard]] Result<GenerationSlot> allocate();
  void activate(GenerationSlot slot, const SourceSnapshot &snapshot);
  [[nodiscard]] std::vector<Error> apply_static(std::span<const SourceChange> changes,
                                                const std::filesystem::path &static_dir);
};

GenerationReadLease::GenerationReadLease(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
GenerationReadLease::GenerationReadLease(GenerationReadLease &&) noexcept = default;
GenerationReadLease &GenerationReadLease::operator=(GenerationReadLease &&) noexcept = default;
GenerationReadLease::~GenerationReadLease() = default;

const std::filesystem::path &GenerationReadLease::root() const { return impl_->generation->root; }

std::uint64_t GenerationReadLease::generation() const { return impl_->generation->number; }

Result<ByteBuffer> GenerationReadLease::read_bytes(const std::filesystem::path &relative) const {
  const auto &root = impl_->generation->root;
  if (escapes_root(root, relative)) {
    return tl::unexpected(
        make_error(ErrorCode::Path,
                   std::format("{}: 生成世代の外を参照しています", util::to_generic_utf8(relative)),
                   relative));
  }
  return read_all_bytes(root / relative);
}

GenerationStore::GenerationStore(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
GenerationStore::GenerationStore(GenerationStore &&) noexcept = default;
GenerationStore &GenerationStore::operator=(GenerationStore &&) noexcept = default;
GenerationStore::~GenerationStore() = default;

Result<GenerationStore> GenerationStore::create() { return create(default_site_builder()); }

Result<GenerationStore> GenerationStore::create(SiteBuilder builder) {
  auto session = create_session_workspace();
  if (!session) {
    return tl::unexpected(session.error());
  }
  auto impl = std::make_unique<Impl>();
  impl->session = std::move(*session);
  impl->builder = std::move(builder);
  return GenerationStore(std::move(impl));
}

Result<GenerationSlot> GenerationStore::Impl::allocate() {
  std::unique_lock state(state_mutex);
  GenerationSlot slot;
  slot.number = next_id++;
  auto dir = make_generation_dir(session->root, slot.number);
  if (!dir) {
    return tl::unexpected(dir.error());
  }
  slot.dir = std::move(*dir);
  return slot;
}

void GenerationStore::Impl::activate(GenerationSlot slot, const SourceSnapshot &snapshot) {
  auto next = std::make_shared<Generation>();
  next->session = session;
  next->root = std::move(slot.dir);
  next->number = slot.number;
  next->static_owned = static_ownership_from(snapshot);

  std::unique_lock state(state_mutex);
  auto previous = std::move(current);
  current = std::move(next);
  generation = slot.number;
  state.unlock();
  previous.reset();
}

PublishAttempt GenerationStore::publish(const PublishOptions &options) {
  std::lock_guard<std::mutex> publish_lock(impl_->publish_mutex);

  auto pre = snapshot_source(options.source);
  if (!pre) {
    return PublishAttempt{PublishStatus::BuildFailed, 0, {pre.error()}, {}};
  }

  auto slot = impl_->allocate();
  if (!slot) {
    return PublishAttempt{PublishStatus::BuildFailed, 0, {slot.error()}, {}};
  }

  auto result = impl_->builder(options.source, slot->dir, options.drafts);

  auto post = snapshot_source(options.source);
  if (!post) {
    auto errors = std::move(result.errors);
    errors.push_back(post.error());
    return reject_generation(PublishStatus::BuildFailed, result.pages_written, std::move(errors),
                             slot->dir);
  }
  if (*pre != *post) {
    return reject_generation(PublishStatus::SourceChanged, result.pages_written,
                             std::move(result.errors), slot->dir, std::move(*post));
  }
  if (!result.errors.empty()) {
    return reject_generation(PublishStatus::BuildFailed, result.pages_written,
                             std::move(result.errors), slot->dir);
  }

  const auto pages_written = result.pages_written;
  impl_->activate(std::move(*slot), *pre);
  return PublishAttempt{PublishStatus::Activated, pages_written, {}, std::move(*pre)};
}

Result<GenerationReadLease> GenerationStore::acquire_read() const {
  std::shared_lock state(impl_->state_mutex);
  if (!impl_->current) {
    return tl::unexpected(io_error(impl_->session->root, "配信できる生成世代がありません"));
  }
  auto lease_impl = std::make_unique<GenerationReadLease::Impl>();
  lease_impl->generation = impl_->current;
  lease_impl->read_lock = std::shared_lock<std::shared_mutex>(impl_->current->mutex);
  return GenerationReadLease(std::move(lease_impl));
}

std::uint64_t GenerationStore::generation() const {
  std::shared_lock state(impl_->state_mutex);
  return impl_->generation;
}

std::vector<Error> GenerationStore::apply_static(std::span<const SourceChange> changes,
                                                 const std::filesystem::path &static_dir) {
  return impl_->apply_static(changes, static_dir);
}

std::vector<Error> GenerationStore::Impl::apply_static(std::span<const SourceChange> changes,
                                                       const std::filesystem::path &static_dir) {
  std::lock_guard<std::mutex> publish_lock(publish_mutex);
  std::unique_lock state(state_mutex);
  if (!current) {
    return {io_error(session->root, "配信できる生成世代がありません")};
  }
  auto gen = current;
  std::unique_lock write_lock(gen->mutex);

  std::vector<StaticPlan> plans;
  std::vector<Error> errors;
  std::set<std::string> seen;
  for (const auto &change : changes) {
    plan_change(change, static_dir, *gen, seen, plans, errors);
  }
  if (!errors.empty() || plans.empty()) {
    return errors;
  }

  std::vector<AppliedStatic> applied;
  applied.reserve(plans.size());
  for (const auto &plan : plans) {
    auto &op = applied.emplace_back();
    const auto step =
        plan.change_kind == ChangeKind::Removed ? apply_remove(plan, op) : apply_write(plan, op);
    if (!step) {
      rollback_applied(applied);
      return {step.error()};
    }
  }

  discard_backups(applied);
  for (const auto &plan : plans) {
    if (plan.change_kind == ChangeKind::Removed) {
      gen->static_owned.erase(plan.key);
      prune_empty_ancestors(gen->root, plan.dest.parent_path());
    } else {
      gen->static_owned.insert(plan.key);
    }
  }

  gen->number += 1;
  generation = gen->number;
  if (next_id <= generation) {
    next_id = generation + 1;
  }
  return {};
}

} // namespace kappan::serve
