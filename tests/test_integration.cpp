#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/vm_api.h"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;

TEST_CASE("Integration: Compile and execute simple arithmetic")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("5 3 +")
  {
    v4front_err err = v4front_compile("5 3 +", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == FrontErr::OK);

    // Create VM with minimal RAM
    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    // Register bytecode as word 0
    int word_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(word_idx >= 0);

    // Execute
    struct Word* entry = vm_get_word(vm, word_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);  // Execution should succeed

    // Verify stack: should have one value (5 + 3 = 8)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 1);
    if (depth >= 1)
    {
      v4_i32 result = vm_ds_peek_public(vm, 0);
      CHECK(result == 8);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }

  SUBCASE("10 3 -")
  {
    v4front_err err = v4front_compile("10 3 -", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == FrontErr::OK);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(word_idx >= 0);

    struct Word* entry = vm_get_word(vm, word_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Verify stack: should have one value (10 - 3 = 7)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 1);
    if (depth >= 1)
    {
      v4_i32 result = vm_ds_peek_public(vm, 0);
      CHECK(result == 7);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }
}

TEST_CASE("Integration: Word definitions")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE(": DOUBLE DUP + ; 5 DOUBLE")
  {
    v4front_err err =
        v4front_compile(": DOUBLE DUP + ; 5 DOUBLE", &buf, errmsg, sizeof(errmsg));
    REQUIRE(err == FrontErr::OK);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    // Register user-defined word first
    int double_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(double_idx >= 0);

    // Register main code
    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    // Execute main
    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Verify stack: should have one value (5 * 2 = 10)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 1);
    if (depth >= 1)
    {
      v4_i32 result = vm_ds_peek_public(vm, 0);
      CHECK(result == 10);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }
}

TEST_CASE("Integration: Local variables")
{
  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("L++ and L-- operations")
  {
    // Define a word that uses local variables
    // Push values to return stack, modify them, fetch results
    const char* source =
        ": TEST 10 >R 20 >R L++ 1 L-- 0 L@ 1 L@ 0 R> DROP R> DROP ; TEST";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == FrontErr::OK, "Compilation failed: ", errmsg);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(word_idx >= 0);

    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Stack should have: L@1 (21 after L++), L@0 (9 after L--)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 2);
    if (depth >= 2)
    {
      v4_i32 val1 = vm_ds_peek_public(vm, 0);  // 9
      v4_i32 val2 = vm_ds_peek_public(vm, 1);  // 21
      CHECK(val1 == 9);
      CHECK(val2 == 21);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }

  SUBCASE("L@ and L! operations")
  {
    // Test LGET and LSET operations
    const char* source =
        ": TEST 100 >R 200 >R L@ 1 L@ 0 300 L! 0 400 L! 1 L@ 0 L@ 1 R> DROP R> DROP ; "
        "TEST";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == FrontErr::OK, "Compilation failed: ", errmsg);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(word_idx >= 0);

    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Stack: L@1 (200), L@0 (100), then L!s set to 300/400, then L@0 (300), L@1 (400)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 4);
    if (depth >= 4)
    {
      v4_i32 val1 = vm_ds_peek_public(vm, 0);  // 400
      v4_i32 val2 = vm_ds_peek_public(vm, 1);  // 300
      v4_i32 val3 = vm_ds_peek_public(vm, 2);  // 100
      v4_i32 val4 = vm_ds_peek_public(vm, 3);  // 200
      CHECK(val1 == 400);
      CHECK(val2 == 300);
      CHECK(val3 == 100);
      CHECK(val4 == 200);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }

  SUBCASE("Optimized L@0, L@1, L!0, L!1")
  {
    // Test optimized local variable access opcodes
    const char* source =
        ": TEST 10 >R 20 >R L@0 L@1 30 L!0 40 L!1 L@0 L@1 R> DROP R> DROP ; TEST";
    v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == FrontErr::OK, "Compilation failed: ", errmsg);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(word_idx >= 0);

    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Stack: L@0 (10), L@1 (20), then modified to 30/40, then L@0 (30), L@1 (40)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 4);
    if (depth >= 4)
    {
      v4_i32 val1 = vm_ds_peek_public(vm, 0);  // 40
      v4_i32 val2 = vm_ds_peek_public(vm, 1);  // 30
      v4_i32 val3 = vm_ds_peek_public(vm, 2);  // 20
      v4_i32 val4 = vm_ds_peek_public(vm, 3);  // 10
      CHECK(val1 == 40);
      CHECK(val2 == 30);
      CHECK(val3 == 20);
      CHECK(val4 == 10);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }
}

TEST_CASE("Integration: RECURSE")
{
  V4FrontContext* ctx = v4front_context_create();
  REQUIRE(ctx != nullptr);

  V4FrontBuf buf;
  char errmsg[256];

  SUBCASE("Simple non-recursive word")
  {
    // Simple word without recursion to verify basic VM integration
    const char* source = ": DOUBLE DUP + ; 5 DOUBLE";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == FrontErr::OK, "Compilation failed: ", errmsg);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[1024] = {0};
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(word_idx >= 0);

    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Stack should have 10 (5 * 2)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 1);
    if (depth >= 1)
    {
      v4_i32 result = vm_ds_peek_public(vm, 0);
      CHECK(result == 10);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }

  SUBCASE("Factorial of small number")
  {
    // FACTORIAL: ( n -- n! ) computes factorial recursively
    const char* source =
        ": FACTORIAL DUP 2 < IF DROP 1 ELSE DUP 1 - RECURSE * THEN ; 3 "
        "FACTORIAL";
    v4front_err err =
        v4front_compile_with_context(ctx, source, &buf, errmsg, sizeof(errmsg));
    REQUIRE_MESSAGE(err == FrontErr::OK, "Compilation failed: ", errmsg);
    REQUIRE(buf.word_count == 1);

    uint8_t ram[4096] = {0};  // Increased stack size
    VmConfig cfg = {ram, sizeof(ram), nullptr, 0};
    struct Vm* vm = vm_create(&cfg);
    REQUIRE(vm != nullptr);

    int word_idx =
        vm_register_word(vm, buf.words[0].name, buf.words[0].code, buf.words[0].code_len);
    REQUIRE(word_idx >= 0);

    int main_idx = vm_register_word(vm, "main", buf.data, static_cast<int>(buf.size));
    REQUIRE(main_idx >= 0);

    struct Word* entry = vm_get_word(vm, main_idx);
    REQUIRE(entry != nullptr);
    v4_err exec_err = vm_exec(vm, entry);
    CHECK(exec_err == 0);

    // Stack should have 6 (3! = 6)
    int depth = vm_ds_depth_public(vm);
    CHECK(depth == 1);
    if (depth >= 1)
    {
      v4_i32 result = vm_ds_peek_public(vm, 0);
      CHECK(result == 6);
    }

    vm_destroy(vm);
    v4front_free(&buf);
  }

  v4front_context_destroy(ctx);
}
