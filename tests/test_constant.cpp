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

// Read little-endian int32_t from buffer
static int32_t read_i32_le(const uint8_t* ptr)
{
  uint32_t u = static_cast<uint32_t>(ptr[0]) | (static_cast<uint32_t>(ptr[1]) << 8) |
               (static_cast<uint32_t>(ptr[2]) << 16) |
               (static_cast<uint32_t>(ptr[3]) << 24);
  return static_cast<int32_t>(u);
}

TEST_CASE("constant: basic constant definition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42 CONSTANT ANSWER", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "ANSWER") == 0);
  REQUIRE(buf.words[0].code_len == 6);  // LIT 42, RET

  const uint8_t* code = buf.words[0].code;
  size_t offset = 0;
  CHECK(code[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code[offset]) == 42);
  offset += 4;
  CHECK(code[offset++] == static_cast<uint8_t>(Op::RET));

  // Main bytecode should just be RET
  CHECK(buf.size == 1);
  CHECK(buf.data[0] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: using constant in expression")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 CONSTANT TEN  TEN 5 +", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "TEN") == 0);

  // Main bytecode: CALL TEN, LIT 5, ADD, RET
  REQUIRE(buf.size >= 10);

  size_t offset = 0;
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::CALL));
  offset += 2;  // skip word index

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&buf.data[offset]) == 5);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::ADD));
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: multiple constants")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc =
      v4front_compile("100 CONSTANT BASE  10 CONSTANT OFFSET", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 2);
  CHECK(strcmp(buf.words[0].name, "BASE") == 0);
  CHECK(strcmp(buf.words[1].name, "OFFSET") == 0);

  // Check BASE constant
  const uint8_t* code0 = buf.words[0].code;
  CHECK(code0[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code0[1]) == 100);
  CHECK(code0[5] == static_cast<uint8_t>(Op::RET));

  // Check OFFSET constant
  const uint8_t* code1 = buf.words[1].code;
  CHECK(code1[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code1[1]) == 10);
  CHECK(code1[5] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: using multiple constants")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("100 CONSTANT BASE  10 CONSTANT OFFSET  BASE OFFSET +", &buf,
                           err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 2);

  // Main bytecode: CALL BASE, CALL OFFSET, ADD, RET
  CHECK(buf.size >= 8);
}

TEST_CASE("constant: negative constant")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("-42 CONSTANT NEGATIVE", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "NEGATIVE") == 0);

  const uint8_t* code = buf.words[0].code;
  CHECK(code[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code[1]) == -42);
  CHECK(code[5] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: hexadecimal constant")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("0xFF CONSTANT MAXBYTE", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "MAXBYTE") == 0);

  const uint8_t* code = buf.words[0].code;
  CHECK(code[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code[1]) == 0xFF);
  CHECK(code[5] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: zero constant")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("0 CONSTANT ZERO", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "ZERO") == 0);

  const uint8_t* code = buf.words[0].code;
  CHECK(code[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_i32_le(&code[1]) == 0);
  CHECK(code[5] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("constant: constant in word definition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc =
      v4front_compile("10 CONSTANT TEN  : TEST TEN 2 * ;  TEST", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 2);
  CHECK(strcmp(buf.words[0].name, "TEN") == 0);
  CHECK(strcmp(buf.words[1].name, "TEST") == 0);
}

TEST_CASE("constant: constant with comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42 ( the answer ) CONSTANT ANSWER \\ Douglas Adams", &buf,
                           err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "ANSWER") == 0);
}

TEST_CASE("constant: error - constant without value")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("CONSTANT FOO", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_ConstantWithoutValue);
}

TEST_CASE("constant: error - constant without name")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42 CONSTANT", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_ConstantWithoutName);
}

TEST_CASE("constant: error - duplicate constant name")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 CONSTANT FOO  20 CONSTANT FOO", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_DuplicateWord);
}

TEST_CASE("constant: error - constant after non-literal")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // DUP is not a literal, so CONSTANT should fail
  int rc = v4front_compile("10 DUP CONSTANT FOO", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_ConstantWithoutValue);
}

TEST_CASE("constant: case insensitive")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42 constant answer  ANSWER", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "answer") == 0);
}

TEST_CASE("constant: using constant before definition should fail")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("UNDEFINED 10 CONSTANT DEFINED", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_UnknownToken);
}
