#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.hpp"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Bitwise operators compile correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("AND: 0xFF 0x0F AND")
  {
    v4front_err err = v4front_compile("0xFF 0x0F AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);
  }

  SUBCASE("OR: 0xF0 0x0F OR")
  {
    v4front_err err = v4front_compile("0xF0 0x0F OR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OR));
    v4front_free(&buf);
  }

  SUBCASE("XOR: 0xFF 0xAA XOR")
  {
    v4front_err err = v4front_compile("0xFF 0xAA XOR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);
  }

  SUBCASE("INVERT: 0xFF INVERT")
  {
    v4front_err err = v4front_compile("0xFF INVERT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    // Structure: LIT(5) + INVERT(1) + RET(1) = 7 bytes
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    v4front_free(&buf);
  }
}

TEST_CASE("Complex bitwise expressions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Multiple operations: 0xFF 0x0F AND 0xF0 OR")
  {
    v4front_err err =
        v4front_compile("0xFF 0x0F AND 0xF0 OR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    // Should compile successfully
    v4front_free(&buf);
  }

  SUBCASE("XOR with negative: -1 0xFFFF XOR")
  {
    v4front_err err = v4front_compile("-1 0xFFFF XOR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);
  }

  SUBCASE("Combining AND and INVERT: 0xAAAA INVERT 0x5555 AND")
  {
    v4front_err err =
        v4front_compile("0xAAAA INVERT 0x5555 AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Bitwise operators in bytecode structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify bytecode structure for: 12 7 AND")
  {
    v4front_err err = v4front_compile("12 7 AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure should be:
    // [0] = LIT, [1-4] = 12 (little-endian)
    // [5] = LIT, [6-9] = 7 (little-endian)
    // [10] = AND
    // [11] = RET

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[1] == 12);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[6] == 7);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
    CHECK(buf.size == 12);

    v4front_free(&buf);
  }

  SUBCASE("Verify bytecode structure for: 42 INVERT")
  {
    v4front_err err = v4front_compile("42 INVERT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure should be:
    // [0] = LIT, [1-4] = 42 (little-endian)
    // [5] = INVERT
    // [6] = RET

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[1] == 42);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));
    CHECK(buf.size == 7);

    v4front_free(&buf);
  }
}

TEST_CASE("Bitwise with decimal and hexadecimal literals")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Decimal literals: 15 8 AND")
  {
    v4front_err err = v4front_compile("15 8 AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);
  }

  SUBCASE("Mixed literals: 255 0xFF AND")
  {
    v4front_err err = v4front_compile("255 0xFF AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);
  }

  SUBCASE("Hexadecimal only: 0xDEAD 0xBEEF OR")
  {
    v4front_err err = v4front_compile("0xDEAD 0xBEEF OR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OR));
    v4front_free(&buf);
  }
}

TEST_CASE("Practical bitwise operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Masking: 0x12345678 0xFF AND (extract lower byte)")
  {
    v4front_err err =
        v4front_compile("0x12345678 0xFF AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);
  }

  SUBCASE("Setting bits: 0x00 0x80 OR (set bit 7)")
  {
    v4front_err err = v4front_compile("0x00 0x80 OR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OR));
    v4front_free(&buf);
  }

  SUBCASE("Toggling bits: 0xFF 0xAA XOR")
  {
    v4front_err err = v4front_compile("0xFF 0xAA XOR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);
  }

  SUBCASE("Bitwise NOT: -1 INVERT (should give 0)")
  {
    v4front_err err = v4front_compile("-1 INVERT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    v4front_free(&buf);
  }
}