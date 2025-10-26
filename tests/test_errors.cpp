#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.h"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;

TEST_CASE("Error codes are properly defined")
{
  CHECK(front_err_to_int(FrontErr::OK) == 0);
  CHECK(front_err_to_int(FrontErr::UnknownToken) == -1);
  CHECK(front_err_to_int(FrontErr::InvalidInteger) == -2);
  CHECK(front_err_to_int(FrontErr::OutOfMemory) == -3);
}

TEST_CASE("Error message retrieval")
{
  CHECK(strcmp(front_err_str(FrontErr::OK), "ok") == 0);
  CHECK(strcmp(front_err_str(FrontErr::UnknownToken), "unknown token") == 0);
  CHECK(strcmp(front_err_str(FrontErr::InvalidInteger), "invalid integer format") == 0);
}

TEST_CASE("C API error string function")
{
  // C++ overload allows passing FrontErr directly to v4front_err_str
  CHECK(strcmp(v4front_err_str(FrontErr::OK), "ok") == 0);
  CHECK(strcmp(v4front_err_str(FrontErr::UnknownToken), "unknown token") == 0);
}

TEST_CASE("Empty source compiles successfully")
{
  V4FrontBuf buf;
  char errmsg[256];

  v4front_err err = v4front_compile("", &buf, errmsg, sizeof(errmsg));

  CHECK(err == FrontErr::OK);
  CHECK(buf.size == 1);
  CHECK(buf.data[0] == V4_OP_RET);

  v4front_free(&buf);
}

TEST_CASE("Simple integer literal")
{
  V4FrontBuf buf;
  char errmsg[256];

  v4front_err err = v4front_compile("42", &buf, errmsg, sizeof(errmsg));

  CHECK(err == FrontErr::OK);
  CHECK(buf.size == 6);  // LIT + 4 bytes + RET
  CHECK(buf.data[0] == V4_OP_LIT);
  CHECK(buf.data[5] == V4_OP_RET);

  v4front_free(&buf);
}

TEST_CASE("Arithmetic operations compile correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Addition: 10 20 +")
  {
    v4front_err err = v4front_compile("10 20 +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[0] == V4_OP_LIT);  // 10
    CHECK(buf.data[5] == V4_OP_LIT);  // 20
    CHECK(buf.data[10] == V4_OP_ADD);
    CHECK(buf.data[11] == V4_OP_RET);
    v4front_free(&buf);
  }

  SUBCASE("Multiplication: 6 7 *")
  {
    v4front_err err = v4front_compile("6 7 *", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == V4_OP_MUL);
    v4front_free(&buf);
  }

  SUBCASE("Division: 42 7 /")
  {
    v4front_err err = v4front_compile("42 7 /", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == V4_OP_DIV);
    v4front_free(&buf);
  }

  SUBCASE("Modulus: 43 7 MOD")
  {
    v4front_err err = v4front_compile("43 7 MOD", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.data[10] == V4_OP_MOD);
    v4front_free(&buf);
  }
}

TEST_CASE("Unknown token returns proper error code")
{
  V4FrontBuf buf;
  char errmsg[256];

  v4front_err err = v4front_compile("10 UNKNOWN 20", &buf, errmsg, sizeof(errmsg));

  CHECK(err == FrontErr::UnknownToken);
  CHECK(strcmp(errmsg, "unknown token") == 0);
}

TEST_CASE("Error message buffer works correctly")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Error message is written")
  {
    v4front_err err = v4front_compile("invalid!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnknownToken);
    CHECK(strlen(errmsg) > 0);
  }

  SUBCASE("NULL error buffer is safe")
  {
    v4front_err err = v4front_compile("invalid!", &buf, nullptr, 0);
    CHECK(err == FrontErr::UnknownToken);
  }
}

TEST_CASE("v4front_compile_word behaves like v4front_compile")
{
  V4FrontBuf buf;
  char errmsg[256];

  v4front_err err = v4front_compile_word("test", "10 20 +", &buf, errmsg, sizeof(errmsg));

  CHECK(err == FrontErr::OK);
  CHECK(buf.size > 0);

  v4front_free(&buf);
}

TEST_CASE("Hex and negative integers")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Hexadecimal: 0xFF")
  {
    v4front_err err = v4front_compile("0xFF", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Negative: -42")
  {
    v4front_err err = v4front_compile("-42", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}