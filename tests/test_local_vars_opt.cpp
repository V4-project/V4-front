#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("L@0 instruction compilation (LGET0)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@0 basic")
  {
    v4front_err err = v4front_compile("L@0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LGET0 (0x7C), [1] = RET (0x51)
    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7C);  // LGET0 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L@0 case insensitive")
  {
    v4front_err err = v4front_compile("l@0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7C);  // LGET0 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L@0 calls")
  {
    v4front_err err = v4front_compile("L@0 L@0 L@0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LGET0, LGET0, RET
    REQUIRE(buf.size >= 4);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x7C);  // LGET0
    CHECK(buf.data[2] == 0x7C);  // LGET0
    CHECK(buf.data[3] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("L@1 instruction compilation (LGET1)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@1 basic")
  {
    v4front_err err = v4front_compile("L@1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LGET1 (0x7D), [1] = RET (0x51)
    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7D);  // LGET1 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L@1 case insensitive")
  {
    v4front_err err = v4front_compile("l@1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7D);  // LGET1 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L@1 calls")
  {
    v4front_err err = v4front_compile("L@1 L@1 L@1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET1, LGET1, LGET1, RET
    REQUIRE(buf.size >= 4);
    CHECK(buf.data[0] == 0x7D);  // LGET1
    CHECK(buf.data[1] == 0x7D);  // LGET1
    CHECK(buf.data[2] == 0x7D);  // LGET1
    CHECK(buf.data[3] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("L!0 instruction compilation (LSET0)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L!0 basic")
  {
    v4front_err err = v4front_compile("L!0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LSET0 (0x7E), [1] = RET (0x51)
    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7E);  // LSET0 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L!0 case insensitive")
  {
    v4front_err err = v4front_compile("l!0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7E);  // LSET0 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L!0 calls")
  {
    v4front_err err = v4front_compile("L!0 L!0 L!0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LSET0, LSET0, LSET0, RET
    REQUIRE(buf.size >= 4);
    CHECK(buf.data[0] == 0x7E);  // LSET0
    CHECK(buf.data[1] == 0x7E);  // LSET0
    CHECK(buf.data[2] == 0x7E);  // LSET0
    CHECK(buf.data[3] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("L!1 instruction compilation (LSET1)")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L!1 basic")
  {
    v4front_err err = v4front_compile("L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = LSET1 (0x7F), [1] = RET (0x51)
    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7F);  // LSET1 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("L!1 case insensitive")
  {
    v4front_err err = v4front_compile("l!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x7F);  // LSET1 opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple L!1 calls")
  {
    v4front_err err = v4front_compile("L!1 L!1 L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LSET1, LSET1, LSET1, RET
    REQUIRE(buf.size >= 4);
    CHECK(buf.data[0] == 0x7F);  // LSET1
    CHECK(buf.data[1] == 0x7F);  // LSET1
    CHECK(buf.data[2] == 0x7F);  // LSET1
    CHECK(buf.data[3] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("Mixed optimized local variable operations")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@0 and L@1 together")
  {
    v4front_err err = v4front_compile("L@0 L@1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LGET1, RET
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x7D);  // LGET1
    CHECK(buf.data[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L!0 and L!1 together")
  {
    v4front_err err = v4front_compile("L!0 L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LSET0, LSET1, RET
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7E);  // LSET0
    CHECK(buf.data[1] == 0x7F);  // LSET1
    CHECK(buf.data[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L@0 L!1 combination")
  {
    v4front_err err = v4front_compile("L@0 L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LSET1, RET
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x7F);  // LSET1
    CHECK(buf.data[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("All four operations")
  {
    v4front_err err = v4front_compile("L@0 L@1 L!0 L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LGET1, LSET0, LSET1, RET
    REQUIRE(buf.size >= 5);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x7D);  // LGET1
    CHECK(buf.data[2] == 0x7E);  // LSET0
    CHECK(buf.data[3] == 0x7F);  // LSET1
    CHECK(buf.data[4] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("Optimized vs general local variable instructions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@0 is more compact than L@ 0")
  {
    // L@0 version
    v4front_err err1 = v4front_compile("L@0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err1 == 0);
    uint32_t opt_size = buf.size;
    v4front_free(&buf);

    // L@ 0 version
    v4front_err err2 = v4front_compile("L@ 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err2 == 0);
    uint32_t gen_size = buf.size;
    v4front_free(&buf);

    // Optimized should be 1 byte smaller (no immediate)
    CHECK(opt_size == gen_size - 1);
  }

  SUBCASE("L!1 is more compact than L! 1")
  {
    // L!1 version
    v4front_err err1 = v4front_compile("L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err1 == 0);
    uint32_t opt_size = buf.size;
    v4front_free(&buf);

    // L! 1 version
    v4front_err err2 = v4front_compile("L! 1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err2 == 0);
    uint32_t gen_size = buf.size;
    v4front_free(&buf);

    // Optimized should be 1 byte smaller (no immediate)
    CHECK(opt_size == gen_size - 1);
  }

  SUBCASE("Mix of optimized and general")
  {
    v4front_err err = v4front_compile("L@0 L@ 2 L!1 L! 3", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LGET 2, LSET1, LSET 3, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x79);  // LGET
    CHECK(buf.data[2] == 0x02);  // index 2
    CHECK(buf.data[3] == 0x7F);  // LSET1
    CHECK(buf.data[4] == 0x7A);  // LSET
    CHECK(buf.data[5] == 0x03);  // index 3
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }
}

TEST_CASE("Optimized local variable operations in word definitions")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@0 in word definition")
  {
    const char* source = ": GET0 L@0 ; GET0";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LGET0, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "GET0") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 2);
    CHECK(buf.words[0].code[0] == 0x7C);  // LGET0
    CHECK(buf.words[0].code[1] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("L!1 in word definition")
  {
    const char* source = ": SET1 L!1 ; SET1";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LSET1, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "SET1") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 2);
    CHECK(buf.words[0].code[0] == 0x7F);  // LSET1
    CHECK(buf.words[0].code[1] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("Complex word with optimized and general local variables")
  {
    // Use optimized L@0/L!1 and general L@ 2/L! 3
    const char* source = ": PROCESS L@0 L@ 2 + L!1 L! 3 ; PROCESS";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "PROCESS") == 0);

    // Check word bytecode contains LGET0, LGET 2, ADD, LSET1, LSET 3, RET
    const uint8_t* code = buf.words[0].code;
    CHECK(code[0] == 0x7C);  // LGET0
    CHECK(code[1] == 0x79);  // LGET
    CHECK(code[2] == 0x02);  // index 2
    CHECK(code[3] == 0x10);  // ADD

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("Optimized local variables with arithmetic")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L@0 L@1 +")
  {
    v4front_err err = v4front_compile("L@0 L@1 +", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LGET0, LGET1, ADD, RET
    REQUIRE(buf.size >= 4);
    CHECK(buf.data[0] == 0x7C);  // LGET0
    CHECK(buf.data[1] == 0x7D);  // LGET1
    CHECK(buf.data[2] == 0x10);  // ADD
    CHECK(buf.data[3] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("42 L!0 L@0 1 + L!1")
  {
    v4front_err err = v4front_compile("42 L!0 L@0 1 + L!1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LIT 42, LSET0, LGET0, LIT 1, ADD, LSET1, RET
    REQUIRE(buf.size >= 12);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x2A);  // 42
    CHECK(buf.data[5] == 0x7E);  // LSET0
    CHECK(buf.data[6] == 0x7C);  // LGET0
    CHECK(buf.data[7] == 0x00);  // LIT
    CHECK(buf.data[8] == 0x01);  // 1
    // ADD and LSET1 follow

    v4front_free(&buf);
  }
}
