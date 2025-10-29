#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("L@ instruction compilation (LGET)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@ with decimal index")
  {
    v4front_err err = v4front_compile("L@ 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LGET (0x79), [1] = index (0x00), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x79);  // LGET opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L@ with hexadecimal index")
  {
    v4front_err err = v4front_compile("L@ 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x79);  // LGET opcode
    CHECK(buf.data[1] == 0x10);  // local index = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L@ with maximum valid index (255)")
  {
    v4front_err err = v4front_compile("L@ 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x79);  // LGET opcode
    CHECK(buf.data[1] == 0xFF);  // local index = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L@ calls")
  {
    v4front_err err = v4front_compile("L@ 0 L@ 1 L@ 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET 0, LGET 1, LGET 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x79);  // LGET
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x79);  // LGET
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x79);  // LGET
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L@ case insensitive")
  {
    v4front_err err = v4front_compile("l@ 5", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x79);  // LGET opcode
    CHECK(buf.data[1] == 0x05);  // local index = 5

    v4front_free(&buf);
  }

  SUBCASE("Error: L@ without index")
  {
    v4front_err err = v4front_compile("L@", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -33);  // MissingLocalIdx
  }

  SUBCASE("Error: L@ with invalid index (256)")
  {
    v4front_err err = v4front_compile("L@ 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }

  SUBCASE("Error: L@ with negative index")
  {
    v4front_err err = v4front_compile("L@ -1", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }
}

TEST_CASE("L! instruction compilation (LSET)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L! with decimal index")
  {
    v4front_err err = v4front_compile("L! 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LSET (0x7A), [1] = index (0x00), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7A);  // LSET opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L! with hexadecimal index")
  {
    v4front_err err = v4front_compile("L! 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7A);  // LSET opcode
    CHECK(buf.data[1] == 0x10);  // local index = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L! with maximum valid index (255)")
  {
    v4front_err err = v4front_compile("L! 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7A);  // LSET opcode
    CHECK(buf.data[1] == 0xFF);  // local index = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L! calls")
  {
    v4front_err err = v4front_compile("L! 0 L! 1 L! 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LSET 0, LSET 1, LSET 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x7A);  // LSET
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x7A);  // LSET
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x7A);  // LSET
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L! case insensitive")
  {
    v4front_err err = v4front_compile("l! 7", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7A);  // LSET opcode
    CHECK(buf.data[1] == 0x07);  // local index = 7

    v4front_free(&buf);
  }

  SUBCASE("Error: L! without index")
  {
    v4front_err err = v4front_compile("L!", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -33);  // MissingLocalIdx
  }

  SUBCASE("Error: L! with invalid index (256)")
  {
    v4front_err err = v4front_compile("L! 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }
}

TEST_CASE("L>! instruction compilation (LTEE)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L>! with decimal index")
  {
    v4front_err err = v4front_compile("L>! 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LTEE (0x7B), [1] = index (0x00), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7B);  // LTEE opcode
    CHECK(buf.data[1] == 0x00);  // local index = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L>! with hexadecimal index")
  {
    v4front_err err = v4front_compile("L>! 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7B);  // LTEE opcode
    CHECK(buf.data[1] == 0x10);  // local index = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L>! with maximum valid index (255)")
  {
    v4front_err err = v4front_compile("L>! 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7B);  // LTEE opcode
    CHECK(buf.data[1] == 0xFF);  // local index = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L>! calls")
  {
    v4front_err err = v4front_compile("L>! 0 L>! 1 L>! 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LTEE 0, LTEE 1, LTEE 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x7B);  // LTEE
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x7B);  // LTEE
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x7B);  // LTEE
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L>! case insensitive")
  {
    v4front_err err = v4front_compile("l>! 3", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7B);  // LTEE opcode
    CHECK(buf.data[1] == 0x03);  // local index = 3

    v4front_free(&buf);
  }

  SUBCASE("Error: L>! without index")
  {
    v4front_err err = v4front_compile("L>!", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -33);  // MissingLocalIdx
  }

  SUBCASE("Error: L>! with invalid index (256)")
  {
    v4front_err err = v4front_compile("L>! 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -34);  // InvalidLocalIdx
  }
}

TEST_CASE("Mixed local variable operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@ and L! together")
  {
    v4front_err err = v4front_compile("L@ 0 L! 1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET 0, LSET 1, RET
    REQUIRE(buf.size >= 5);
    CHECK(buf.data[0] == 0x79);  // LGET
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x7A);  // LSET
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L@ L>! L! combination")
  {
    v4front_err err = v4front_compile("L@ 0 L>! 1 L! 2", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET 0, LTEE 1, LSET 2, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x79);  // LGET
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x7B);  // LTEE
    CHECK(buf.data[3] == 0x01);  // index 1
    CHECK(buf.data[4] == 0x7A);  // LSET
    CHECK(buf.data[5] == 0x02);  // index 2
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("Literal then L!")
  {
    v4front_err err = v4front_compile("42 L! 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LIT 42, LSET 0, RET
    REQUIRE(buf.size >= 8);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x2A);  // 42 (little-endian)
    CHECK(buf.data[5] == 0x7A);  // LSET
    CHECK(buf.data[6] == 0x00);  // index 0
    CHECK(buf.data[7] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L@ then arithmetic")
  {
    v4front_err err = v4front_compile("L@ 0 1 +", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET 0, LIT 1, ADD, RET
    REQUIRE(buf.size >= 9);
    CHECK(buf.data[0] == 0x79);  // LGET
    CHECK(buf.data[1] == 0x00);  // index 0
    CHECK(buf.data[2] == 0x00);  // LIT
    CHECK(buf.data[3] == 0x01);  // 1
    CHECK(buf.data[7] == 0x10);  // ADD
    CHECK(buf.data[8] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("Local variable operations in word definitions")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@ in word definition")
  {
    const char* source = ": GET-LOCAL L@ 0 ; GET-LOCAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LGET 0, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "GET-LOCAL") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x79);  // LGET
    CHECK(buf.words[0].code[1] == 0x00);  // index 0
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L! in word definition")
  {
    const char* source = ": SET-LOCAL L! 1 ; SET-LOCAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LSET 1, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "SET-LOCAL") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x7A);  // LSET
    CHECK(buf.words[0].code[1] == 0x01);  // index 1
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L>! in word definition")
  {
    const char* source = ": TEE-LOCAL L>! 2 ; TEE-LOCAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LTEE 2, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "TEE-LOCAL") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x7B);  // LTEE
    CHECK(buf.words[0].code[1] == 0x02);  // index 2
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("Complex word with local variables")
  {
    // Store value to local 0, retrieve it, add 1, store to local 1
    const char* source = ": PROCESS L! 0 L@ 0 1 + L! 1 ; PROCESS";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "PROCESS") == 0);

    // Check word bytecode contains LSET 0, LGET 0, LIT 1, ADD, LSET 1, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == 0x7A);  // LSET
    CHECK(code[1] == 0x00);  // index 0
    CHECK(code[2] == 0x79);  // LGET
    CHECK(code[3] == 0x00);  // index 0

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
