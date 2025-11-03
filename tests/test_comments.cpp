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

TEST_CASE("comments: line comment at end")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 20 + \\ this is a comment", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET

  size_t offset = 0;
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 10u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 20u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::ADD));
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("comments: line comment in middle")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 \\ skip this\n 20 +", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: multiple line comments")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 \\ first\n 20 \\ second\n +", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: parenthesized comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 ( skip this ) 20 +", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: multi-line parenthesized comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 ( this is\n a multi-line\n comment ) 20 +", &buf, err,
                           sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: nested parentheses in comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // Note: Nested parentheses are not supported - will close at first )
  int rc = v4front_compile("10 ( outer ( inner ) outer ) 20 +", &buf, err, sizeof(err));
  // The comment closes at the first ), so "outer ) 20 +" remains
  // This will cause "outer" to be an unknown token
  CHECK(rc != 0);  // Should fail with unknown token
}

TEST_CASE("comments: parenthesized comment must be followed by whitespace")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  // "(LOCAL)" should not be treated as a comment because no whitespace after (
  // This test verifies that ( without trailing whitespace is not treated as a comment
  int rc = v4front_compile("10 (LOCAL) 20 +", &buf, err, sizeof(err));
  // This should fail with UnknownToken since (LOCAL) is not implemented yet
  CHECK(rc != 0);
}

TEST_CASE("comments: unterminated parenthesized comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 ( this is not closed", &buf, err, sizeof(err));
  CHECK(rc == V4FRONT_ERR_UnterminatedComment);
}

TEST_CASE("comments: mixed line and parenthesized comments")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 ( paren comment ) \\ line comment\n 20 +", &buf, err,
                           sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: comment in word definition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile(": DOUBLE ( n -- 2n ) 2 * ; \\ double a number\n 5 DOUBLE",
                           &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "DOUBLE") == 0);
}

TEST_CASE("comments: comment after colon before word name")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile(": ( comment ) FOO 42 ;", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.word_count == 1);
  CHECK(strcmp(buf.words[0].name, "FOO") == 0);
}

TEST_CASE("comments: comment after SYS keyword")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("SYS ( system call ) 10", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 3);  // SYS 10, RET

  CHECK(buf.data[0] == static_cast<uint8_t>(Op::SYS));
  CHECK(buf.data[1] == 10);
  CHECK(buf.data[2] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("comments: comment after L@ keyword")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("L@ ( get local ) 0", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 3);  // LGET 0, RET

  CHECK(buf.data[0] == static_cast<uint8_t>(Op::LGET));
  CHECK(buf.data[1] == 0);
  CHECK(buf.data[2] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("comments: empty parenthesized comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 ( ) 20 +", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);  // LIT 10, LIT 20, ADD, RET
}

TEST_CASE("comments: only comments")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("\\ just a comment", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.size == 1);  // Just RET
  CHECK(buf.data[0] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("comments: only parenthesized comment")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("( just a comment )", &buf, err, sizeof(err));
  CHECK(rc == 0);
  CHECK(buf.size == 1);  // Just RET
  CHECK(buf.data[0] == static_cast<uint8_t>(Op::RET));
}
