#pragma once

#include "output/write.hpp"

#include <kappan/error.hpp>

#include <filesystem>
#include <vector>

namespace kappan::output {

// options は走査失敗の扱いを変えるためのテスト用の口。既定は本番の挙動。
[[nodiscard]] std::vector<Error>
copy_static(const std::filesystem::path &static_dir, const std::filesystem::path &out_dir,
            ClaimedOutputs &claimed,
            std::filesystem::directory_options options =
                std::filesystem::directory_options::skip_permission_denied);

} // namespace kappan::output
