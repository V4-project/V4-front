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

TEST_CASE("Basic IF/THEN structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple IF/THEN: 1 IF 42 THEN")
  {
    v4front_err err = v4front_compile("1 IF 42 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure:
    // LIT 1, JZ offset, LIT 42, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::JZ));

    // JZ should jump to RET (skip LIT 42)
    int16_t jz_offset = read_i16_le(&buf.data[6]);
    CHECK(jz_offset == 5);  // Jump over LIT 42 (5 bytes)

    CHECK(buf.data[8] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[13] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("IF/THEN with comparison: 5 3 > IF 100 THEN")
  {
    v4front_err err = v4front_compile("5 3 > IF 100 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure: LIT 5, LIT 3, GT, JZ offset, LIT 100, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::GT));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::JZ));

    v4front_free(&buf);
  }

  SUBCASE("IF/THEN with zero condition: 0 IF DROP THEN")
  {
    v4front_err err = v4front_compile("0 IF DROP THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure: LIT 0, JZ offset, DROP, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::JZ));
    CHECK(buf.data[8] == static_cast<uint8_t>(Op::DROP));

    v4front_free(&buf);
  }
}

TEST_CASE("IF/ELSE/THEN structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple IF/ELSE/THEN: 1 IF 42 ELSE 99 THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 42 ELSE 99 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure:
    // Position  0: LIT 1       (5 bytes)
    // Position  5: JZ offset   (3 bytes: opcode + 2 bytes offset)
    // Position  8: LIT 42      (5 bytes)
    // Position 13: JMP offset  (3 bytes)
    // Position 16: LIT 99      (5 bytes)
    // Position 21: RET         (1 byte)
    //
    // JZ offset: target=16 (ELSE clause), next_ip=8, offset=16-8=8
    // JMP offset: target=21 (RET), next_ip=16, offset=21-16=5

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::JZ));

    int16_t jz_offset = read_i16_le(&buf.data[6]);
    CHECK(jz_offset == 8);  // Jump to ELSE clause (LIT 99)

    CHECK(buf.data[8] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[13] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[14]);
    CHECK(jmp_offset == 5);  // Jump to RET (skip ELSE clause)

    CHECK(buf.data[16] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[21] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("IF/ELSE/THEN with comparison: 5 3 < IF 10 ELSE 20 THEN")
  {
    v4front_err err =
        v4front_compile("5 3 < IF 10 ELSE 20 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure: LIT 5, LIT 3, LT, JZ, LIT 10, JMP, LIT 20, RET
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::LT));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::JZ));
    CHECK(buf.data[19] == static_cast<uint8_t>(Op::JMP));

    v4front_free(&buf);
  }

  SUBCASE("IF/ELSE/THEN with operations: 1 IF 10 20 + ELSE 30 40 * THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 10 20 + ELSE 30 40 * THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Nested IF structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Nested IF/THEN: 1 IF 2 IF 42 THEN THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 2 IF 42 THEN THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure: LIT 1, JZ outer_end, LIT 2, JZ inner_end, LIT 42, (inner_end:),
    // (outer_end:), RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::JZ));
    CHECK(buf.data[8] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[13] == static_cast<uint8_t>(Op::JZ));

    v4front_free(&buf);
  }

  SUBCASE("Nested IF/ELSE/THEN: 1 IF 2 IF 10 ELSE 20 THEN ELSE 30 THEN")
  {
    v4front_err err = v4front_compile("1 IF 2 IF 10 ELSE 20 THEN ELSE 30 THEN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("Triple nested IF: 1 IF 2 IF 3 IF 42 THEN THEN THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 2 IF 3 IF 42 THEN THEN THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("IF with complex expressions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("IF with arithmetic: 10 5 > IF 100 200 + THEN")
  {
    v4front_err err =
        v4front_compile("10 5 > IF 100 200 + THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("IF with stack operations: 1 IF DUP DROP SWAP THEN")
  {
    v4front_err err =
        v4front_compile("1 IF DUP DROP SWAP THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("IF with bitwise: 0xFF 0xAA AND IF 1 ELSE 0 THEN")
  {
    v4front_err err =
        v4front_compile("0xFF 0xAA AND IF 1 ELSE 0 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Multiple sequential IF structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Two sequential IF/THEN: 1 IF 10 THEN 2 IF 20 THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 10 THEN 2 IF 20 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE(
      "Three sequential IF/ELSE: 1 IF 10 ELSE 11 THEN 2 IF 20 ELSE 21 THEN 3 IF 30 ELSE "
      "31 THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 10 ELSE 11 THEN 2 IF 20 ELSE 21 THEN 3 IF 30 ELSE 31 THEN",
                        &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Error cases: malformed IF structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("ELSE without IF")
  {
    v4front_err err = v4front_compile("10 ELSE 20", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::ElseWithoutIf);
    CHECK(strcmp(errmsg, "ELSE without matching IF") == 0);
  }

  SUBCASE("THEN without IF")
  {
    v4front_err err = v4front_compile("10 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::ThenWithoutIf);
    CHECK(strcmp(errmsg, "THEN without matching IF") == 0);
  }

  SUBCASE("Unclosed IF (missing THEN)")
  {
    v4front_err err = v4front_compile("1 IF 42", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedIf);
    CHECK(strcmp(errmsg, "unclosed IF structure") == 0);
  }

  SUBCASE("Unclosed nested IF")
  {
    v4front_err err = v4front_compile("1 IF 2 IF 42 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedIf);
  }

  SUBCASE("Duplicate ELSE")
  {
    v4front_err err =
        v4front_compile("1 IF 10 ELSE 20 ELSE 30 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::DuplicateElse);
    CHECK(strcmp(errmsg, "duplicate ELSE in IF structure") == 0);
  }

  SUBCASE("ELSE after THEN")
  {
    v4front_err err =
        v4front_compile("1 IF 10 THEN ELSE 20", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::ElseWithoutIf);
  }
}

TEST_CASE("Case insensitive control flow keywords")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Lowercase: if then else")
  {
    v4front_err err = v4front_compile("1 if 42 then", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);

    err = v4front_compile("1 if 10 else 20 then", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case: If Then Else")
  {
    v4front_err err = v4front_compile("1 If 42 Then", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);

    err = v4front_compile("1 If 10 Else 20 Then", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Uppercase: IF THEN ELSE")
  {
    v4front_err err = v4front_compile("1 IF 42 THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical IF examples")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Absolute value: DUP 0 < IF 0 SWAP - THEN")
  {
    v4front_err err =
        v4front_compile("DUP 0 < IF 0 SWAP - THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Maximum of two numbers: 2DUP > IF DROP ELSE SWAP DROP THEN")
  {
    v4front_err err = v4front_compile("OVER OVER > IF DROP ELSE SWAP DROP THEN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE(
      "Sign function: DUP 0 > IF DROP 1 ELSE DUP 0 < IF DROP -1 ELSE DROP 0 THEN THEN")
  {
    v4front_err err =
        v4front_compile("DUP 0 > IF DROP 1 ELSE DUP 0 < IF DROP -1 ELSE DROP 0 THEN THEN",
                        &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Deep nesting limit")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Maximum nesting depth (32 levels)")
  {
    // Build a string with 32 nested IFs
    std::string code = "";
    for (int i = 0; i < 32; i++)
    {
      code += "1 IF ";
    }
    code += "42 ";
    for (int i = 0; i < 32; i++)
    {
      code += "THEN ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Exceeding nesting depth (33 levels)")
  {
    // Build a string with 33 nested IFs
    std::string code = "";
    for (int i = 0; i < 33; i++)
    {
      code += "1 IF ";
    }
    code += "42 ";
    for (int i = 0; i < 33; i++)
    {
      code += "THEN ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::ControlDepthExceeded);
    CHECK(strcmp(errmsg, "control structure nesting too deep") == 0);
  }
}