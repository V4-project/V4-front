#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.hpp"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Case-insensitive stack operators")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("DUP in uppercase")
  {
    v4front_err err = v4front_compile("10 DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    v4front_free(&buf);
  }

  SUBCASE("dup in lowercase")
  {
    v4front_err err = v4front_compile("10 dup", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    v4front_free(&buf);
  }

  SUBCASE("Dup in mixed case")
  {
    v4front_err err = v4front_compile("10 Dup", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    v4front_free(&buf);
  }

  SUBCASE("DROP variations")
  {
    v4front_err err;

    err = v4front_compile("10 DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("10 drop", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("10 Drop", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("SWAP variations")
  {
    v4front_err err;

    err = v4front_compile("10 20 SWAP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SWAP));
    v4front_free(&buf);

    err = v4front_compile("10 20 swap", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SWAP));
    v4front_free(&buf);
  }

  SUBCASE("OVER variations")
  {
    v4front_err err;

    err = v4front_compile("10 20 OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OVER));
    v4front_free(&buf);

    err = v4front_compile("10 20 over", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OVER));
    v4front_free(&buf);
  }
}

TEST_CASE("Case-insensitive arithmetic operators")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("MOD in uppercase")
  {
    v4front_err err = v4front_compile("43 7 MOD", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::MOD));
    v4front_free(&buf);
  }

  SUBCASE("mod in lowercase")
  {
    v4front_err err = v4front_compile("43 7 mod", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::MOD));
    v4front_free(&buf);
  }

  SUBCASE("Mod in mixed case")
  {
    v4front_err err = v4front_compile("43 7 Mod", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::MOD));
    v4front_free(&buf);
  }
}

TEST_CASE("Case-insensitive bitwise operators")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("AND variations")
  {
    v4front_err err;

    err = v4front_compile("15 7 AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);

    err = v4front_compile("15 7 and", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);

    err = v4front_compile("15 7 And", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::AND));
    v4front_free(&buf);
  }

  SUBCASE("OR variations")
  {
    v4front_err err;

    err = v4front_compile("8 4 OR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OR));
    v4front_free(&buf);

    err = v4front_compile("8 4 or", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OR));
    v4front_free(&buf);
  }

  SUBCASE("XOR variations")
  {
    v4front_err err;

    err = v4front_compile("12 5 XOR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);

    err = v4front_compile("12 5 xor", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);

    err = v4front_compile("12 5 Xor", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::XOR));
    v4front_free(&buf);
  }

  SUBCASE("INVERT variations")
  {
    v4front_err err;

    err = v4front_compile("42 INVERT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    v4front_free(&buf);

    err = v4front_compile("42 invert", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    v4front_free(&buf);

    err = v4front_compile("42 Invert", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::INVERT));
    v4front_free(&buf);
  }
}

TEST_CASE("Symbol operators remain case-sensitive (or case-irrelevant)")
{
  V4FrontBuf buf;
  char errmsg[256];

  // Symbols don't have uppercase/lowercase variants, so this just confirms
  // they still work as expected
  SUBCASE("Arithmetic symbols")
  {
    v4front_err err;

    err = v4front_compile("10 20 +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("20 10 -", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("6 7 *", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("42 7 /", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Comparison symbols")
  {
    v4front_err err;

    err = v4front_compile("5 5 =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("5 3 <>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("3 5 <", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);

    err = v4front_compile("5 3 >", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Complex expressions with mixed case")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Mixed case stack manipulation")
  {
    v4front_err err =
        v4front_compile("10 dup 20 Swap over DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case arithmetic")
  {
    v4front_err err =
        v4front_compile("100 7 mod 2 * 3 + dup", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case bitwise")
  {
    v4front_err err =
        v4front_compile("15 7 and 8 Or 3 xor", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}