#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <v4/opcodes.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

// RAII guard for V4FrontBuf (safe even without exceptions)
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

TEST_CASE("empty source -> RET only")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1);
  CHECK(buf.data[0] == static_cast<uint8_t>(V4_OP_RET));
}

TEST_CASE("whitespace-only source -> RET only")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("  \t  \n", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1);
  CHECK(buf.data[0] == static_cast<uint8_t>(V4_OP_RET));
}

TEST_CASE("single literal -> LIT imm32 + RET")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("42", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1 + 4 + 1);
  CHECK(buf.data[0] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[1]) == 42u);
  CHECK(buf.data[5] == static_cast<uint8_t>(V4_OP_RET));
}

TEST_CASE("multiple literals and negative")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("1 2 -3", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 3 + 1);

  size_t offset = 0;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 1u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 2u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(static_cast<int32_t>(read_u32_le(&buf.data[offset])) == -3);
  offset += 4;

  CHECK(buf.data[offset] == static_cast<uint8_t>(V4_OP_RET));
}

TEST_CASE("hex and boundary literals")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile("0x10 2147483647 -2147483648", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 3 + 1);

  size_t offset = 0;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 0x10u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 2147483647u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(static_cast<int32_t>(read_u32_le(&buf.data[offset])) ==
        static_cast<int32_t>(0x80000000u));
  offset += 4;

  CHECK(buf.data[offset] == static_cast<uint8_t>(V4_OP_RET));
}

TEST_CASE("unknown token -> error + message")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128] = {0};

  int rc = v4front_compile("HELLO", &buf, err, sizeof(err));
  CHECK(rc < 0);
  CHECK(std::strlen(err) > 0);
}

TEST_CASE("compile_word wrapper passes through")
{
  V4FrontBuf buf{};
  BufferGuard guard(&buf);
  char err[128];

  int rc = v4front_compile_word("SOMEWORD", "7 8", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 2 + 1);

  size_t offset = 0;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 7u);
  offset += 4;

  CHECK(buf.data[offset++] == static_cast<uint8_t>(V4_OP_LIT));
  CHECK(read_u32_le(&buf.data[offset]) == 8u);
  offset += 4;

  CHECK(buf.data[offset] == static_cast<uint8_t>(V4_OP_RET));
}