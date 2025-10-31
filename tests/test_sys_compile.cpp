#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("SYS instruction compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("SYS with decimal ID")
  {
    v4front_err err = v4front_compile("SYS 1", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = SYS (0x60), [1] = ID (0x01), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x01);  // SYS ID = 1
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with hexadecimal ID")
  {
    v4front_err err = v4front_compile("SYS 0x10", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x10);  // SYS ID = 0x10
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with maximum valid ID (255)")
  {
    v4front_err err = v4front_compile("SYS 255", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0xFF);  // SYS ID = 255
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS with minimum valid ID (0)")
  {
    v4front_err err = v4front_compile("SYS 0", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x00);  // SYS ID = 0
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("SYS in expression: 13 1 SYS 0x01")
  {
    // Example: GPIO write - pin=13, value=1, SYS GPIO_WRITE
    v4front_err err = v4front_compile("13 1 SYS 0x01", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected bytecode:
    // [0-4]  = LIT 13 (0x00 0x0D 0x00 0x00 0x00)
    // [5-9]  = LIT 1  (0x00 0x01 0x00 0x00 0x00)
    // [10]   = SYS (0x60)
    // [11]   = ID (0x01)
    // [12]   = RET (0x51)
    REQUIRE(buf.size >= 13);

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

    // Check SYS 0x01
    CHECK(buf.data[10] == 0x60);  // SYS opcode
    CHECK(buf.data[11] == 0x01);  // SYS ID

    // Check RET
    CHECK(buf.data[12] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Multiple SYS calls")
  {
    v4front_err err = v4front_compile("SYS 1 SYS 2 SYS 3", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: SYS 1, SYS 2, SYS 3, RET
    REQUIRE(buf.size >= 7);
    CHECK(buf.data[0] == 0x60);  // SYS
    CHECK(buf.data[1] == 0x01);  // ID 1
    CHECK(buf.data[2] == 0x60);  // SYS
    CHECK(buf.data[3] == 0x02);  // ID 2
    CHECK(buf.data[4] == 0x60);  // SYS
    CHECK(buf.data[5] == 0x03);  // ID 3
    CHECK(buf.data[6] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("SYS case insensitive")
  {
    v4front_err err = v4front_compile("sys 42", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 42);    // SYS ID = 42

    v4front_free(&buf);
  }

  SUBCASE("Error: SYS without ID")
  {
    v4front_err err = v4front_compile("SYS", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -31);  // MissingSysId
  }

  SUBCASE("Error: SYS with invalid ID (256)")
  {
    v4front_err err = v4front_compile("SYS 256", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -32);  // InvalidSysId
  }

  SUBCASE("Error: SYS with negative ID")
  {
    v4front_err err = v4front_compile("SYS -1", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -32);  // InvalidSysId
  }

  SUBCASE("Error: SYS with non-numeric ID")
  {
    v4front_err err = v4front_compile("SYS FOO", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -32);  // InvalidSysId
  }

  SUBCASE("Error: SYS with ID too large (1000)")
  {
    v4front_err err = v4front_compile("SYS 1000", &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -32);  // InvalidSysId
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
    const char* source = ": EMIT SYS 1 ; EMIT";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: SYS 1, RET
    // Main code should contain: CALL 0, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "EMIT") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x60);  // SYS
    CHECK(buf.words[0].code[1] == 0x01);  // ID 1
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("EMIT and KEY compilation")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("EMIT compiles to SYS 0x30")
  {
    v4front_err err = v4front_compile("EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = SYS (0x60), [1] = ID (0x30), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x30);  // SYS_EMIT = 0x30
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("KEY compiles to SYS 0x31")
  {
    v4front_err err = v4front_compile("KEY", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Bytecode verification
    // [0] = SYS (0x60), [1] = ID (0x31), [2] = RET (0x51)
    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x31);  // SYS_KEY = 0x31
    CHECK(buf.data[2] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("EMIT with character literal: 65 EMIT")
  {
    v4front_err err = v4front_compile("65 EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected bytecode:
    // [0-4]  = LIT 65 (0x00 0x41 0x00 0x00 0x00)
    // [5]    = SYS (0x60)
    // [6]    = ID (0x30)
    // [7]    = RET (0x51)
    REQUIRE(buf.size >= 8);

    // Check LIT 65
    CHECK(buf.data[0] == 0x00);  // LIT opcode
    CHECK(buf.data[1] == 0x41);  // 65 = 'A' (little-endian)
    CHECK(buf.data[2] == 0x00);
    CHECK(buf.data[3] == 0x00);
    CHECK(buf.data[4] == 0x00);

    // Check EMIT (SYS 0x30)
    CHECK(buf.data[5] == 0x60);  // SYS opcode
    CHECK(buf.data[6] == 0x30);  // SYS_EMIT

    // Check RET
    CHECK(buf.data[7] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("KEY followed by EMIT: KEY EMIT")
  {
    v4front_err err = v4front_compile("KEY EMIT", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // Expected: KEY (SYS 0x31), EMIT (SYS 0x30), RET
    REQUIRE(buf.size >= 5);
    CHECK(buf.data[0] == 0x60);  // SYS
    CHECK(buf.data[1] == 0x31);  // SYS_KEY
    CHECK(buf.data[2] == 0x60);  // SYS
    CHECK(buf.data[3] == 0x30);  // SYS_EMIT
    CHECK(buf.data[4] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("EMIT case insensitive")
  {
    v4front_err err = v4front_compile("emit", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x30);  // SYS_EMIT

    v4front_free(&buf);
  }

  SUBCASE("KEY case insensitive")
  {
    v4front_err err = v4front_compile("key", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    REQUIRE(buf.size >= 3);
    CHECK(buf.data[0] == 0x60);  // SYS opcode
    CHECK(buf.data[1] == 0x31);  // SYS_KEY

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

    // The word should contain: SYS 0x30, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "PUTC") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x60);  // SYS
    CHECK(buf.words[0].code[1] == 0x30);  // SYS_EMIT
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  SUBCASE("Define word with KEY")
  {
    const char* source = ": GETC KEY ; GETC";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == 0);

    // The word should contain: SYS 0x31, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "GETC") == 0);

    // Check word bytecode
    REQUIRE(buf.words[0].code_len >= 3);
    CHECK(buf.words[0].code[0] == 0x60);  // SYS
    CHECK(buf.words[0].code[1] == 0x31);  // SYS_KEY
    CHECK(buf.words[0].code[2] == 0x51);  // RET

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
