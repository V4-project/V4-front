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

TEST_CASE("Basic BEGIN/AGAIN structure")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simplest infinite loop: BEGIN AGAIN")
  {
    v4front_err err = v4front_compile("BEGIN AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure:
    // Position 0: (BEGIN) JMP offset (3 bytes)
    // Position 3: (never reached)
    //
    // JMP: target=0, next_ip=3, offset=0-3=-3

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[1]);
    CHECK(jmp_offset == -3);

    // Note: No RET after AGAIN since it's unreachable
    CHECK(buf.size == 3);

    v4front_free(&buf);
  }

  SUBCASE("Simple loop with body: BEGIN DUP AGAIN")
  {
    v4front_err err = v4front_compile("BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure:
    // Position 0: (BEGIN) DUP (1 byte)
    // Position 1: JMP offset (3 bytes)
    // Position 4: (unreachable)
    //
    // JMP: target=0, next_ip=4, offset=0-4=-4

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[2]);
    CHECK(jmp_offset == -4);

    v4front_free(&buf);
  }

  SUBCASE("Loop with initialization: 0 BEGIN 1 + DUP AGAIN")
  {
    v4front_err err =
        v4front_compile("0 BEGIN 1 + DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Structure:
    // Position  0: LIT 0       (5 bytes)
    // Position  5: (BEGIN)     LIT 1 (5 bytes)
    // Position 10: ADD         (1 byte)
    // Position 11: DUP         (1 byte)
    // Position 12: JMP offset  (3 bytes)
    //
    // JMP: target=5, next_ip=15, offset=5-15=-10

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[5] == static_cast<uint8_t>(Op::LIT));
    CHECK(buf.data[10] == static_cast<uint8_t>(Op::ADD));
    CHECK(buf.data[11] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[12] == static_cast<uint8_t>(Op::JMP));

    int16_t jmp_offset = read_i16_le(&buf.data[13]);
    CHECK(jmp_offset == -10);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/AGAIN with various operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("With arithmetic: BEGIN 2 * AGAIN")
  {
    v4front_err err = v4front_compile("BEGIN 2 * AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("With stack operations: BEGIN SWAP DUP OVER AGAIN")
  {
    v4front_err err =
        v4front_compile("BEGIN SWAP DUP OVER AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("With comparison: BEGIN DUP 100 > AGAIN")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP 100 > AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("With bitwise: BEGIN 1 - DUP 0xFF AND AGAIN")
  {
    v4front_err err =
        v4front_compile("BEGIN 1 - DUP 0xFF AND AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Nested BEGIN/AGAIN structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple nesting: BEGIN BEGIN DUP AGAIN AGAIN")
  {
    v4front_err err =
        v4front_compile("BEGIN BEGIN DUP AGAIN AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Inner AGAIN jumps to inner BEGIN
    // Outer AGAIN is unreachable
    v4front_free(&buf);
  }

  SUBCASE("Triple nesting")
  {
    v4front_err err = v4front_compile("BEGIN BEGIN BEGIN DUP AGAIN AGAIN AGAIN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("IF inside BEGIN/AGAIN")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple IF in infinite loop")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP 5 > IF 1 - THEN AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("IF/ELSE in infinite loop")
  {
    v4front_err err = v4front_compile("BEGIN DUP 10 < IF 1 + ELSE 1 - THEN AGAIN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("BEGIN/AGAIN inside IF")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Infinite loop in TRUE branch")
  {
    v4front_err err =
        v4front_compile("1 IF BEGIN DUP AGAIN THEN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // THEN is unreachable
    v4front_free(&buf);
  }

  SUBCASE("Infinite loop in ELSE branch")
  {
    v4front_err err = v4front_compile("0 IF 42 ELSE BEGIN DUP AGAIN THEN", &buf, errmsg,
                                      sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Multiple sequential BEGIN/AGAIN structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Two sequential infinite loops (first is unreachable)")
  {
    // Note: The second BEGIN is unreachable because AGAIN never exits
    v4front_err err =
        v4front_compile("BEGIN DUP AGAIN BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("AGAIN with other loop types (sequential)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("UNTIL before AGAIN (sequential)")
  {
    // First loop can exit, second is infinite
    v4front_err err =
        v4front_compile("BEGIN DUP UNTIL BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("WHILE/REPEAT before AGAIN (sequential)")
  {
    v4front_err err = v4front_compile("BEGIN DUP WHILE DROP REPEAT BEGIN DUP AGAIN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("UNTIL inside AGAIN")
  {
    v4front_err err =
        v4front_compile("BEGIN BEGIN 1 - DUP UNTIL AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }

  SUBCASE("WHILE/REPEAT inside AGAIN")
  {
    v4front_err err = v4front_compile("BEGIN BEGIN DUP WHILE DROP REPEAT AGAIN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    v4front_free(&buf);
  }
}

TEST_CASE("Error cases: malformed BEGIN/AGAIN structures")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("AGAIN without BEGIN")
  {
    v4front_err err = v4front_compile("10 DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::AgainWithoutBegin);
    CHECK(strcmp(errmsg, "AGAIN without matching BEGIN") == 0);
  }

  SUBCASE("Unclosed BEGIN (missing AGAIN, UNTIL, or REPEAT)")
  {
    v4front_err err = v4front_compile("BEGIN 10 20 +", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::UnclosedBegin);
    CHECK(strcmp(errmsg, "unclosed BEGIN structure") == 0);
  }

  SUBCASE("AGAIN after WHILE")
  {
    v4front_err err =
        v4front_compile("BEGIN DUP WHILE 1 - AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::AgainAfterWhile);
    CHECK(strcmp(errmsg, "AGAIN cannot be used after WHILE") == 0);
  }

  SUBCASE("AGAIN for IF (wrong control type)")
  {
    v4front_err err = v4front_compile("1 IF 42 AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::AgainWithoutBegin);
  }
}

TEST_CASE("Case insensitive AGAIN keyword")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Lowercase: begin again")
  {
    v4front_err err = v4front_compile("begin dup again", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed case: Begin Again")
  {
    v4front_err err = v4front_compile("Begin dup Again", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Uppercase: BEGIN AGAIN")
  {
    v4front_err err = v4front_compile("BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Practical BEGIN/AGAIN examples (conceptual)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Event loop skeleton")
  {
    // Infinite event processing loop
    v4front_err err = v4front_compile("BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Counter with overflow check (conceptual)")
  {
    v4front_err err = v4front_compile("0 BEGIN 1 + DUP 1000000 > IF DROP 0 THEN AGAIN",
                                      &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("State machine (conceptual)")
  {
    // Would use IF to dispatch to different states
    v4front_err err = v4front_compile("BEGIN DUP 1 = IF 42 ELSE 99 THEN DROP AGAIN", &buf,
                                      errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}

TEST_CASE("Backward jump offset verification")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify offset calculation")
  {
    v4front_err err = v4front_compile("BEGIN DUP AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Position 0: (BEGIN) DUP (1 byte)
    // Position 1: JMP opcode  (1 byte)
    // Position 2-3: offset    (2 bytes)
    // Position 4: (unreachable)
    //
    // JMP: target=0, next_ip=4, offset=0-4=-4

    CHECK(buf.data[0] == static_cast<uint8_t>(Op::DUP));
    CHECK(buf.data[1] == static_cast<uint8_t>(Op::JMP));

    int16_t offset = read_i16_le(&buf.data[2]);
    CHECK(offset == -4);

    v4front_free(&buf);
  }

  SUBCASE("Longer loop body")
  {
    v4front_err err =
        v4front_compile("0 BEGIN 1 + 2 * 3 - AGAIN", &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);

    // Calculate expected offset
    // Position 0: LIT 0 (5 bytes)
    // Position 5: BEGIN
    // ... body ...
    // Position N: JMP
    // next_ip = N + 3
    // offset = 5 - (N + 3)

    int pos = 0;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));
    pos += 5;  // LIT 0

    int begin_pos = pos;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));
    pos += 5;  // LIT 1
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::ADD));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));
    pos += 5;  // LIT 2
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::MUL));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::LIT));
    pos += 5;  // LIT 3
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::SUB));
    pos += 1;
    CHECK(buf.data[pos] == static_cast<uint8_t>(Op::JMP));

    int16_t offset = read_i16_le(&buf.data[pos + 1]);
    int expected_offset = begin_pos - (pos + 3);
    CHECK(offset == expected_offset);

    v4front_free(&buf);
  }
}

TEST_CASE("Deep nesting with AGAIN")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("16 nested AGAIN loops")
  {
    std::string code = "";
    for (int i = 0; i < 16; i++)
    {
      code += "BEGIN ";
    }
    code += "DUP ";
    for (int i = 0; i < 16; i++)
    {
      code += "AGAIN ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }

  SUBCASE("Mixed IF and AGAIN nesting")
  {
    std::string code = "";
    for (int i = 0; i < 16; i++)
    {
      code += "1 IF BEGIN ";
    }
    code += "DUP ";
    for (int i = 0; i < 16; i++)
    {
      code += "AGAIN THEN ";
    }

    v4front_err err = v4front_compile(code.c_str(), &buf, errmsg, sizeof(errmsg));
    CHECK(err == FrontErr::OK);
    v4front_free(&buf);
  }
}