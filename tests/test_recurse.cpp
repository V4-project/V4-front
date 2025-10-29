#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

TEST_CASE("RECURSE in word definition")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple RECURSE in word definition")
  {
    // Define a word that uses RECURSE
    const char* source = ": COUNTDOWN DUP IF DUP 1 - RECURSE THEN DROP ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    // The word should contain: DUP, JZ, DUP, LIT 1, SUB, CALL 0 (self), DROP, RET
    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "COUNTDOWN") == 0);

    // Check that word bytecode contains CALL instruction
    const uint8_t* code = buf.words[0].code;
    bool found_call = false;
    for (uint32_t i = 0; i < buf.words[0].code_len - 2; i++)
    {
      if (code[i] == 0x50)
      {  // CALL opcode
        // Check that it's calling word 0 (itself)
        int16_t idx = static_cast<int16_t>(code[i + 1] | (code[i + 2] << 8));
        CHECK(idx == 0);
        found_call = true;
        break;
      }
    }
    CHECK(found_call);

    v4front_free(&buf);
  }

  SUBCASE("RECURSE with multiple words")
  {
    // Define two words, second one uses RECURSE
    const char* source =
        ": HELPER 1 + ; : FACTORIAL DUP 1 > IF DUP 1 - RECURSE * ELSE DROP 1 THEN ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    REQUIRE(buf.word_count == 2);
    CHECK(std::strcmp(buf.words[0].name, "HELPER") == 0);
    CHECK(std::strcmp(buf.words[1].name, "FACTORIAL") == 0);

    // Check that FACTORIAL contains CALL to itself (word index 1)
    const uint8_t* code = buf.words[1].code;
    bool found_call = false;
    for (uint32_t i = 0; i < buf.words[1].code_len - 2; i++)
    {
      if (code[i] == 0x50)
      {  // CALL opcode
        int16_t idx = static_cast<int16_t>(code[i + 1] | (code[i + 2] << 8));
        if (idx == 1)
        {  // Calling itself (word 1)
          found_call = true;
          break;
        }
      }
    }
    CHECK(found_call);

    v4front_free(&buf);
  }

  SUBCASE("Multiple RECURSE calls in one word")
  {
    // Fibonacci-like recursive structure
    const char* source =
        ": FIB DUP 2 < IF ELSE DUP 1 - RECURSE SWAP 2 - RECURSE + THEN ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    REQUIRE(buf.word_count == 1);
    CHECK(std::strcmp(buf.words[0].name, "FIB") == 0);

    // Check that word bytecode contains two CALL instructions to itself
    const uint8_t* code = buf.words[0].code;
    int call_count = 0;
    for (uint32_t i = 0; i < buf.words[0].code_len - 2; i++)
    {
      if (code[i] == 0x50)
      {  // CALL opcode
        int16_t idx = static_cast<int16_t>(code[i + 1] | (code[i + 2] << 8));
        if (idx == 0)
        {  // Calling itself
          call_count++;
        }
      }
    }
    CHECK(call_count == 2);

    v4front_free(&buf);
  }

  SUBCASE("RECURSE case insensitive")
  {
    const char* source = ": LOOP DUP 0 > IF 1 - recurse THEN ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    REQUIRE(buf.word_count == 1);

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}

TEST_CASE("RECURSE error handling")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Error: RECURSE outside word definition")
  {
    const char* source = "RECURSE";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -35);  // RecurseOutsideWord
  }

  SUBCASE("Error: RECURSE in main code (not in word)")
  {
    const char* source = "1 2 + RECURSE";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -35);  // RecurseOutsideWord
  }

  SUBCASE("Error: RECURSE after word definition")
  {
    const char* source = ": FOO 1 ; RECURSE";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    CHECK(err < 0);
    CHECK(err == -35);  // RecurseOutsideWord
  }
}

TEST_CASE("RECURSE bytecode verification")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Verify RECURSE emits correct CALL instruction")
  {
    const char* source = ": TEST RECURSE ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    REQUIRE(buf.word_count == 1);

    // Word bytecode should be: CALL 0, RET
    // CALL is 0x50, followed by 16-bit index (0x00 0x00 for word 0), then RET (0x51)
    REQUIRE(buf.words[0].code_len >= 4);
    CHECK(buf.words[0].code[0] == 0x50);  // CALL opcode
    CHECK(buf.words[0].code[1] == 0x00);  // index low byte (0)
    CHECK(buf.words[0].code[2] == 0x00);  // index high byte (0)
    CHECK(buf.words[0].code[3] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  SUBCASE("Verify second word RECURSE calls correct index")
  {
    const char* source = ": FIRST 1 ; : SECOND RECURSE ;";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

    REQUIRE(buf.word_count == 2);

    // SECOND word should call word index 1 (itself)
    // CALL is 0x50, followed by 16-bit index (0x01 0x00 for word 1)
    const uint8_t* code = buf.words[1].code;
    REQUIRE(buf.words[1].code_len >= 4);
    CHECK(code[0] == 0x50);  // CALL opcode
    CHECK(code[1] == 0x01);  // index low byte (1)
    CHECK(code[2] == 0x00);  // index high byte (0)
    CHECK(code[3] == 0x51);  // RET opcode

    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
