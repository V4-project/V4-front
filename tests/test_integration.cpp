#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "v4/vm_api.h"
#include "v4front/compile.h"
#include "v4front/errors.hpp"
#include "vendor/doctest/doctest.h"

using namespace v4front;

// Note: vm_ds_depth_public and vm_ds_peek_public are not yet implemented in V4,
// so these tests only verify that compilation and execution complete without errors.
// Once V4 implements stack inspection APIs, we can add result verification.

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

    vm_destroy(vm);
    v4front_free(&buf);
  }
}
