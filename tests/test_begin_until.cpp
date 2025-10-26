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

TEST_CASE("Basic BEGIN/UNTIL structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple countdown loop: 5 BEGIN 1 - DUP UNTIL DROP")
  {
    v4front_err err =
        v4front_compile("5 BEGIN 1 - DUP UNTIL DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure:
    // Position  0: LIT 5       (5 bytes)
    // Position  5: (BEGIN)     LIT 1   (5 bytes)
    // Position 10: SUB         (1 byte)
    // Position 11: DUP         (1 byte)
    // Position 12: JZ offset   (3 bytes) ; backward jump to position 5
    // Position 15: DROP        (1 byte)
    // Position 16: RET         (1 byte)
    //
    // JZ offset: target=5 (BEGIN), next_ip=15, offset=5-15=-10

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SUB));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[12] == static_cast<uint8_t>(Op::JZ));

    int16_t jz_offset = read_i16_le(&buf.data[13]);
    CHECK(jz_offset == -10);  // Backward jump to BEGIN

    CHECK(buf.data[15] == static_cast<uint8_t>(Op::DROP));
    CHECK(buf.data[16] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Simple loop: BEGIN DUP UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure:
    // Position 0: (BEGIN)  DUP  (1 byte)
    // Position 1: JZ opcode    (1 byte)
    // Position 2-3: offset     (2 bytes)
    // Position 4: RET          (1 byte)
    //
    // JZ calculation:
    //   After reading opcode (pos 1), IP at pos 2
    //   After reading offset (2 bytes), next_ip at pos 4
    //   Target: 0 (BEGIN)
    //   Offset: 0 - 4 = -4

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::JZ));

    int16_t jz_offset = read_i16_le(&buf.data[2]);
    CHECK(jz_offset == -4);

    v4front_free(&buf);
  }

  SUBCASE("Loop with comparison: 10 BEGIN 1 - DUP 0 = UNTIL DROP")
  {
    v4front_err err =
        v4front_compile("10 BEGIN 1 - DUP 0 = UNTIL DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/UNTIL with various operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("With arithmetic: BEGIN 2 * DUP 100 > UNTIL")
  {
    v4front_err err =
        v4front_compile("BEGIN 2 * DUP 100 > UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("With stack operations: BEGIN SWAP DUP UNTIL")
  {
    v4front_err err =
        v4front_compile("BEGIN SWAP DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("With bitwise: BEGIN 1 - DUP 0xF AND UNTIL")
  {
    v4front_err err =
        v4front_compile("BEGIN 1 - DUP 0xF AND UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Nested BEGIN/UNTIL structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple nesting: BEGIN BEGIN DUP UNTIL DROP DUP UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN BEGIN DUP UNTIL DROP DUP UNTIL", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Triple nesting")
  {
    v4front_err err =
        v4front_compile("BEGIN BEGIN BEGIN DUP UNTIL DROP DUP UNTIL DROP DUP UNTIL", &buf,
                        errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("IF inside BEGIN/UNTIL")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple IF in loop: BEGIN DUP 5 > IF 1 - THEN DUP UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN DUP 5 > IF 1 - THEN DUP UNTIL", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("IF/ELSE in loop: BEGIN DUP 10 < IF 1 + ELSE 1 - THEN DUP 0 = UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN DUP 10 < IF 1 + ELSE 1 - THEN DUP 0 = UNTIL",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/UNTIL inside IF")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Loop in TRUE branch: 1 IF BEGIN DUP UNTIL THEN")
  {
    v4front_err err =
        v4front_compile("1 IF BEGIN DUP UNTIL THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Loop in ELSE branch: 0 IF 42 ELSE BEGIN DUP UNTIL THEN")
  {
    v4front_err err = v4front_compile("0 IF 42 ELSE BEGIN DUP UNTIL THEN", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Loop in both branches: 1 IF BEGIN DUP UNTIL ELSE BEGIN DUP UNTIL THEN")
  {
    v4front_err err = v4front_compile("1 IF BEGIN DUP UNTIL ELSE BEGIN DUP UNTIL THEN",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Multiple sequential BEGIN/UNTIL structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Two sequential loops")
  {
    v4front_err err = v4front_compile("BEGIN DUP UNTIL DROP BEGIN DUP UNTIL", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }

  SUBCASE("Three sequential loops")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP UNTIL DROP BEGIN DUP UNTIL DROP BEGIN DUP UNTIL", &buf,
                        errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Error cases: malformed BEGIN/UNTIL structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("UNTIL without BEGIN")
  {
    v4front_err err = v4front_compile("10 DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNTIL_WITHOUT_BEGIN);
    CHECK(strcmp(errmsg, "UNTIL without matching BEGIN") == 0);
  }

  SUBCASE("Unclosed BEGIN (missing UNTIL)")
  {
    v4front_err err = v4front_compile("BEGIN 10 20 +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNCLOSED_BEGIN);
    CHECK(strcmp(errmsg, "unclosed BEGIN structure") == 0);
  }

  SUBCASE("Unclosed nested BEGIN")
  {
    v4front_err err =
        v4front_compile("BEGIN BEGIN DUP UNTIL DROP DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNCLOSED_BEGIN);
  }

  SUBCASE("UNTIL for IF (wrong control type)")
  {
    v4front_err err = v4front_compile("1 IF 42 UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_UNTIL_WITHOUT_BEGIN);
  }

  SUBCASE("THEN for BEGIN (wrong control type)")
  {
    v4front_err err = v4front_compile("BEGIN 42 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_THEN_WITHOUT_IF);
  }
}

TEST_CASE("Case insensitive BEGIN/UNTIL keywords")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Lowercase: begin until")
  {
    v4front_err err = v4front_compile("begin dup until", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case: Begin Until")
  {
    v4front_err err = v4front_compile("Begin dup Until", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Uppercase: BEGIN UNTIL")
  {
    v4front_err err = v4front_compile("BEGIN DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical BEGIN/UNTIL examples")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Count down from N to 0")
  {
    v4front_err err =
        v4front_compile("10 BEGIN 1 - DUP UNTIL DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Double until >= 100")
  {
    v4front_err err =
        v4front_compile("1 BEGIN 2 * DUP 100 >= UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Find power of 2")
  {
    v4front_err err = v4front_compile("1 BEGIN DUP 2 * SWAP DROP DUP 1000 > UNTIL", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("GCD algorithm skeleton")
  {
    v4front_err err =
        v4front_compile("BEGIN OVER OVER = UNTIL", &buf, errmsg,
                        sizeof(errmsg));  // Simplified, real GCD needs more logic
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Backward jump offset calculation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify backward jump offset")
  {
    v4front_err err = v4front_compile("BEGIN DUP UNTIL", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Position 0: (BEGIN) DUP (1 byte)
    // Position 1: JZ opcode   (1 byte)
    // Position 2-3: offset    (2 bytes) - this is what read_i16_le reads
    // Position 4: RET         (1 byte)
    //
    // JZ offset calculation:
    // - After reading JZ opcode, IP is at position 2
    // - After reading 2-byte offset, next_ip is at position 4
    // - Target is position 0 (BEGIN)
    // - offset = target - next_ip = 0 - 4 = -4

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::JZ));

    int16_t offset = read_i16_le(&buf.data[2]);
    CHECK(offset == -4);

    v4front_free(&buf);
  }
}

TEST_CASE("Deep nesting limit with BEGIN/UNTIL")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Maximum nesting depth (16 BEGINs)")
  {
    // Build a string with 16 nested BEGIN/UNTIL
    std::string code = "";
    for (int i = 0; i < 16; i++)
    {
      code += "BEGIN DUP ";
    }
    for (int i = 0; i < 16; i++)
    {
      code += "UNTIL DROP ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed IF and BEGIN nesting (total 32)")
  {
    std::string code = "";
    for (int i = 0; i < 16; i++)
    {
      code += "1 IF BEGIN ";
    }
    code += "42 ";
    for (int i = 0; i < 16; i++)
    {
      code += "UNTIL THEN ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Exceeding nesting depth")
  {
    std::string code = "";
    for (int i = 0; i < 33; i++)
    {
      code += "BEGIN ";
    }
    code += "DUP ";
    for (int i = 0; i < 33; i++)
    {
      code += "UNTIL ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_CONTROL_DEPTH_EXCEEDED);
  }
}