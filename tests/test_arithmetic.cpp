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

TEST_CASE("arithmetic: simple addition")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("10 20 +", &buf, err, sizeof(err));
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
  CHECK(buf.data[offset] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: subtraction")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("50 30 -", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);

  CHECK(buf.data[10] == static_cast<uint8_t>(Op::SUB));
  CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: multiplication")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("6 7 *", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);

  CHECK(buf.data[10] == static_cast<uint8_t>(Op::MUL));
  CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: division")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42 7 /", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);

  CHECK(buf.data[10] == static_cast<uint8_t>(Op::DIV));
  CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: modulus")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("43 7 MOD", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 12);

  CHECK(buf.data[10] == static_cast<uint8_t>(Op::MOD));
  CHECK(buf.data[11] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: complex expression (1 + 2) * 3")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("1 2 + 3 *", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 18);  // LIT 1, LIT 2, ADD, LIT 3, MUL, RET

  size_t offset = 0;
  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 1u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 2u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::ADD));

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 3u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(Op::MUL));
  CHECK(buf.data[offset] == static_cast<uint8_t>(Op::RET));
}

TEST_CASE("arithmetic: unknown operator -> error")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128] = {0};

  int rc = v4front_compile("10 20 UNKNOWN", &buf, err, sizeof(err));
  CHECK(rc < 0);
  CHECK(std::strlen(err) > 0);
}

TEST_CASE("arithmetic: literals still work")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 6);

  CHECK(buf.data[0] == static_cast<uint8_t>(Op::LIT));
  CHECK(read_u32_le(&buf.data[1]) == 42u);
  CHECK(buf.data[5] == static_cast<uint8_t>(Op::RET));
}