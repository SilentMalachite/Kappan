#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <vector>

namespace kappan::content {

struct ScanResult {
  std::vector<std::filesystem::path> files{};
  // 個々のエントリの失敗。走査自体は続行している
  std::vector<Error> errors{};
};

// options は走査失敗の扱いを変えるためのテスト用の口。既定は本番の挙動。
[[nodiscard]] Result<ScanResult>
scan_markdown(const std::filesystem::path &content_dir,
              std::filesystem::directory_options options =
                  std::filesystem::directory_options::skip_permission_denied);

} // namespace kappan::content
