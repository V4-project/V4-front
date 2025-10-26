#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

// Helper to extract 16-bit little-endian value
static int16_t read_i16_le(const uint8_t* buf)
{
  return (int16_t)((buf[1] << 8) | buf[0]);
}

TEST_CASE("Basic BEGIN/WHILE/REPEAT structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple while loop: 10 BEGIN DUP 0 > WHILE 1 - REPEAT DROP")
  {
    v4front_err err = v4front_compile("10 BEGIN DUP 0 > WHILE 1 - REPEAT DROP", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure:
    // Position  0: LIT 10      (5 bytes)
    // Position  5: (BEGIN)     DUP (1 byte)
    // Position  6: LIT 0       (5 bytes)
    // Position 11: GT          (1 byte)
    // Position 12: JZ offset   (3 bytes) ; forward jump to after REPEAT
    // Position 15: LIT 1       (5 bytes)
    // Position 20: SUB         (1 byte)
    // Position 21: JMP offset  (3 bytes) ; backward jump to BEGIN
    // Position 24: DROP        (1 byte)
    // Position 25: RET         (1 byte)
    //
    // JZ: target=24, next_ip=15, offset=24-15=9
    // JMP: target=5, next_ip=24, offset=5-24=-19

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::GT));
    CHECK(buf.data[12] == static_cast<uint8_t>(Op::JZ));

    int16_t jz_offset = read_i16_le(&buf.data[13]);
    CHECK(jz_offset == 9);  // Forward jump to after REPEAT

    CHECK(buf.data[15] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[20] == static_cast<uint8_t>(Op::SUB));
    CHECK(buf.data[21] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[22]);
    CHECK(jmp_offset == -19);  // Backward jump to BEGIN

    CHECK(buf.data[24] == static_cast<uint8_t>(Op::DROP));
    CHECK(buf.data[25] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Simple while: BEGIN DUP WHILE DROP REPEAT")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE DROP REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("While with comparison: BEGIN DUP 100 < WHILE 2 * REPEAT")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP 100 < WHILE 2 * REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/WHILE/REPEAT with various operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("With arithmetic: 1 BEGIN DUP 100 < WHILE 2 * REPEAT")
  {
    v4front_err err = v4front_compile("1 BEGIN DUP 100 < WHILE 2 * REPEAT", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("With stack operations: BEGIN OVER OVER = WHILE SWAP DROP REPEAT")
  {
    v4front_err err = v4front_compile("BEGIN OVER OVER = WHILE SWAP DROP REPEAT", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("With bitwise: BEGIN DUP 0xFF AND WHILE 1 - REPEAT")
  {
    v4front_err err = v4front_compile("BEGIN DUP 0xFF AND WHILE 1 - REPEAT", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Nested BEGIN/WHILE/REPEAT structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple nesting")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE BEGIN DUP WHILE DROP REPEAT DROP REPEAT", &buf,
                        errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Triple nesting")
  {
    v4front_err err = v4front_compile(
        "BEGIN DUP WHILE BEGIN DUP WHILE BEGIN DUP WHILE DROP REPEAT DROP REPEAT DROP "
        "REPEAT",
        &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("IF inside BEGIN/WHILE/REPEAT")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("IF in loop body")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP 0 > WHILE DUP 5 > IF 2 - ELSE 1 - THEN REPEAT", &buf,
                        errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("IF in condition")
  {
    v4front_err err = v4front_compile("BEGIN 1 IF DUP 0 > ELSE 0 THEN WHILE 1 - REPEAT",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/WHILE/REPEAT inside IF")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Loop in TRUE branch")
  {
    v4front_err err = v4front_compile("1 IF BEGIN DUP WHILE DROP REPEAT THEN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Loop in ELSE branch")
  {
    v4front_err err = v4front_compile("0 IF 42 ELSE BEGIN DUP WHILE DROP REPEAT THEN",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Mixed UNTIL and WHILE/REPEAT")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("UNTIL after WHILE/REPEAT (sequential)")
  {
    v4front_err err = v4front_compile("BEGIN DUP WHILE DROP REPEAT BEGIN DUP UNTIL", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("WHILE/REPEAT after UNTIL (sequential)")
  {
    v4front_err err = v4front_compile("BEGIN DUP UNTIL BEGIN DUP WHILE DROP REPEAT", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("UNTIL inside WHILE/REPEAT")
  {
    v4front_err err = v4front_compile("BEGIN DUP WHILE BEGIN 1 - DUP UNTIL DROP REPEAT",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("WHILE/REPEAT inside UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN BEGIN DUP WHILE DROP REPEAT DUP UNTIL", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Error cases: malformed BEGIN/WHILE/REPEAT structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("WHILE without BEGIN")
  {
    v4front_err err =
        v4front_compile("10 DUP WHILE DROP REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_WHILE_WITHOUT_BEGIN);
    CHECK(strcmp(errmsg, "WHILE without matching BEGIN") == 0);
  }

  SUBCASE("REPEAT without BEGIN")
  {
    v4front_err err = v4front_compile("10 20 + REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_REPEAT_WITHOUT_BEGIN);
    CHECK(strcmp(errmsg, "REPEAT without matching BEGIN") == 0);
  }

  SUBCASE("REPEAT without WHILE")
  {
    v4front_err err =
        v4front_compile("BEGIN 10 20 + REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_REPEAT_WITHOUT_WHILE);
    CHECK(strcmp(errmsg, "REPEAT without matching WHILE") == 0);
  }

  SUBCASE("Duplicate WHILE")
  {
    v4front_err err = v4front_compile("BEGIN DUP WHILE DUP WHILE DROP REPEAT", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_DUPLICATE_WHILE);
    CHECK(strcmp(errmsg, "duplicate WHILE in BEGIN structure") == 0);
  }

  SUBCASE("UNTIL after WHILE (same BEGIN)")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE 1 - UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNTIL_AFTER_WHILE);
    CHECK(strcmp(errmsg, "UNTIL cannot be used after WHILE") == 0);
  }

  SUBCASE("Unclosed BEGIN with WHILE")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE 1 -", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNCLOSED_BEGIN);
  }

  SUBCASE("WHILE for IF (wrong control type)")
  {
    v4front_err err =
        v4front_compile("1 IF 42 WHILE DROP REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_WHILE_WITHOUT_BEGIN);
  }
}

TEST_CASE("Case insensitive WHILE/REPEAT keywords")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Lowercase")
  {
    v4front_err err =
        v4front_compile("begin dup while drop repeat", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case")
  {
    v4front_err err =
        v4front_compile("Begin Dup While Drop Repeat", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Uppercase")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE DROP REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical BEGIN/WHILE/REPEAT examples")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Countdown from N to 0")
  {
    v4front_err err = v4front_compile("10 BEGIN DUP 0 > WHILE 1 - REPEAT DROP", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Double until >= 100")
  {
    v4front_err err = v4front_compile("1 BEGIN DUP 100 < WHILE 2 * REPEAT", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("GCD algorithm skeleton")
  {
    v4front_err err = v4front_compile("BEGIN DUP WHILE SWAP OVER MOD REPEAT DROP", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Sum of numbers 1 to N")
  {
    v4front_err err =
        v4front_compile("0 SWAP BEGIN DUP 0 > WHILE OVER + SWAP 1 - SWAP REPEAT DROP",
                        &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Forward and backward jump offset verification")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify both offsets")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE DROP REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Position 0: (BEGIN) DUP (1 byte)
    // Position 1: JZ opcode   (1 byte)
    // Position 2-3: offset    (2 bytes)
    // Position 4: DROP        (1 byte)
    // Position 5: JMP opcode  (1 byte)
    // Position 6-7: offset    (2 bytes)
    // Position 8: RET         (1 byte)

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::JZ));

    int16_t jz_offset = read_i16_le(&buf.data[2]);
    // JZ: target=8, next_ip=4, offset=8-4=4
    CHECK(jz_offset == 4);

    CHECK(buf.data[4] == static_cast<uint8_t>(Op::DROP));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[6]);
    // JMP: target=0, next_ip=8, offset=0-8=-8
    CHECK(jmp_offset == -8);

    v4front_free(&buf);
  }
}

TEST_CASE("Deep nesting with mixed control structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Mixed IF, WHILE, and UNTIL")
  {
    v4front_err err =
        v4front_compile("1 IF BEGIN DUP WHILE BEGIN DUP UNTIL DROP REPEAT THEN", &buf,
                        errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Maximum nesting depth (16 WHILE loops)")
  {
    std::string code = "";
    for (int i = 0; i < 16; i++)
    {
      code += "BEGIN DUP WHILE ";
    }
    for (int i = 0; i < 16; i++)
    {
      code += "DROP REPEAT ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Empty condition or body")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Minimal while loop: BEGIN 1 WHILE REPEAT")
  {
    v4front_err err =
        v4front_compile("BEGIN 1 WHILE REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Empty body: BEGIN DUP WHILE REPEAT")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE REPEAT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}