#include "vendor/doctest/doctest.h"

#include "v4front/front_api.h"
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <v4/opcodes.h> // expects V4_OP_LIT / V4_OP_RET

// RAII guard (safe even without exceptions)
struct BufGuard
{
  V4FrontBuf *b;
  explicit BufGuard(V4FrontBuf *bb) : b(bb) {}
  ~BufGuard()
  {
    if (b)
      v4front_free(b);
  }
};

// Read little-endian u32
static uint32_t rd32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

TEST_CASE("empty source -> RET only")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile("", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1);
  CHECK(buf.data[0] == (uint8_t)V4_OP_RET);
}

TEST_CASE("whitespace-only source -> RET only")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile("  \t  \n", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1);
  CHECK(buf.data[0] == (uint8_t)V4_OP_RET);
}

TEST_CASE("single literal -> LIT imm32 + RET")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile("42", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == 1 + 4 + 1);
  CHECK(buf.data[0] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[1]) == 42u);
  CHECK(buf.data[5] == (uint8_t)V4_OP_RET);
}

TEST_CASE("multiple literals and negative")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile("1 2 -3", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 3 + 1);

  size_t k = 0;
  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 1u);
  k += 4;

  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 2u);
  k += 4;

  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK((int32_t)rd32(&buf.data[k]) == -3);
  k += 4;

  CHECK(buf.data[k] == (uint8_t)V4_OP_RET);
}

TEST_CASE("hex and boundary literals")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile("0x10 2147483647 -2147483648", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 3 + 1);

  size_t k = 0;
  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 0x10u);
  k += 4;

  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 2147483647u);
  k += 4;

  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK((int32_t)rd32(&buf.data[k]) == (int32_t)0x80000000u);
  k += 4;

  CHECK(buf.data[k] == (uint8_t)V4_OP_RET);
}

TEST_CASE("unknown token -> error + message")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128] = {0};

  int rc = v4front_compile("HELLO", &buf, err, sizeof(err));
  CHECK(rc < 0);
  CHECK(std::strlen(err) > 0);
}

TEST_CASE("compile_word wrapper passes through")
{
  V4FrontBuf buf{};
  BufGuard g(&buf);
  char err[128];

  int rc = v4front_compile_word("SOMEWORD", "7 8", &buf, err, sizeof(err));
  CHECK(rc == 0);
  REQUIRE(buf.size == (1 + 4) * 2 + 1);

  size_t k = 0;
  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 7u);
  k += 4;

  CHECK(buf.data[k++] == (uint8_t)V4_OP_LIT);
  CHECK(rd32(&buf.data[k]) == 8u);
  k += 4;

  CHECK(buf.data[k] == (uint8_t)V4_OP_RET);
}
