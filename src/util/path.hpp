#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <tl/expected.hpp>

namespace kappan::util {

[[nodiscard]] std::filesystem::path from_utf8(std::string_view utf8);

[[nodiscard]] std::string to_utf8(const std::filesystem::path &path);

[[nodiscard]] std::string to_generic_utf8(const std::filesystem::path &path);

[[nodiscard]] std::filesystem::path output_from_permalink(std::string_view permalink);

// パスの構成要素に ".." が含まれるか。字句だけを見るので、実体の有無に依存しない。
[[nodiscard]] bool contains_dotdot(const std::filesystem::path &rel);

// absolute → weakly_canonical。失敗の文面は呼び出し側が組むので、ここは error_code のまま返す。
[[nodiscard]] tl::expected<std::filesystem::path, std::error_code>
weakly_canonical_absolute(const std::filesystem::path &path);

// root/relative が root の外を指すか。解決に失敗したら「外」と見なす（フェイルセーフ）。
[[nodiscard]] bool escapes_root(const std::filesystem::path &root,
                                const std::filesystem::path &relative);

} // namespace kappan::util
