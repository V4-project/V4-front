#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;
using Op = v4::Op;

TEST_CASE("Composite words: simple stack manipulation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("ROT: 3-element rotation")
  {
    // ROT ( a b c -- b c a )
    v4front_err err = v4front_compile(": TEST ROT ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: TOR, SWAP, FROMR, SWAP, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::TOR));
    CHECK(code[1] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[2] == static_cast<uint8_t>(Op::FROMR));
    CHECK(code[3] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[4] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("NIP: remove second item")
  {
    // NIP ( a b -- b )
    v4front_err err = v4front_compile(": TEST NIP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: SWAP, DROP, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[1] == static_cast<uint8_t>(Op::DROP));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("TUCK: insert copy under second")
  {
    // TUCK ( a b -- b a b )
    v4front_err err = v4front_compile(": TEST TUCK ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: SWAP, OVER, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[1] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: arithmetic")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("NEGATE: sign negation")
  {
    // NEGATE ( n -- -n )
    v4front_err err = v4front_compile(": TEST NEGATE ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LIT0, SWAP, SUB, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[1] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[2] == static_cast<uint8_t>(Op::SUB));
    CHECK(code[3] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("ABS: absolute value")
  {
    // ABS ( n -- |n| )
    v4front_err err = v4front_compile(": TEST ABS ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: DUP, LIT0, LT, JZ, NEGATE sequence
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(code[1] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[2] == static_cast<uint8_t>(Op::LT));
    CHECK(code[3] == static_cast<uint8_t>(Op::JZ));

    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: conditional")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("?DUP: conditional duplicate")
  {
    // ?DUP ( x -- 0 | x x )
    v4front_err err = v4front_compile(": TEST ?DUP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: DUP, DUP, JZ, DUP
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(code[1] == static_cast<uint8_t>(Op::DUP));
    CHECK(code[2] == static_cast<uint8_t>(Op::JZ));
    // Skip offset check (2 bytes)
    CHECK(code[5] == static_cast<uint8_t>(Op::DUP));

    v4front_free(&buf);
  }

  SUBCASE("MIN: minimum of two values")
  {
    // MIN ( a b -- min )
    v4front_err err = v4front_compile(": TEST MIN ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: OVER, OVER, LT, JZ, DROP, JMP, SWAP, DROP
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[1] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[2] == static_cast<uint8_t>(Op::LT));
    CHECK(code[3] == static_cast<uint8_t>(Op::JZ));

    v4front_free(&buf);
  }

  SUBCASE("MAX: maximum of two values")
  {
    // MAX ( a b -- max )
    v4front_err err = v4front_compile(": TEST MAX ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: OVER, OVER, GT, JZ, DROP, JMP, SWAP, DROP
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[1] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[2] == static_cast<uint8_t>(Op::GT));
    CHECK(code[3] == static_cast<uint8_t>(Op::JZ));

    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: case insensitive")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("rot (lowercase)")
  {
    v4front_err err = v4front_compile(": test rot ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);
    v4front_free(&buf);
  }

  SUBCASE("NEGATE (uppercase)")
  {
    v4front_err err = v4front_compile(": TEST NEGATE ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);
    v4front_free(&buf);
  }

  SUBCASE("?dup (mixed case)")
  {
    v4front_err err = v4front_compile(": test ?dup ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);
    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: in expressions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("ROT in expression")
  {
    v4front_err err = v4front_compile("1 2 3 ROT", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("NEGATE in expression")
  {
    v4front_err err = v4front_compile("5 NEGATE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("MIN in expression")
  {
    v4front_err err = v4front_compile("10 20 MIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("MAX in expression")
  {
    v4front_err err = v4front_compile("10 20 MAX", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("ABS in expression")
  {
    v4front_err err = v4front_compile("-42 ABS", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("?DUP in expression")
  {
    v4front_err err = v4front_compile("5 ?DUP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: zero comparisons")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("0=: test if zero")
  {
    // 0= ( n -- flag )
    v4front_err err = v4front_compile(": TEST 0= ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LIT0, EQ, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[1] == static_cast<uint8_t>(Op::EQ));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("0<: test if less than zero")
  {
    // 0< ( n -- flag )
    v4front_err err = v4front_compile(": TEST 0< ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LIT0, LT, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[1] == static_cast<uint8_t>(Op::LT));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("0>: test if greater than zero")
  {
    // 0> ( n -- flag )
    v4front_err err = v4front_compile(": TEST 0> ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LIT0, GT, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[1] == static_cast<uint8_t>(Op::GT));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("Zero comparisons in expressions")
  {
    v4front_err err = v4front_compile("0 0= 5 0= -3 0< 10 0>", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: double-cell stack operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("2DUP: duplicate top two items")
  {
    // 2DUP ( a b -- a b a b )
    v4front_err err = v4front_compile(": TEST 2DUP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: OVER, OVER, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[1] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("2DROP: drop top two items")
  {
    // 2DROP ( a b -- )
    v4front_err err = v4front_compile(": TEST 2DROP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: DROP, DROP, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::DROP));
    CHECK(code[1] == static_cast<uint8_t>(Op::DROP));
    CHECK(code[2] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("2SWAP: swap top two pairs")
  {
    // 2SWAP ( a b c d -- c d a b )
    v4front_err err = v4front_compile(": TEST 2SWAP ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: ROT, TOR, ROT, FROMR, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::TOR));
    CHECK(code[1] == static_cast<uint8_t>(Op::SWAP));
    CHECK(code[2] == static_cast<uint8_t>(Op::FROMR));
    CHECK(code[3] == static_cast<uint8_t>(Op::SWAP));
    // Second ROT
    CHECK(code[4] == static_cast<uint8_t>(Op::TOR));

    v4front_free(&buf);
  }

  SUBCASE("2OVER: copy second pair")
  {
    // 2OVER ( a b c d -- a b c d a b )
    v4front_err err = v4front_compile(": TEST 2OVER ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: TOR, TOR, OVER, OVER, FROMR, FROMR, then 2SWAP
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::TOR));
    CHECK(code[1] == static_cast<uint8_t>(Op::TOR));
    CHECK(code[2] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[3] == static_cast<uint8_t>(Op::OVER));
    CHECK(code[4] == static_cast<uint8_t>(Op::FROMR));
    CHECK(code[5] == static_cast<uint8_t>(Op::FROMR));

    v4front_free(&buf);
  }

  SUBCASE("Double-cell operations in expressions")
  {
    v4front_err err = v4front_compile("1 2 2DUP 3 4 2SWAP 2DROP", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: memory addition")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("+!: add to memory")
  {
    // +! ( n addr -- )
    v4front_err err = v4front_compile(": TEST +! ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: DUP, TOR, LOAD, ADD, FROMR, STORE, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(code[1] == static_cast<uint8_t>(Op::TOR));
    CHECK(code[2] == static_cast<uint8_t>(Op::LOAD));
    CHECK(code[3] == static_cast<uint8_t>(Op::ADD));
    CHECK(code[4] == static_cast<uint8_t>(Op::FROMR));
    CHECK(code[5] == static_cast<uint8_t>(Op::STORE));
    CHECK(code[6] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("+! in expression")
  {
    v4front_err err = v4front_compile("5 1000 +!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    bool has_load = false;
    bool has_store = false;
    bool has_add = false;
    for (size_t i = 0; i < buf.size; i++)
    {
      if (buf.data[i] == static_cast<uint8_t>(Op::LOAD))
        has_load = true;
      if (buf.data[i] == static_cast<uint8_t>(Op::STORE))
        has_store = true;
      if (buf.data[i] == static_cast<uint8_t>(Op::ADD))
        has_add = true;
    }
    CHECK(has_load);
    CHECK(has_store);
    CHECK(has_add);

    v4front_free(&buf);
  }

  SUBCASE("+! with calculation")
  {
    v4front_err err = v4front_compile("10 5 + 1000 +!", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Composite words: boolean constants")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("TRUE: true flag constant")
  {
    // TRUE ( -- -1 )
    v4front_err err = v4front_compile(": TEST TRUE ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LITN1, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LITN1));
    CHECK(code[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("FALSE: false flag constant")
  {
    // FALSE ( -- 0 )
    v4front_err err = v4front_compile(": TEST FALSE ;", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    CHECK(buf.word_count == 1);

    // Should contain: LIT0, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == static_cast<uint8_t>(Op::LIT0));
    CHECK(code[1] == static_cast<uint8_t>(Op::RET));

    v4front_free(&buf);
  }

  SUBCASE("TRUE and FALSE in expressions")
  {
    v4front_err err = v4front_compile("TRUE FALSE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("TRUE and FALSE case insensitive")
  {
    v4front_err err = v4front_compile("true false TrUe FaLsE", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}
