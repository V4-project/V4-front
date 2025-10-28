#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;

TEST_CASE("Error position tracking: basic errors")
{
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("Unknown token")
  {
    const char* source = "1 2 UNKNOWN +";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnknownToken));
    CHECK(error.position == 4);  // Position of "UNKNOWN"
    CHECK(error.line == 1);
    CHECK(error.column == 5);
    CHECK(strcmp(error.token, "UNKNOWN") == 0);
    CHECK(strcmp(error.context, "1 2 UNKNOWN +") == 0);
  }

  SUBCASE("Unknown token on second line")
  {
    const char* source = "1 2 +\nFOO BAR";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.line == 2);
    CHECK(error.column == 1);
    CHECK(strcmp(error.token, "FOO") == 0);
    CHECK(strcmp(error.context, "FOO BAR") == 0);
  }

  SUBCASE("Error at different column")
  {
    const char* source = "1 2 3 BADTOKEN 5";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.position == 6);
    CHECK(error.column == 7);
    CHECK(strcmp(error.token, "BADTOKEN") == 0);
  }
}

TEST_CASE("Error position tracking: control flow errors")
{
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("THEN without IF")
  {
    const char* source = "1 2 THEN +";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::ThenWithoutIf));
    CHECK(error.position == 4);
    CHECK(strcmp(error.token, "THEN") == 0);
  }

  SUBCASE("UNTIL without BEGIN")
  {
    const char* source = "1 2 UNTIL";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UntilWithoutBegin));
    CHECK(strcmp(error.token, "UNTIL") == 0);
  }

  SUBCASE("LOOP without DO")
  {
    const char* source = "1 2 LOOP";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::LoopWithoutDo));
    CHECK(strcmp(error.token, "LOOP") == 0);
  }

  SUBCASE("Unclosed IF")
  {
    const char* source = "1 IF 2 +";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnclosedIf));
    // Position should be at end of source
    CHECK(error.position >= 0);
  }

  SUBCASE("Unclosed BEGIN")
  {
    const char* source = "BEGIN 1 2 +";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnclosedBegin));
  }

  SUBCASE("Unclosed DO")
  {
    const char* source = "5 0 DO I";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnclosedDo));
  }
}

TEST_CASE("Error position tracking: word definition errors")
{
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("Colon without name")
  {
    const char* source = ": ";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::ColonWithoutName));
  }

  SUBCASE("Semicolon without colon")
  {
    const char* source = "1 2 + ;";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::SemicolonWithoutColon));
    CHECK(strcmp(error.token, ";") == 0);
  }

  SUBCASE("Unclosed colon")
  {
    const char* source = ": SQUARE DUP *";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnclosedColon));
  }

  SUBCASE("Duplicate word")
  {
    const char* source = ": SQUARE DUP * ; : SQUARE DUP * ;";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::DuplicateWord));
    CHECK(strcmp(error.token, "SQUARE") == 0);
    v4front_free(&buf);
  }
}

TEST_CASE("Error formatting: v4front_format_error")
{
  V4FrontBuf buf;
  V4FrontError error;
  char formatted[512];

  SUBCASE("Format unknown token error")
  {
    const char* source = "1 2 UNKNOWN +";
    v4front_compile_ex(source, &buf, &error);
    v4front_format_error(&error, source, formatted, sizeof(formatted));

    // Check that formatted message contains expected elements
    CHECK(strstr(formatted, "unknown token") != nullptr);
    CHECK(strstr(formatted, "line 1") != nullptr);
    CHECK(strstr(formatted, "column 5") != nullptr);
    CHECK(strstr(formatted, "1 2 UNKNOWN +") != nullptr);
    CHECK(strstr(formatted, "^") != nullptr);  // Caret indicator
  }

  SUBCASE("Format multiline error")
  {
    const char* source = "1 2 +\n3 4 BADWORD";
    v4front_compile_ex(source, &buf, &error);
    v4front_format_error(&error, source, formatted, sizeof(formatted));

    CHECK(strstr(formatted, "line 2") != nullptr);
    CHECK(strstr(formatted, "3 4 BADWORD") != nullptr);
    CHECK(strstr(formatted, "^") != nullptr);
  }

  SUBCASE("Format control flow error")
  {
    const char* source = "BEGIN 1 2 + REPEAT";  // Missing WHILE
    v4front_compile_ex(source, &buf, &error);
    v4front_format_error(&error, source, formatted, sizeof(formatted));

    CHECK(strstr(formatted, "Error:") != nullptr);
    CHECK(strstr(formatted, "line 1") != nullptr);
  }
}

TEST_CASE("Error position tracking: with context")
{
  V4FrontContext* ctx = v4front_context_create();
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("Unknown word with context")
  {
    // Register a word
    v4front_compile_with_context(ctx, ": SQUARE DUP * ;", &buf, nullptr, 0);
    v4front_context_register_word(ctx, "SQUARE", 0);
    v4front_free(&buf);

    // Try to use unknown word
    const char* source = "5 UNKNOWN SQUARE";
    v4front_err err = v4front_compile_with_context_ex(ctx, source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.code == front_err_to_int(FrontErr::UnknownToken));
    CHECK(strcmp(error.token, "UNKNOWN") == 0);
    CHECK(error.position == 2);
  }

  SUBCASE("Error in word definition with context")
  {
    const char* source = ": TEST DUP BADTOKEN * ;";
    v4front_err err = v4front_compile_with_context_ex(ctx, source, &buf, &error);

    CHECK(err < 0);
    CHECK(strcmp(error.token, "BADTOKEN") == 0);
    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("Error position tracking: edge cases")
{
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("Error at start of source")
  {
    const char* source = "NOTAWORD";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.position == 0);
    CHECK(error.line == 1);
    CHECK(error.column == 1);
  }

  SUBCASE("Error after whitespace")
  {
    const char* source = "   BAD";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.position == 3);
    CHECK(error.column == 4);
  }

  SUBCASE("Error with tabs")
  {
    const char* source = "1\t2\tBAD";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.position == 4);
  }

  SUBCASE("NULL error_out (should not crash)")
  {
    const char* source = "1 2 UNKNOWN";
    v4front_err err = v4front_compile_ex(source, &buf, nullptr);

    CHECK(err < 0);  // Should still return error
  }

  SUBCASE("NULL source in format_error (should not crash)")
  {
    error.code = front_err_to_int(FrontErr::UnknownToken);
    strcpy(error.message, "test error");
    error.position = -1;
    error.line = -1;
    error.column = -1;
    error.token[0] = '\0';
    error.context[0] = '\0';

    char formatted[256];
    v4front_format_error(&error, nullptr, formatted, sizeof(formatted));

    // Should format without position info
    CHECK(strstr(formatted, "Error:") != nullptr);
    CHECK(strstr(formatted, "test error") != nullptr);
  }
}

TEST_CASE("Error position tracking: complex multiline source")
{
  V4FrontBuf buf;
  V4FrontError error;

  SUBCASE("Error on line 3")
  {
    const char* source =
        "1 2 +\n"
        "3 4 *\n"
        "5 WRONG -";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.line == 3);
    CHECK(strcmp(error.token, "WRONG") == 0);
    CHECK(strcmp(error.context, "5 WRONG -") == 0);
  }

  SUBCASE("Error after mixed content")
  {
    const char* source =
        ": DOUBLE DUP + ;\n"
        "5 DOUBLE\n"
        "OOPS";
    v4front_err err = v4front_compile_ex(source, &buf, &error);

    CHECK(err < 0);
    CHECK(error.line == 3);
    CHECK(strcmp(error.token, "OOPS") == 0);
    v4front_free(&buf);
  }
}
