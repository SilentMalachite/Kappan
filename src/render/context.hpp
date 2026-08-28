#pragma once

#include <kappan/config.hpp>
#include <kappan/document.hpp>

#include <nlohmann/json.hpp>

namespace kappan::render {

[[nodiscard]] nlohmann::json make_context(const Config &config, const Document &document);

} // namespace kappan::render
