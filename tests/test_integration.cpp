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

// NOTE: Local variable integration tests are skipped because V4 VM's current
// CALL implementation doesn't automatically set the frame pointer (vm->fp).
// Local variables require manual frame pointer setup, which is not done by
// the standard CALL opcode. This will be addressed in future V4 VM updates.
//
// For reference, V4 VM tests manually set fp like this:
//   vm.fp = vm.RS;
//   vm.RS[0] = value0;
//   vm.RS[1] = value1;
//   vm.rp = vm.RS + 2;
//
// See V4 test_vm.cpp lines 1223-1264 for manual fp setup examples.

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
