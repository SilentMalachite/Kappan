#include "util/slug.hpp"

#include <catch2/catch_test_macros.hpp>

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
