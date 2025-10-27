#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;

TEST_CASE("Stateful compiler: basic context operations")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  SUBCASE("Initial state")
  {
    CHECK(v4front_context_get_word_count(ctx) == 0);
    CHECK(v4front_context_get_word_name(ctx, 0) == nullptr);
    CHECK(v4front_context_find_word(ctx, "NONEXISTENT") == -1);
  }

  SUBCASE("Register single word")
  {
    v4front_err err = v4front_context_register_word(ctx, "SQUARE", 0);
    REQUIRE(err == 0);

    CHECK(v4front_context_get_word_count(ctx) == 1);
    CHECK(strcmp(v4front_context_get_word_name(ctx, 0), "SQUARE") == 0);
    CHECK(v4front_context_find_word(ctx, "SQUARE") == 0);
    CHECK(v4front_context_find_word(ctx, "square") == 0);  // Case insensitive
  }

  SUBCASE("Register multiple words")
  {
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_context_register_word(ctx, "DOUBLE", 1);
    v4front_context_register_word(ctx, "TRIPLE", 2);

    CHECK(v4front_context_get_word_count(ctx) == 3);
    CHECK(v4front_context_find_word(ctx, "SQUARE") == 0);
    CHECK(v4front_context_find_word(ctx, "DOUBLE") == 1);
    CHECK(v4front_context_find_word(ctx, "TRIPLE") == 2);
  }

  SUBCASE("Update existing word")
  {
    v4front_context_register_word(ctx, "TEST", 0);
    v4front_context_register_word(ctx, "TEST", 5);  // Update with new index

    CHECK(v4front_context_get_word_count(ctx) == 1);     // Still only one word
    CHECK(v4front_context_find_word(ctx, "TEST") == 5);  // Updated index
  }

  SUBCASE("Reset context")
  {
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_context_register_word(ctx, "DOUBLE", 1);

    v4front_context_reset(ctx);

    CHECK(v4front_context_get_word_count(ctx) == 0);
    CHECK(v4front_context_find_word(ctx, "SQUARE") == -1);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("Stateful compiler: incremental compilation")
{
  V4FrontContext* ctx = v4front_context_create();
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Define word in first compilation")
  {
    // First compilation: define SQUARE
    v4front_err err = v4front_compile_with_context(ctx, ": SQUARE DUP * ;", &buf, errmsg,
                                                   sizeof(errmsg));
    REQUIRE(err == 0);
    REQUIRE(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "SQUARE") == 0);

    // Register to context
    v4front_context_register_word(ctx, "SQUARE", 0);

    v4front_free(&buf);
  }

  SUBCASE("Use word from context in second compilation")
  {
    // First: define and register SQUARE
    v4front_compile_with_context(ctx, ": SQUARE DUP * ;", &buf, errmsg, sizeof(errmsg));
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_free(&buf);

    // Second: use SQUARE
    v4front_err err =
        v4front_compile_with_context(ctx, "5 SQUARE", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);
    CHECK(buf.word_count == 0);  // No new word definitions
    CHECK(buf.size > 0);         // Has main code

    v4front_free(&buf);
  }

  SUBCASE("Chain multiple word definitions")
  {
    // Define SQUARE
    v4front_compile_with_context(ctx, ": SQUARE DUP * ;", &buf, errmsg, sizeof(errmsg));
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_free(&buf);

    // Define QUADRUPLE using SQUARE
    v4front_err err = v4front_compile_with_context(ctx, ": QUADRUPLE SQUARE SQUARE ;",
                                                   &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);
    REQUIRE(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "QUADRUPLE") == 0);

    v4front_context_register_word(ctx, "QUADRUPLE", 1);
    v4front_free(&buf);

    // Use both words
    err = v4front_compile_with_context(ctx, "2 SQUARE QUADRUPLE", &buf, errmsg,
                                       sizeof(errmsg));
    REQUIRE(err == 0);

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("Stateful compiler: error cases")
{
  V4FrontContext* ctx = v4front_context_create();
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Unknown word without context")
  {
    v4front_err err =
        v4front_compile_with_context(ctx, "5 UNKNOWN", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);  // Should fail
    CHECK(strstr(errmsg, "unknown") != nullptr);
  }

  SUBCASE("Unknown word with context")
  {
    v4front_context_register_word(ctx, "SQUARE", 0);

    v4front_err err =
        v4front_compile_with_context(ctx, "5 UNKNOWN", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);  // Should still fail
  }

  SUBCASE("Compile without context (stateless)")
  {
    // Define word in first compilation
    v4front_compile_with_context(ctx, ": SQUARE DUP * ;", &buf, errmsg, sizeof(errmsg));
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_free(&buf);

    // Try to use word WITHOUT context (pass nullptr)
    v4front_err err =
        v4front_compile_with_context(nullptr, "5 SQUARE", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);  // Should fail - SQUARE not found without context
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("Stateful compiler: NULL safety")
{
  SUBCASE("Destroy NULL context")
  {
    v4front_context_destroy(nullptr);  // Should not crash
  }

  SUBCASE("Reset NULL context")
  {
    v4front_context_reset(nullptr);  // Should not crash
  }

  SUBCASE("Query NULL context")
  {
    CHECK(v4front_context_get_word_count(nullptr) == 0);
    CHECK(v4front_context_get_word_name(nullptr, 0) == nullptr);
    CHECK(v4front_context_find_word(nullptr, "TEST") == -1);
  }

  SUBCASE("Register with NULL context")
  {
    v4front_err err = v4front_context_register_word(nullptr, "TEST", 0);
    CHECK(err < 0);  // Should fail
  }

  SUBCASE("Register NULL name")
  {
    V4FrontContext* ctx = v4front_context_create();
    v4front_err err = v4front_context_register_word(ctx, nullptr, 0);
    CHECK(err < 0);  // Should fail
    v4front_context_destroy(ctx);
  }
}

TEST_CASE("Stateful compiler: case insensitivity")
{
  V4FrontContext* ctx = v4front_context_create();
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Register and find with different case")
  {
    v4front_context_register_word(ctx, "square", 0);

    CHECK(v4front_context_find_word(ctx, "SQUARE") == 0);
    CHECK(v4front_context_find_word(ctx, "Square") == 0);
    CHECK(v4front_context_find_word(ctx, "square") == 0);
  }

  SUBCASE("Use word with different case")
  {
    // Define with lowercase
    v4front_compile_with_context(ctx, ": square dup * ;", &buf, errmsg, sizeof(errmsg));
    v4front_context_register_word(ctx, "square", 0);
    v4front_free(&buf);

    // Use with uppercase
    v4front_err err =
        v4front_compile_with_context(ctx, "5 SQUARE", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
