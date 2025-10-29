#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("L++ instruction compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L++ with decimal index")
  {
    v4front_err err = v4front_compile("L++ 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LINC (0x80), [1] = index (0x00), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x80);  // LINC opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L++ with hexadecimal index")
  {
    v4front_err err = v4front_compile("L++ 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x80);  // LINC opcode
    CHECK(buf.data[1] == 0x10);  // local index = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L++ with maximum valid index (255)")
  {
    v4front_err err = v4front_compile("L++ 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x80);  // LINC opcode
    CHECK(buf.data[1] == 0xFF);  // local index = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L++ with minimum valid index (0)")
  {
    v4front_err err = v4front_compile("L++ 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x80);  // LINC opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L++ calls")
  {
    v4front_err err = v4front_compile("L++ 0 L++ 1 L++ 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LINC 0, LINC 1, LINC 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x80);  // LINC
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x80);  // LINC
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x80);  // LINC
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L++ case insensitive")
  {
    v4front_err err = v4front_compile("l++ 5", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x80);  // LINC opcode
    CHECK(buf.data[1] == 0x05);  // local index = 5

    v4front_free(&buf);
  }

  SUBCASE("Error: L++ without index")
  {
    v4front_err err = v4front_compile("L++", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -33);  // MissingLocalIdx
  }

  SUBCASE("Error: L++ with invalid index (256)")
  {
    v4front_err err = v4front_compile("L++ 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }

  SUBCASE("Error: L++ with negative index")
  {
    v4front_err err = v4front_compile("L++ -1", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }

  SUBCASE("Error: L++ with non-numeric index")
  {
    v4front_err err = v4front_compile("L++ FOO", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }

  SUBCASE("Error: L++ with index too large (1000)")
  {
    v4front_err err = v4front_compile("L++ 1000", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }
}

TEST_CASE("L-- instruction compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L-- with decimal index")
  {
    v4front_err err = v4front_compile("L-- 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LDEC (0x81), [1] = index (0x00), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x81);  // LDEC opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L-- with hexadecimal index")
  {
    v4front_err err = v4front_compile("L-- 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x81);  // LDEC opcode
    CHECK(buf.data[1] == 0x10);  // local index = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L-- with maximum valid index (255)")
  {
    v4front_err err = v4front_compile("L-- 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x81);  // LDEC opcode
    CHECK(buf.data[1] == 0xFF);  // local index = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L-- calls")
  {
    v4front_err err = v4front_compile("L-- 0 L-- 1 L-- 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LDEC 0, LDEC 1, LDEC 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x81);  // LDEC
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x81);  // LDEC
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x81);  // LDEC
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L-- case insensitive")
  {
    v4front_err err = v4front_compile("l-- 7", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x81);  // LDEC opcode
    CHECK(buf.data[1] == 0x07);  // local index = 7

    v4front_free(&buf);
  }

  SUBCASE("Error: L-- without index")
  {
    v4front_err err = v4front_compile("L--", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -33);  // MissingLocalIdx
  }

  SUBCASE("Error: L-- with invalid index (256)")
  {
    v4front_err err = v4front_compile("L-- 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }

  SUBCASE("Error: L-- with negative index")
  {
    v4front_err err = v4front_compile("L-- -1", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }
}

TEST_CASE("L++ and L-- mixed operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L++ and L-- together")
  {
    v4front_err err = v4front_compile("L++ 0 L-- 1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LINC 0, LDEC 1, RET
    REQUIRE(buf.size >= 5);
    CHECK(buf.data[0] == 0x80);  // LINC
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x81);  // LDEC
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L++ with literal before")
  {
    v4front_err err = v4front_compile("10 L++ 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LIT 10, LINC 0, RET
    REQUIRE(buf.size >= 8);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x0A);  // 10 (little-endian)
    CHECK(buf.data[5] == 0x80);  // LINC
    CHECK(buf.data[6] == 0x00);  // index 0
    CHECK(buf.data[7] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("L++ and L-- in word definitions")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L++ in word definition")
  {
    const char* source = ": INC-LOCAL L++ 0 ; INC-LOCAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LINC 0, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "INC-LOCAL") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x80);  // LINC
    CHECK(buf.words[0].code[1] == 0x00);  // index 0
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L-- in word definition")
  {
    const char* source = ": DEC-LOCAL L-- 1 ; DEC-LOCAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LDEC 1, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "DEC-LOCAL") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x81);  // LDEC
    CHECK(buf.words[0].code[1] == 0x01);  // index 1
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
