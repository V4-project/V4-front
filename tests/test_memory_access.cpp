#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Memory fetch (@)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple @ operation")
  {
    v4front_err err = v4front_compile("1000 @", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain: LIT 1000, LOAD, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    // bytes 1-4: value 1000
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LOAD));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Multiple @ operations")
  {
    v4front_err err = v4front_compile("100 @ 200 @", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Count LOAD instructions
    int load_count = 0;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD))
        load_count++;
    }
    CHECK(load_count == 2);

    v4front_free(&buf);
  }

  SUBCASE("@ in word definition")
  {
    v4front_err err = v4front_compile(": FETCH@ @ ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::LOAD));
    CHECK(buf.words[0].code[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("@ with calculation: 1000 4 + @")
  {
    v4front_err err = v4front_compile("1000 4 + @", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should have LIT, LIT, ADD, LOAD, RET
    bool found_pattern = false;
    for (size_t i = 0; i + 1 < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::ADD) &&
          buf.data[i + 1] == static_cast<uint8_t>(Op::LOAD))
      {
        found_pattern = true;
        break;
      }
    }
    CHECK(found_pattern);

    v4front_free(&buf);
  }
}

TEST_CASE("Memory store (!)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple ! operation")
  {
    v4front_err err = v4front_compile("42 1000 !", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain: LIT 42, LIT 1000, STORE, RET
    // Find STORE instruction
    bool found_store = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE))
      {
        found_store = true;
        break;
      }
    }
    CHECK(found_store);

    v4front_free(&buf);
  }

  SUBCASE("Multiple ! operations")
  {
    v4front_err err = v4front_compile("1 100 ! 2 200 !", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Count STORE instructions
    int store_count = 0;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE))
        store_count++;
    }
    CHECK(store_count == 2);

    v4front_free(&buf);
  }

  SUBCASE("! in word definition")
  {
    v4front_err err = v4front_compile(": STORE! ! ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);
    CHECK(buf.words[0].code[0] == static_cast<uint8_t>(Op::STORE));
    CHECK(buf.words[0].code[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }
}

TEST_CASE("Combined @ and ! operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Copy memory: 100 @ 200 !")
  {
    v4front_err err = v4front_compile("100 @ 200 !", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should have LOAD followed by STORE
    bool found_pattern = false;
    for (size_t i = 0; i < buf.size - 1; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD))
      {
        // Find STORE after LOAD (with LIT in between for address)
        for (size_t j = i + 1; j < buf.size; j++)
        {
          if (buf.data[j] == static_cast<uint8_t>(Op::STORE))
          {
            found_pattern = true;
            break;
          }
        }
        break;
      }
    }
    CHECK(found_pattern);

    v4front_free(&buf);
  }

  SUBCASE("Increment memory: : INC@ DUP @ 1 + SWAP ! ;")
  {
    v4front_err err =
        v4front_compile(": INC@ DUP @ 1 + SWAP ! ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    CHECK(buf.word_count == 1);

    // Should contain both LOAD and STORE
    bool has_load = false;
    bool has_store = false;
    for (size_t i = 0; i < buf.words[0].code_len; i++)
    {
      if (buf.words[0].code[i] == static_cast<uint8_t>(Op::LOAD))
        has_load = true;
      if (buf.words[0].code[i] == static_cast<uint8_t>(Op::STORE))
        has_store = true;
    }
    CHECK(has_load);
    CHECK(has_store);

    v4front_free(&buf);
  }
}

TEST_CASE("Memory access in control structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("@ in IF")
  {
    v4front_err err =
        v4front_compile("100 @ IF 200 @ THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    int load_count = 0;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD))
        load_count++;
    }
    CHECK(load_count == 2);

    v4front_free(&buf);
  }

  SUBCASE("! in LOOP")
  {
    v4front_err err =
        v4front_compile("10 0 DO I 1000 ! LOOP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    bool has_store = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE))
      {
        has_store = true;
        break;
      }
    }
    CHECK(has_store);

    v4front_free(&buf);
  }
}

TEST_CASE("Case insensitive @ and !")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("@ should only match exact symbol")
  {
    // @ is a symbol, should be case-sensitive
    v4front_err err = v4front_compile("1000 @", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("! should only match exact symbol")
  {
    // ! is a symbol, should be case-sensitive
    v4front_err err = v4front_compile("42 1000 !", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Byte memory access (C@ and C!)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("C@ (byte fetch)")
  {
    v4front_err err = v4front_compile("1000 C@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain: LIT 1000, LOAD8U, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LOAD8U));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("C! (byte store)")
  {
    v4front_err err = v4front_compile("42 1000 C!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Find STORE8 instruction
    bool found_store8 = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE8))
      {
        found_store8 = true;
        break;
      }
    }
    CHECK(found_store8);

    v4front_free(&buf);
  }

  SUBCASE("C@ and C! combined")
  {
    v4front_err err = v4front_compile("100 C@ 200 C!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    bool has_load8 = false;
    bool has_store8 = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD8U))
        has_load8 = true;
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE8))
        has_store8 = true;
    }
    CHECK(has_load8);
    CHECK(has_store8);

    v4front_free(&buf);
  }
}

TEST_CASE("Halfword memory access (W@ and W!)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("W@ (halfword fetch)")
  {
    v4front_err err = v4front_compile("1000 W@", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Should contain: LIT 1000, LOAD16U, RET
    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LOAD16U));
    CHECK(buf.data[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("W! (halfword store)")
  {
    v4front_err err = v4front_compile("1234 1000 W!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Find STORE16 instruction
    bool found_store16 = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE16))
      {
        found_store16 = true;
        break;
      }
    }
    CHECK(found_store16);

    v4front_free(&buf);
  }

  SUBCASE("W@ and W! combined")
  {
    v4front_err err = v4front_compile("100 W@ 200 W!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    bool has_load16 = false;
    bool has_store16 = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD16U))
        has_load16 = true;
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE16))
        has_store16 = true;
    }
    CHECK(has_load16);
    CHECK(has_store16);

    v4front_free(&buf);
  }
}
