#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Stack operators compile correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("DUP: 42 DUP")
  {
    v4front_err err = v4front_compile("42 DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Structure: LIT(5) + DUP(1) + RET(1) = 7 bytes
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    v4front_free(&buf);
  }

  SUBCASE("DROP: 42 DROP")
  {
    v4front_err err = v4front_compile("42 DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DROP));
    v4front_free(&buf);
  }

  SUBCASE("SWAP: 1 2 SWAP")
  {
    v4front_err err = v4front_compile("1 2 SWAP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Structure: LIT(5) + LIT(5) + SWAP(1) + RET(1) = 12 bytes
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SWAP));
    v4front_free(&buf);
  }

  SUBCASE("OVER: 1 2 OVER")
  {
    v4front_err err = v4front_compile("1 2 OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::OVER));
    v4front_free(&buf);
  }
}

TEST_CASE("Stack operators in bytecode structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify bytecode for: 10 DUP")
  {
    v4front_err err = v4front_compile("10 DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure:
    // [0] = LIT, [1-4] = 10 (little-endian)
    // [5] = DUP
    // [6] = RET

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[1] == 10);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));
    CHECK(buf.size == 7);

    v4front_free(&buf);
  }

  SUBCASE("Verify bytecode for: 3 7 SWAP")
  {
    v4front_err err = v4front_compile("3 7 SWAP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure:
    // [0] = LIT, [1-4] = 3
    // [5] = LIT, [6-9] = 7
    // [10] = SWAP
    // [11] = RET

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[1] == 3);
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[6] == 7);
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::SWAP));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
    CHECK(buf.size == 12);

    v4front_free(&buf);
  }
}

TEST_CASE("Complex stack manipulation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("DUP DUP: 5 DUP DUP")
  {
    v4front_err err = v4front_compile("5 DUP DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should produce: 5 5 5
    v4front_free(&buf);
  }

  SUBCASE("SWAP DROP: 1 2 SWAP DROP")
  {
    v4front_err err = v4front_compile("1 2 SWAP DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("OVER OVER: 1 2 OVER OVER")
  {
    v4front_err err = v4front_compile("1 2 OVER OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should produce stack: 1 2 1 2
    v4front_free(&buf);
  }

  SUBCASE("ROT pattern: a b c SWAP OVER")
  {
    v4front_err err = v4front_compile("1 2 3 SWAP OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Stack operations with arithmetic")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Square: 7 DUP *")
  {
    v4front_err err = v4front_compile("7 DUP *", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should compute 7 * 7 = 49
    v4front_free(&buf);
  }

  SUBCASE("Add then DUP: 3 4 + DUP")
  {
    v4front_err err = v4front_compile("3 4 + DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should produce: 7 7
    v4front_free(&buf);
  }

  SUBCASE("OVER with ADD: 10 20 OVER +")
  {
    v4front_err err = v4front_compile("10 20 OVER +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Stack: 10 20 10 → 10 30
    v4front_free(&buf);
  }

  SUBCASE("Clean stack: 1 2 3 DROP DROP")
  {
    v4front_err err = v4front_compile("1 2 3 DROP DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should leave only: 1
    v4front_free(&buf);
  }
}

TEST_CASE("Stack operations with comparison")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Compare duplicates: 5 DUP =")
  {
    v4front_err err = v4front_compile("5 DUP =", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should always be true (-1)
    v4front_free(&buf);
  }

  SUBCASE("SWAP before compare: 3 5 SWAP <")
  {
    v4front_err err = v4front_compile("3 5 SWAP <", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // 5 3 < = false (0)
    v4front_free(&buf);
  }
}

TEST_CASE("Stack operations with bitwise")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("DUP with AND: 0xFF DUP AND")
  {
    v4front_err err = v4front_compile("0xFF DUP AND", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // 0xFF & 0xFF = 0xFF
    v4front_free(&buf);
  }

  SUBCASE("OVER with XOR: 0xAA 0x55 OVER XOR")
  {
    v4front_err err = v4front_compile("0xAA 0x55 OVER XOR", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Stack: 0xAA 0x55 0xAA → 0xAA 0xFF
    v4front_free(&buf);
  }
}

TEST_CASE("Practical stack patterns")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("2DUP pattern: a b OVER OVER")
  {
    v4front_err err = v4front_compile("10 20 OVER OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Simulates 2DUP: ( a b -- a b a b )
    v4front_free(&buf);
  }

  SUBCASE("NIP pattern: a b SWAP DROP")
  {
    v4front_err err = v4front_compile("10 20 SWAP DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Simulates NIP: ( a b -- b )
    v4front_free(&buf);
  }

  SUBCASE("TUCK pattern: a b SWAP OVER")
  {
    v4front_err err = v4front_compile("10 20 SWAP OVER", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Simulates TUCK: ( a b -- b a b )
    v4front_free(&buf);
  }
}

TEST_CASE("Edge cases")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Multiple DUPs: 42 DUP DUP DUP DUP")
  {
    v4front_err err = v4front_compile("42 DUP DUP DUP DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should produce: 42 42 42 42 42
    v4front_free(&buf);
  }

  SUBCASE("Alternating SWAP: 1 2 SWAP SWAP SWAP")
  {
    v4front_err err = v4front_compile("1 2 SWAP SWAP SWAP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    // Should end with: 2 1
    v4front_free(&buf);
  }

  SUBCASE("Mix all stack ops: 1 2 3 OVER SWAP DROP DUP")
  {
    v4front_err err =
        v4front_compile("1 2 3 OVER SWAP DROP DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}