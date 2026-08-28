#pragma once

#include <string>
#include <string_view>

namespace kappan::util {

// マークアップのテキストノード・属性値へ埋め込める形に変換する。
// apostrophe は ' に使う実体参照。HTML は "&#39;"、XML は "&apos;"。
// どちらも XML・HTML の両方で整形式だが、既存の出力を動かさないため呼び出し側が選ぶ。
[[nodiscard]] std::string escape_markup(std::string_view text, std::string_view apostrophe);

} // namespace kappan::util
