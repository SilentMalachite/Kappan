#pragma once

#include <kappan/config.hpp>
#include <kappan/document.hpp>
#include <kappan/error.hpp>

#include <filesystem>

namespace kappan::content {

[[nodiscard]] Result<Document> parse_document(const std::filesystem::path &source,
                                              const Config &config);

} // namespace kappan::content
