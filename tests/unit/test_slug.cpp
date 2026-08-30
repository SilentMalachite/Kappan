#include "util/slug.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string_view>

TEST_CASE("slugify keeps hiragana kanji and replaces spaces") {
  REQUIRE(kappan::util::slugify("こんにちは 世界") == "こんにちは-世界");
}

TEST_CASE("slugify keeps emoji") { REQUIRE(kappan::util::slugify("記事 🐙") == "記事-🐙"); }

TEST_CASE("slugify lowercases ASCII") { REQUIRE(kappan::util::slugify("Foo Bar") == "foo-bar"); }

TEST_CASE("slugify replaces fullwidth space") {
  REQUIRE(kappan::util::slugify("こんにちは　世界") == "こんにちは-世界");
}

TEST_CASE("slugify replaces Windows reserved characters") {
  REQUIRE(kappan::util::slugify("a/b:c*d") == "a-b-c-d");
}

TEST_CASE("slugify collapses dashes and rejects empty") {
  REQUIRE(kappan::util::slugify("  --  ") == "untitled");
  REQUIRE(kappan::util::slugify("") == "untitled");
}

TEST_CASE("try_slugify returns nullopt for reserved-only input") {
  REQUIRE_FALSE(kappan::util::try_slugify("***"));
  REQUIRE_FALSE(kappan::util::try_slugify("---"));
  const auto literal = kappan::util::try_slugify("untitled");
  REQUIRE(literal);
  REQUIRE(*literal == "untitled");
}

TEST_CASE("try_slugify rejects dot-only input") {
  REQUIRE_FALSE(kappan::util::try_slugify("."));
  REQUIRE_FALSE(kappan::util::try_slugify(".."));
  REQUIRE_FALSE(kappan::util::try_slugify("..."));
  REQUIRE(kappan::util::slugify("..") == "untitled");
}

TEST_CASE("try_slugify keeps dots inside a name") {
  const auto version = kappan::util::try_slugify("v1.2");
  REQUIRE(version);
  REQUIRE(*version == "v1.2");
  const auto leading = kappan::util::try_slugify("..記事");
  REQUIRE(leading);
  REQUIRE(*leading == "..記事");
}

TEST_CASE("slugify makes Windows device names safe") {
  struct Case {
    std::string_view input;
    std::string_view expected;
  };
  constexpr std::array<Case, 36> cases{{
      {"con", "_con"},   {"prn", "_prn"},           {"aux", "_aux"},      {"nul", "_nul"},
      {"com1", "_com1"}, {"com2", "_com2"},         {"com3", "_com3"},    {"com4", "_com4"},
      {"com5", "_com5"}, {"com6", "_com6"},         {"com7", "_com7"},    {"com8", "_com8"},
      {"com9", "_com9"}, {"lpt1", "_lpt1"},         {"lpt2", "_lpt2"},    {"lpt3", "_lpt3"},
      {"lpt4", "_lpt4"}, {"lpt5", "_lpt5"},         {"lpt6", "_lpt6"},    {"lpt7", "_lpt7"},
      {"lpt8", "_lpt8"}, {"lpt9", "_lpt9"},         {"CON", "_con"},      {"prn.txt", "_prn.txt"},
      {"AUX", "_aux"},   {"nul.", "_nul"},          {"COM1", "_com1"},    {"com9.log", "_com9.log"},
      {"LPT1", "_lpt1"}, {"lpt9.txt", "_lpt9.txt"}, {"COM0", "com0"},     {"COM10", "com10"},
      {".con", ".con"},  {"v1.2.", "v1.2"},         {"..記事", "..記事"}, {"...", "untitled"},
  }};

  for (const auto &test_case : cases) {
    CHECK(kappan::util::slugify(test_case.input) == test_case.expected);
  }
}
