#pragma once

#include <kappan/error.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace kappan::output {

using ClaimedOutputs = std::map<std::string, std::filesystem::path>;

// kappan が書いた出力先である印。空でないディレクトリを消してよいかの判断に使う（ADR-0007）。
inline constexpr std::string_view kOutMarker = ".kappan-out";

// Refuse: --out が空でなく kOutMarker も無ければ、何も消さずに拒否する。
// Force: それでも消す（--force）。ソースを守る 2 つの判定には効かない。
enum class OutDirPolicy { Refuse, Force };

[[nodiscard]] Result<void> prepare_out_dir(const std::filesystem::path &source,
                                           const std::filesystem::path &out_dir,
                                           OutDirPolicy policy = OutDirPolicy::Refuse);

// ソースが --out の内側にあるか。引数は source.lexically_relative(out)。
// 空のパスはルート名が違うことを意味する（Windows の C:\ と D:\）。
// これは「無関係」であって「同じ」でも「内側」でもない。
[[nodiscard]] bool source_inside_out(const std::filesystem::path &relative_source);

// 同じ論理パスを 2 度使わせないための共通処理。noun はメッセージに出す語（"permalink" /
// "出力先"）。
[[nodiscard]] bool claim_unique(ClaimedOutputs &claimed, std::string_view key,
                                std::string_view noun, const std::filesystem::path &source,
                                std::vector<Error> &errors);

[[nodiscard]] bool claim_output(ClaimedOutputs &claimed, std::string_view relative,
                                const std::filesystem::path &source, std::vector<Error> &errors);

// 解決後の出力先がまだ空いているかを見る。claim_output の文字列キーは大文字小文字を
// 区別するが、APFS / Windows は区別せず、APFS は Unicode 正規化もするため、
// 別のキーが同じファイルに解決されうる。書き込み・コピーの直前に必ず通すこと。
[[nodiscard]] bool claim_destination(const std::filesystem::path &dest,
                                     const std::filesystem::path &source,
                                     std::vector<Error> &errors);

} // namespace kappan::output
