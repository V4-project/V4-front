#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstdint>
#include <cstring>
#include <v4/opcodes.hpp>

#include "v4front/compile.hpp"
#include "vendor/doctest/doctest.h"

using Op = v4::Op;

// RAII guard for V4FrontBuf
struct BufferGuard
{
  V4FrontBuf* buf;

  explicit BufferGuard(V4FrontBuf* buffer) : buf(buffer) {}

  ~BufferGuard()
  {
    if (buf)
      v4front_free(buf);
  }
};

// Read little-endian uint32_t from buffer
static uint32_t read_u32_le(const uint8_t* ptr)
{
  return static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
         (static_cast<uint32_t>(ptr[2]) << 16) | (static_cast<uint32_t>(ptr[3]) << 24);
}

TEST_CASE("variable: basic variable definition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE counter", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "counter") == 0);
  REQUIRE(buf.words[0].code_len == 6);  // LIT <addr>, RET

  const uint8_t* code = buf.words[0].code;
  size_t offset = 0;
  CHECK(code[offset++] == static_cast<uint8_t>(Op::LIT));
  uint32_t addr = read_u32_le(&code[offset]);
  CHECK(addr == 0x10000);  // Default DATA_SPACE_BASE
  offset += 4;
  CHECK(code[offset++] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("variable: multiple variables have different addresses")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE X  VARIABLE Y  VARIABLE Z", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 3);

  // Extract addresses from each variable word
  const uint8_t* code_x = buf.words[0].code;
  uint32_t addr_x = read_u32_le(&code_x[1]);

  const uint8_t* code_y = buf.words[1].code;
  uint32_t addr_y = read_u32_le(&code_y[1]);

  const uint8_t* code_z = buf.words[2].code;
  uint32_t addr_z = read_u32_le(&code_z[1]);

  // Variables should be 4 bytes apart (aligned)
  CHECK(addr_x == 0x10000);
  CHECK(addr_y == 0x10004);
  CHECK(addr_z == 0x10008);
}

TEST_CASE("variable: using variable with @ and !")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE X  100 X !", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "X") == 0);

  // Main bytecode: LIT 100, CALL X, STORE, RET
  REQUIRE(buf.size >= 10);

  size_t offset = 0;
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 100u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::CALL));
  offset += 2;  // skip word index

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::STORE));
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("variable: reading and writing variable")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE X  42 X !  X @", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);

  // Main bytecode: LIT 42, CALL X, STORE, CALL X, LOAD, RET
  CHECK(buf.size >= 14);
}

TEST_CASE("variable: variable in word definition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE VAR  : SET-VAR 100 VAR ! ;  : GET-VAR VAR @ ;", &buf,
                           err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 3);
  CHECK(strcmp(buf.words[0].name, "VAR") == 0);
  CHECK(strcmp(buf.words[1].name, "SET-VAR") == 0);
  CHECK(strcmp(buf.words[2].name, "GET-VAR") == 0);
}

TEST_CASE("variable: variable with comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE ( loop ) counter \\ for counting", &buf, err,
                           sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "counter") == 0);
}

TEST_CASE("variable: error - variable without name")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_VariableWithoutName);
}

TEST_CASE("variable: error - duplicate variable name")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE FOO  VARIABLE FOO", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_DuplicateWord);
}

TEST_CASE("variable: case insensitive")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("variable myvar  MYVAR", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "myvar") == 0);
}

TEST_CASE("variable: combining constants and variables")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc =
      v4front_compile("10 CONSTANT TEN  VARIABLE X  TEN X !", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 2);
  CHECK(strcmp(buf.words[0].name, "TEN") == 0);
  CHECK(strcmp(buf.words[1].name, "X") == 0);

  // TEN is a constant with value 10
  const uint8_t* const_code = buf.words[0].code;
  CHECK(const_code[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&const_code[1]) == 10u);

  // X is a variable with address 0x10000
  const uint8_t* var_code = buf.words[1].code;
  CHECK(var_code[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&var_code[1]) == 0x10000u);
}

TEST_CASE("variable: variable address calculation")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("VARIABLE A  VARIABLE B  A B SWAP -", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 2);

  // Main bytecode should calculate B - A = 4 (variables are 4 bytes apart)
  // CALL A, CALL B, SWAP, SUB, RET
  CHECK(buf.size >= 9);
}

TEST_CASE("variable: many variables")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // Create 10 variables
  int rc = v4front_compile(
      "VARIABLE V0  VARIABLE V1  VARIABLE V2  VARIABLE V3  VARIABLE V4  "
      "VARIABLE V5  VARIABLE V6  VARIABLE V7  VARIABLE V8  VARIABLE V9",
      &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 10);

  // Check that addresses are properly allocated
  for (int i = 0; i < 10; i++)
  {
    const uint8_t* code = buf.words[i].code;
    uint32_t addr = read_u32_le(&code[1]);
    CHECK(addr == 0x10000u + i * 4);
  }
}

TEST_CASE("variable: using variable before definition should fail")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("UNDEFINED VARIABLE X", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_UnknownToken);
}

TEST_CASE("variable: variable and constant with same name should fail")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 CONSTANT FOO  VARIABLE FOO", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_DuplicateWord);
}

TEST_CASE("variable: increment variable value")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // X @ 1 + X !  (increment X)
  int rc = v4front_compile("VARIABLE X  10 X !  X @ 1 + X !", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
}

TEST_CASE("variable: +! for adding to variable")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // 5 X +!  (add 5 to X)
  int rc = v4front_compile("VARIABLE X  10 X !  5 X +!", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);

  // Check that +! is compiled correctly (it's a composite word)
  CHECK(buf.size > 0);
}
