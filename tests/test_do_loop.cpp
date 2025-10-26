#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Basic DO LOOP structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simplest loop: 10 0 DO LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure should contain:
    // LIT 10, LIT 0, SWAP, TOR, TOR, ... LOOP body ..., DROP DROP, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SWAP));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::TOR));
    CHECK(buf.data[12] == static_cast<uint8_t>(Op::TOR));

    v4front_free(&buf);
  }

  SUBCASE("Loop with body: 10 0 DO I LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain RFETCH for I
    bool found_rfetch = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::RFETCH))
      {
        found_rfetch = true;
        break;
      }
    }
    CHECK(found_rfetch);

    v4front_free(&buf);
  }
}

TEST_CASE("DO LOOP with +LOOP")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Basic +LOOP: 10 0 DO I 2 +LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I 2 +LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("+LOOP with variable increment: 10 0 DO I DUP +LOOP")
  {
    v4front_err err =
        v4front_compile("10 0 DO I DUP +LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Loop index access with I")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Using I in calculations: 10 0 DO I 2 * LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I 2 * LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Multiple I references: 10 0 DO I I + LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I I + LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Nested DO LOOP structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple nesting: 3 0 DO 3 0 DO I LOOP LOOP")
  {
    v4front_err err =
        v4front_compile("3 0 DO 3 0 DO I LOOP LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Using J in nested loop: 3 0 DO 3 0 DO I J + LOOP LOOP")
  {
    v4front_err err =
        v4front_compile("3 0 DO 3 0 DO I J + LOOP LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain code for J (3 FROMR, DUP, 3 TOR)
    v4front_free(&buf);
  }

  SUBCASE("Triple nesting with K: 2 0 DO 2 0 DO 2 0 DO I J K LOOP LOOP LOOP")
  {
    v4front_err err = v4front_compile("2 0 DO 2 0 DO 2 0 DO I J K LOOP LOOP LOOP", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("DO LOOP with arithmetic in body")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Accumulator pattern: 0 10 0 DO I + LOOP")
  {
    v4front_err err = v4front_compile("0 10 0 DO I + LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Complex calculation: 10 0 DO I DUP * LOOP")
  {
    v4front_err err =
        v4front_compile("10 0 DO I DUP * LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("DO LOOP inside IF")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Loop in TRUE branch: 1 IF 10 0 DO I LOOP THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 10 0 DO I LOOP THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Loop in ELSE branch: 0 IF 42 ELSE 10 0 DO I LOOP THEN")
  {
    v4front_err err =
        v4front_compile("0 IF 42 ELSE 10 0 DO I LOOP THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("IF inside DO LOOP")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Conditional in loop: 10 0 DO I 5 > IF I THEN LOOP")
  {
    v4front_err err =
        v4front_compile("10 0 DO I 5 > IF I THEN LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("IF/ELSE in loop: 10 0 DO I 5 < IF I ELSE 0 THEN LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I 5 < IF I ELSE 0 THEN LOOP", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("DO LOOP with BEGIN/UNTIL")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("BEGIN inside DO: 3 0 DO BEGIN I UNTIL LOOP")
  {
    v4front_err err =
        v4front_compile("3 0 DO BEGIN I UNTIL LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("DO inside BEGIN: BEGIN 10 0 DO I LOOP DUP UNTIL")
  {
    v4front_err err =
        v4front_compile("BEGIN 10 0 DO I LOOP DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Error cases: malformed DO LOOP structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("LOOP without DO")
  {
    v4front_err err = v4front_compile("10 20 + LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::LoopWithoutDo);
    CHECK(strcmp(errmsg, "LOOP without matching DO") == 0);
  }

  SUBCASE("+LOOP without DO")
  {
    v4front_err err = v4front_compile("2 +LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::PLoopWithoutDo);
    CHECK(strcmp(errmsg, "+LOOP without matching DO") == 0);
  }

  SUBCASE("Unclosed DO (missing LOOP)")
  {
    v4front_err err = v4front_compile("10 0 DO I 2 *", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedDo);
    CHECK(strcmp(errmsg, "unclosed DO structure") == 0);
  }

  SUBCASE("Unclosed nested DO")
  {
    v4front_err err =
        v4front_compile("3 0 DO 3 0 DO I LOOP DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedDo);
  }

  SUBCASE("LOOP for IF (wrong control type)")
  {
    v4front_err err = v4front_compile("1 IF 42 LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::LoopWithoutDo);
  }
}

TEST_CASE("Case insensitive DO/LOOP keywords")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Lowercase: do loop")
  {
    v4front_err err = v4front_compile("10 0 do i loop", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case: Do Loop")
  {
    v4front_err err = v4front_compile("10 0 Do I Loop", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Uppercase: DO LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical DO LOOP examples")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Sum 0 to 9: 0 10 0 DO I + LOOP")
  {
    v4front_err err = v4front_compile("0 10 0 DO I + LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Factorial skeleton: 1 5 1 DO I * LOOP")
  {
    v4front_err err = v4front_compile("1 5 1 DO I * LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Multiplication table: 10 0 DO 10 0 DO I J * LOOP LOOP")
  {
    v4front_err err =
        v4front_compile("10 0 DO 10 0 DO I J * LOOP LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Count by 2: 10 0 DO I 2 +LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO I 2 +LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Edge cases")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Empty loop body: 10 0 DO LOOP")
  {
    v4front_err err = v4front_compile("10 0 DO LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Loop with limit=index: 5 5 DO I LOOP")
  {
    v4front_err err = v4front_compile("5 5 DO I LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Multiple sequential loops")
  {
    v4front_err err =
        v4front_compile("10 0 DO I LOOP 10 0 DO I LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}