#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace kappan::output {

using ClaimedOutputs = std::map<std::string, std::filesystem::path>;

[[nodiscard]] Result<void> prepare_out_dir(const std::filesystem::path &source,
                                           const std::filesystem::path &out_dir);

[[nodiscard]] bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                                const std::filesystem::path &source, std::vector<Error> &errors);

} // namespace kappan::output
