#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("SYS instruction compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("SYS with decimal ID (Forth-style)")
  {
    v4front_err err = v4front_compile("1 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0-4] = LIT 1 (0x00 0x01 0x00 0x00 0x00)
    // [5]   = SYS (0x60)
    // [6]   = RET (0x51)
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x01);  // Value = 1 (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with hexadecimal ID")
  {
    v4front_err err = v4front_compile("0x10 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 0x10, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x10);  // Value = 0x10 (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with maximum valid ID (255)")
  {
    v4front_err err = v4front_compile("255 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 255, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0xFF);  // Value = 255 (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with minimum valid ID (0)")
  {
    v4front_err err = v4front_compile("0 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 0, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x00);  // Value = 0 (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS in expression: 13 1 0x01 SYS")
  {
    // Example: GPIO write - pin=13, value=1, SYS GPIO_WRITE
    v4front_err err = v4front_compile("13 1 0x01 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected bytecode:
    // [0-4]  = LIT 13 (0x00 0x0D 0x00 0x00 0x00)
    // [5-9]  = LIT 1  (0x00 0x01 0x00 0x00 0x00)
    // [10-14] = LIT 0x01 (0x00 0x01 0x00 0x00 0x00)
    // [15]   = SYS (0x60)
    // [16]   = RET (0x51)
    REQUIRE(buf.size >= 17);

    // Check LIT 13
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x0D);  // 13 (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);

    // Check LIT 1
    CHECK(buf.data[5] == 0x00);  // LIT opcode
    CHECK(buf.data[6] == 0x01);  // 1 (little-endian)
    CHECK(buf.data[7] == 0x00);
    CHECK(buf.data[8] == 0x00);
    CHECK(buf.data[9] == 0x00);

    // Check LIT 0x01 (SYS ID)
    CHECK(buf.data[10] == 0x00);  // LIT opcode
    CHECK(buf.data[11] == 0x01);  // SYS ID = 0x01
    CHECK(buf.data[12] == 0x00);
    CHECK(buf.data[13] == 0x00);
    CHECK(buf.data[14] == 0x00);

    // Check SYS
    CHECK(buf.data[15] == 0x60);  // SYS opcode

    // Check RET
    CHECK(buf.data[16] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple SYS calls")
  {
    v4front_err err = v4front_compile("1 SYS 2 SYS 3 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LIT 1, SYS, LIT 2, SYS, LIT 3, SYS, RET
    REQUIRE(buf.size >= 19);
    // LIT 1
    CHECK(buf.data[0] == 0x00);  // LIT
    CHECK(buf.data[1] == 0x01);  // ID 1
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS
    // LIT 2
    CHECK(buf.data[6] == 0x00);  // LIT
    CHECK(buf.data[7] == 0x02);  // ID 2
    CHECK(buf.data[8] == 0x00);
    CHECK(buf.data[9] == 0x00);
    CHECK(buf.data[10] == 0x00);
    CHECK(buf.data[11] == 0x60);  // SYS
    // LIT 3
    CHECK(buf.data[12] == 0x00);  // LIT
    CHECK(buf.data[13] == 0x03);  // ID 3
    CHECK(buf.data[14] == 0x00);
    CHECK(buf.data[15] == 0x00);
    CHECK(buf.data[16] == 0x00);
    CHECK(buf.data[17] == 0x60);  // SYS
    CHECK(buf.data[18] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("SYS case insensitive")
  {
    v4front_err err = v4front_compile("42 sys", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 42, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 42);    // SYS ID = 42
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS without argument (will underflow at runtime)")
  {
    // SYS is now a NoImm primitive, so this compiles successfully
    // but will cause stack underflow at runtime
    v4front_err err = v4front_compile("SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode: [SYS] [RET]
    REQUIRE(buf.size >= 2);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with 256 (valid for 16-bit)")
  {
    // 256 is valid for 16-bit SYS ID
    v4front_err err = v4front_compile("256 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 256, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x00);  // Value = 256 = 0x0100 (little-endian)
    CHECK(buf.data[2] == 0x01);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with negative ID (runtime error)")
  {
    // Negative numbers compile but will be rejected at runtime
    v4front_err err = v4front_compile("-1 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT -1, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0xFF);  // Value = -1 = 0xFFFFFFFF (little-endian)
    CHECK(buf.data[2] == 0xFF);
    CHECK(buf.data[3] == 0xFF);
    CHECK(buf.data[4] == 0xFF);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Error: SYS with non-numeric argument")
  {
    // FOO is an unknown word, so this should fail
    v4front_err err = v4front_compile("FOO SYS", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -1);  // UnknownToken
  }

  SUBCASE("SYS with large ID (65535 is valid)")
  {
    // 65535 (0xFFFF) is the maximum valid 16-bit unsigned value
    v4front_err err = v4front_compile("65535 SYS", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 65535, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0xFF);  // Value = 65535 = 0x0000FFFF (little-endian)
    CHECK(buf.data[2] == 0xFF);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }
}

TEST_CASE("SYS instruction with context")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("SYS in word definition")
  {
    const char* source = ": EMIT 1 SYS ; EMIT";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LIT 1, SYS, RET
    // Main code should contain: CALL 0, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "EMIT") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 7);
    CHECK(buf.words[0].code[0] == 0x00);  // LIT
    CHECK(buf.words[0].code[1] == 0x01);  // ID 1
    CHECK(buf.words[0].code[2] == 0x00);
    CHECK(buf.words[0].code[3] == 0x00);
    CHECK(buf.words[0].code[4] == 0x00);
    CHECK(buf.words[0].code[5] == 0x60);  // SYS
    CHECK(buf.words[0].code[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("EMIT and KEY compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("EMIT compiles to LIT 0x30 + SYS")
  {
    v4front_err err = v4front_compile("EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0-4] = LIT 0x30, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x30);  // SYS_EMIT = 0x30
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("KEY compiles to LIT 0x31 + SYS")
  {
    v4front_err err = v4front_compile("KEY", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0-4] = LIT 0x31, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x31);  // SYS_KEY = 0x31
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("EMIT with character literal: 65 EMIT")
  {
    v4front_err err = v4front_compile("65 EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected bytecode:
    // [0-4]  = LIT 65 (0x00 0x41 0x00 0x00 0x00)
    // [5-9]  = LIT 0x30 (0x00 0x30 0x00 0x00 0x00)
    // [10]   = SYS (0x60)
    // [11]   = RET (0x51)
    REQUIRE(buf.size >= 12);

    // Check LIT 65
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x41);  // 65 = 'A' (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);

    // Check EMIT (LIT 0x30 + SYS)
    CHECK(buf.data[5] == 0x00);  // LIT opcode
    CHECK(buf.data[6] == 0x30);  // SYS_EMIT
    CHECK(buf.data[7] == 0x00);
    CHECK(buf.data[8] == 0x00);
    CHECK(buf.data[9] == 0x00);
    CHECK(buf.data[10] == 0x60);  // SYS opcode

    // Check RET
    CHECK(buf.data[11] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("KEY followed by EMIT: KEY EMIT")
  {
    v4front_err err = v4front_compile("KEY EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: LIT 0x31, SYS, LIT 0x30, SYS, RET
    REQUIRE(buf.size >= 13);
    // KEY
    CHECK(buf.data[0] == 0x00);  // LIT
    CHECK(buf.data[1] == 0x31);  // SYS_KEY
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS
    // EMIT
    CHECK(buf.data[6] == 0x00);  // LIT
    CHECK(buf.data[7] == 0x30);  // SYS_EMIT
    CHECK(buf.data[8] == 0x00);
    CHECK(buf.data[9] == 0x00);
    CHECK(buf.data[10] == 0x00);
    CHECK(buf.data[11] == 0x60);  // SYS
    CHECK(buf.data[12] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("EMIT case insensitive")
  {
    v4front_err err = v4front_compile("emit", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 0x30, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x30);  // SYS_EMIT
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("KEY case insensitive")
  {
    v4front_err err = v4front_compile("key", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // [0-4] = LIT 0x31, [5] = SYS, [6] = RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x31);  // SYS_KEY
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x51);  // RET opcode

    v4front_free(&buf);
  }
}

TEST_CASE("EMIT and KEY in word definitions")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Define word with EMIT")
  {
    const char* source = ": PUTC EMIT ; 72 PUTC";  // 'H'
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LIT 0x30, SYS, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "PUTC") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 7);
    CHECK(buf.words[0].code[0] == 0x00);  // LIT
    CHECK(buf.words[0].code[1] == 0x30);  // SYS_EMIT
    CHECK(buf.words[0].code[2] == 0x00);
    CHECK(buf.words[0].code[3] == 0x00);
    CHECK(buf.words[0].code[4] == 0x00);
    CHECK(buf.words[0].code[5] == 0x60);  // SYS
    CHECK(buf.words[0].code[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("Define word with KEY")
  {
    const char* source = ": GETC KEY ; GETC";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: LIT 0x31, SYS, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "GETC") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 7);
    CHECK(buf.words[0].code[0] == 0x00);  // LIT
    CHECK(buf.words[0].code[1] == 0x31);  // SYS_KEY
    CHECK(buf.words[0].code[2] == 0x00);
    CHECK(buf.words[0].code[3] == 0x00);
    CHECK(buf.words[0].code[4] == 0x00);
    CHECK(buf.words[0].code[5] == 0x60);  // SYS
    CHECK(buf.words[0].code[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
