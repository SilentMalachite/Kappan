#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <string>
#include <string_view>

namespace kappan::serve {

enum class ResolveKind { File, Redirect, NotFound };

struct ResolvedRequest {
  ResolveKind kind = ResolveKind::NotFound;
  std::filesystem::path file;
  std::string location;
};

[[nodiscard]] Result<ResolvedRequest> resolve_request_path(const std::filesystem::path &root,
                                                           std::string_view raw_target);
[[nodiscard]] std::string content_type_for(const std::filesystem::path &path);

} // namespace kappan::serve
