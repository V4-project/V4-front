#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.hpp"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Comparison operators compile correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Equal: 5 5 =")
  {
    v4front_err err = v4front_compile("5 5 =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::EQ));
    v4front_free(&buf);
  }

  SUBCASE("Equal (alternative): 5 5 ==")
  {
    v4front_err err = v4front_compile("5 5 ==", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::EQ));
    v4front_free(&buf);
  }

  SUBCASE("Not equal: 5 3 <>")
  {
    v4front_err err = v4front_compile("5 3 <>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::NE));
    v4front_free(&buf);
  }

  SUBCASE("Not equal (alternative): 5 3 !=")
  {
    v4front_err err = v4front_compile("5 3 !=", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::NE));
    v4front_free(&buf);
  }

  SUBCASE("Less than: 3 5 <")
  {
    v4front_err err = v4front_compile("3 5 <", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::LT));
    v4front_free(&buf);
  }

  SUBCASE("Less than or equal: 3 5 <=")
  {
    v4front_err err = v4front_compile("3 5 <=", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::LE));
    v4front_free(&buf);
  }

  SUBCASE("Greater than: 5 3 >")
  {
    v4front_err err = v4front_compile("5 3 >", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::GT));
    v4front_free(&buf);
  }

  SUBCASE("Greater than or equal: 5 3 >=")
  {
    v4front_err err = v4front_compile("5 3 >=", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::GE));
    v4front_free(&buf);
  }
}

TEST_CASE("Complex comparison expressions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Multiple comparisons: 10 20 < 30 40 > =")
  {
    v4front_err err = v4front_compile("10 20 < 30 40 > =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    // Should compile successfully
    v4front_free(&buf);
  }

  SUBCASE("Comparison with negative numbers: -5 0 <")
  {
    v4front_err err = v4front_compile("-5 0 <", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::LT));
    v4front_free(&buf);
  }

  SUBCASE("Comparison with hex: 0xFF 255 =")
  {
    v4front_err err = v4front_compile("0xFF 255 =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::EQ));
    v4front_free(&buf);
  }
}

TEST_CASE("Comparison operators in bytecode structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify bytecode structure for: 42 42 =")
  {
    v4front_err err = v4front_compile("42 42 =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure should be:
    // [0] = LIT, [1-4] = 42 (little-endian)
    // [5] = LIT, [6-9] = 42 (little-endian)
    // [10] = EQ
    // [11] = RET

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[1] == 42);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[6] == 42);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::EQ));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
    CHECK(buf.size == 12);

    v4front_free(&buf);
  }
}