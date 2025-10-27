#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Basic word definition")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Define simple word: : DOUBLE DUP + ;")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should have 1 word defined
    CHECK(buf.word_count == 1);

    // Word should be named "DOUBLE"
    CHECK(strcmp(buf.words[0].name, "DOUBLE") == 0);

    // Word bytecode should contain: DUP, ADD, RET
    CHECK(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.words[0].code[1] == static_cast<uint8_t>(Op::ADD));
    CHECK(buf.words[0].code[2] == static_cast<uint8_t>(Op::RET));

    // Main bytecode should only contain RET (no main code)
    CHECK(buf.size == 1);
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Define word and use it: : DOUBLE DUP + ; 5 DOUBLE")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ; 5 DOUBLE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should have 1 word defined
    CHECK(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "DOUBLE") == 0);

    // Main bytecode should contain: LIT 5, CALL 0, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    // bytes 1-4: value 5
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::CALL));
    // bytes 6-7: index 0 (little-endian)
    CHECK(buf.data[6] == 0);  // index 0 low byte
    CHECK(buf.data[7] == 0);  // index 0 high byte

    v4front_free(&buf);
  }

  SUBCASE("Multiple word definitions")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ; : TRIPLE DUP DUP + + ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should have 2 words defined
    CHECK(buf.word_count == 2);
    CHECK(strcmp(buf.words[0].name, "DOUBLE") == 0);
    CHECK(strcmp(buf.words[1].name, "TRIPLE") == 0);

    // DOUBLE: DUP, ADD, RET
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.words[0].code[1] == static_cast<uint8_t>(Op::ADD));
    CHECK(buf.words[0].code[2] == static_cast<uint8_t>(Op::RET));

    // TRIPLE: DUP, DUP, ADD, ADD, RET
    CHECK(buf.words[1].code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.words[1].code[1] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.words[1].code[2] == static_cast<uint8_t>(Op::ADD));
    CHECK(buf.words[1].code[3] == static_cast<uint8_t>(Op::ADD));
    CHECK(buf.words[1].code[4] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }
}

TEST_CASE("Word calling word")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Define DOUBLE, then QUADRUPLE that uses DOUBLE")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ; : QUADRUPLE DOUBLE DOUBLE ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 2);
    CHECK(strcmp(buf.words[0].name, "DOUBLE") == 0);
    CHECK(strcmp(buf.words[1].name, "QUADRUPLE") == 0);

    // QUADRUPLE should contain: CALL 0, CALL 0, RET
    CHECK(buf.words[1].code[0] == static_cast<uint8_t>(Op::CALL));
    CHECK(buf.words[1].code[1] == 0);  // index 0 low byte
    CHECK(buf.words[1].code[2] == 0);  // index 0 high byte
    CHECK(buf.words[1].code[3] == static_cast<uint8_t>(Op::CALL));
    CHECK(buf.words[1].code[4] == 0);  // index 0 low byte
    CHECK(buf.words[1].code[5] == 0);  // index 0 high byte
    CHECK(buf.words[1].code[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Use both words in main code")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ; : TRIPLE DUP DUP + + ; 5 DOUBLE 3 TRIPLE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 2);

    // Main code should have: LIT 5, CALL 0, LIT 3, CALL 1, RET
    size_t pos = 0;
    CHECK(buf.data[pos++] == static_cast<uint8_t>(Op::LIT));
    pos += 4;  // skip 32-bit value
    CHECK(buf.data[pos++] == static_cast<uint8_t>(Op::CALL));
    CHECK(buf.data[pos++] == 0);  // DOUBLE is index 0
    CHECK(buf.data[pos++] == 0);
    CHECK(buf.data[pos++] == static_cast<uint8_t>(Op::LIT));
    pos += 4;  // skip 32-bit value
    CHECK(buf.data[pos++] == static_cast<uint8_t>(Op::CALL));
    CHECK(buf.data[pos++] == 1);  // TRIPLE is index 1
    CHECK(buf.data[pos++] == 0);

    v4front_free(&buf);
  }
}

TEST_CASE("Word definition with control flow")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Word with IF-THEN")
  {
    v4front_err err = v4front_compile(": ABS DUP 0 < IF 0 SWAP - THEN ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "ABS") == 0);

    // Should contain DUP, LIT, LT, JZ, ...
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::DUP));

    v4front_free(&buf);
  }

  SUBCASE("Word with DO-LOOP")
  {
    v4front_err err = v4front_compile(": SUM 0 SWAP 0 DO I + LOOP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "SUM") == 0);

    // Should contain loop structure
    bool has_tor = false;
    bool has_fromr = false;
    for (size_t i = 0; i < buf.words[0].code_len; i++)
    {
      if (buf.words[0].code[i] == static_cast<uint8_t>(Op::TOR))
        has_tor = true;
      if (buf.words[0].code[i] == static_cast<uint8_t>(Op::FROMR))
        has_fromr = true;
    }
    CHECK(has_tor);
    CHECK(has_fromr);

    v4front_free(&buf);
  }
}

TEST_CASE("Word definition errors")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Colon without name")
  {
    v4front_err err = v4front_compile(":", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::ColonWithoutName);
  }

  SUBCASE("Semicolon without colon")
  {
    v4front_err err = v4front_compile("5 5 + ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::SemicolonWithoutColon);
  }

  SUBCASE("Unclosed colon")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedColon);
  }

  SUBCASE("Nested colon")
  {
    v4front_err err = v4front_compile(": OUTER : INNER + ; ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::NestedColon);
  }

  SUBCASE("Duplicate word name")
  {
    v4front_err err = v4front_compile(": DOUBLE DUP + ; : DOUBLE DUP DUP + + ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::DuplicateWord);
  }
}

TEST_CASE("Case insensitive word names")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Define and use with different case")
  {
    v4front_err err = v4front_compile(": double dup + ; 5 DOUBLE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    // Main code should successfully call the word
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::CALL));

    v4front_free(&buf);
  }

  SUBCASE("Duplicate detection is case insensitive")
  {
    v4front_err err = v4front_compile(": double dup + ; : DOUBLE dup dup + + ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::DuplicateWord);
  }
}

TEST_CASE("Empty word definition")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Word with no body: : NOOP ;")
  {
    v4front_err err = v4front_compile(": NOOP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    CHECK(strcmp(buf.words[0].name, "NOOP") == 0);

    // Should only contain RET
    CHECK(buf.words[0].code_len == 1);
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }
}
