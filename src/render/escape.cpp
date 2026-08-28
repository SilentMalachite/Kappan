#include "render/escape.hpp"

#include "util/escape.hpp"

namespace kappan::render {

std::string html_escape(std::string_view text) { return util::escape_markup(text, "&#39;"); }

} // namespace kappan::render
