#pragma once

#include "output/write.hpp"

#include <kappan/error.hpp>

#include <filesystem>
#include <vector>

namespace kappan::output {

[[nodiscard]] std::vector<Error> copy_static(const std::filesystem::path &static_dir,
                                             const std::filesystem::path &out_dir,
                                             ClaimedOutputs &claimed);

} // namespace kappan::output
