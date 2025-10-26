#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Compile >R (TOR)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple: 42 >R")
  {
    v4front_err err = v4front_compile("42 >R", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure: LIT 42, TOR, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::TOR));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Case insensitive: 10 >r")
  {
    v4front_err err = v4front_compile("10 >r", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Compile R> (FROMR)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple: R>")
  {
    v4front_err err = v4front_compile("R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::FROMR));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Case insensitive: r>")
  {
    v4front_err err = v4front_compile("r>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Compile R@ (RFETCH)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple: R@")
  {
    v4front_err err = v4front_compile("R@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::RFETCH));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Case insensitive: r@")
  {
    v4front_err err = v4front_compile("r@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Return stack roundtrip")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Push and pop: 99 >R R>")
  {
    v4front_err err = v4front_compile("99 >R R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Structure: LIT 99, TOR, FROMR, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::TOR));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::FROMR));
    CHECK(buf.data[7] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Push and fetch: 123 >R R@")
  {
    v4front_err err = v4front_compile("123 >R R@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::TOR));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RFETCH));

    v4front_free(&buf);
  }
}

TEST_CASE("Multiple return stack operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Multiple push/pop: 1 >R 2 >R 3 >R R> R> R>")
  {
    v4front_err err =
        v4front_compile("1 >R 2 >R 3 >R R> R> R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Multiple fetch: 42 >R R@ R@ R@")
  {
    v4front_err err = v4front_compile("42 >R R@ R@ R@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Return stack with arithmetic")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Temporary storage: 5 >R 10 20 + R> +")
  {
    v4front_err err = v4front_compile("5 >R 10 20 + R> +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);

    // Verify opcodes are present
    int pos = 0;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));  // 5
    pos += 5;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::TOR));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));  // 10
    pos += 5;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));  // 20
    pos += 5;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::ADD));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::FROMR));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::ADD));

    v4front_free(&buf);
  }
}

TEST_CASE("Return stack with stack operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("ROT using return stack: 1 2 3 >R SWAP R>")
  {
    v4front_err err = v4front_compile("1 2 3 >R SWAP R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("2DUP using return stack: DUP >R DUP R>")
  {
    v4front_err err = v4front_compile("DUP >R DUP R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Return stack with control flow")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("IF with return stack: 1 >R IF R> THEN")
  {
    v4front_err err = v4front_compile("1 >R 1 IF R> THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("BEGIN/UNTIL with return stack: 5 >R BEGIN R@ UNTIL R>")
  {
    v4front_err err =
        v4front_compile("5 >R BEGIN R@ UNTIL R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical Forth patterns")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("OVER implementation: >R DUP R> SWAP")
  {
    v4front_err err = v4front_compile(">R DUP R> SWAP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("PICK implementation simulation")
  {
    // Simplified pick pattern
    v4front_err err = v4front_compile("SWAP >R SWAP R>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Edge cases")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Only return stack operations: >R R> >R R@")
  {
    v4front_err err = v4front_compile(">R R> >R R@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case: >r R@ r>")
  {
    v4front_err err = v4front_compile(">r R@ r>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == V4FRONT_OK);
    v4front_free(&buf);
  }
}