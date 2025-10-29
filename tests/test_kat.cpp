#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <cstring>

#include "kat_runner.hpp"
#include "v4front/compile.h"
#include "vendor/doctest/doctest.h"

using namespace v4front::kat;

// Helper function to run a single KAT test
static void run_kat_test(const KatTest& test)
{
  INFO("Test: ", test.name);
  INFO("Source: ", test.source);

  V4FrontBuf buf;
  char errmsg[256];

  // Compile source
  v4front_err err = v4front_compile(test.source.c_str(), &buf, errmsg, sizeof(errmsg));
  REQUIRE_MESSAGE(err == 0, "Compilation failed: ", errmsg);

  // Compare bytecode
  REQUIRE_MESSAGE(buf.size == test.expected_bytes.size(),
                  "Bytecode size mismatch: expected ", test.expected_bytes.size(),
                  " bytes, got ", buf.size, " bytes");

  // Compare byte by byte
  for (size_t i = 0; i < buf.size && i < test.expected_bytes.size(); i++)
  {
    INFO("Byte offset: ", i);
    CHECK_MESSAGE(buf.data[i] == test.expected_bytes[i], "Expected 0x", std::hex,
                  static_cast<int>(test.expected_bytes[i]), ", got 0x",
                  static_cast<int>(buf.data[i]));
  }

  v4front_free(&buf);
}

TEST_CASE("KAT: Arithmetic operations")
{
  auto tests = load_kat_file("tests/kat/arithmetic.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load arithmetic.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: Stack operations")
{
  auto tests = load_kat_file("tests/kat/stack.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load stack.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: Control flow")
{
  auto tests = load_kat_file("tests/kat/control.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load control.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: Memory operations")
{
  auto tests = load_kat_file("tests/kat/memory.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load memory.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: System calls")
{
  auto tests = load_kat_file("tests/kat/sys.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load sys.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: Word definitions")
{
  auto tests = load_kat_file("tests/kat/words.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load words.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT: Local variables")
{
  auto tests = load_kat_file("tests/kat/locals.kat");
  REQUIRE_MESSAGE(!tests.empty(), "Failed to load locals.kat");

  for (const auto& test : tests)
  {
    SUBCASE(test.name.c_str())
    {
      run_kat_test(test);
    }
  }
}

TEST_CASE("KAT Parser: Hex byte parsing")
{
  uint8_t byte;

  SUBCASE("Valid hex bytes")
  {
    CHECK(parse_hex_byte("00", &byte));
    CHECK(byte == 0x00);

    CHECK(parse_hex_byte("FF", &byte));
    CHECK(byte == 0xFF);

    CHECK(parse_hex_byte("0A", &byte));
    CHECK(byte == 0x0A);

    CHECK(parse_hex_byte("ff", &byte));  // lowercase
    CHECK(byte == 0xFF);

    CHECK(parse_hex_byte("10", &byte));
    CHECK(byte == 0x10);

    CHECK(parse_hex_byte("A", &byte));  // single digit
    CHECK(byte == 0x0A);
  }

  SUBCASE("Invalid hex bytes")
  {
    CHECK(!parse_hex_byte("GG", &byte));     // invalid hex
    CHECK(!parse_hex_byte("100", &byte));    // too large value
    CHECK(!parse_hex_byte("", &byte));       // empty
    CHECK(!parse_hex_byte(nullptr, &byte));  // null pointer
  }
}

TEST_CASE("KAT Parser: Hex bytes sequence")
{
  SUBCASE("Valid sequences")
  {
    auto bytes = parse_hex_bytes("00 0A 00 00 00");
    REQUIRE(bytes.size() == 5);
    CHECK(bytes[0] == 0x00);
    CHECK(bytes[1] == 0x0A);
    CHECK(bytes[2] == 0x00);
    CHECK(bytes[3] == 0x00);
    CHECK(bytes[4] == 0x00);
  }

  SUBCASE("Mixed spacing")
  {
    auto bytes = parse_hex_bytes("00  0A   00");
    REQUIRE(bytes.size() == 3);
    CHECK(bytes[0] == 0x00);
    CHECK(bytes[1] == 0x0A);
    CHECK(bytes[2] == 0x00);
  }

  SUBCASE("With comments")
  {
    auto bytes = parse_hex_bytes("00 0A # comment");
    REQUIRE(bytes.size() == 2);
    CHECK(bytes[0] == 0x00);
    CHECK(bytes[1] == 0x0A);
  }

  SUBCASE("Empty string")
  {
    auto bytes = parse_hex_bytes("");
    CHECK(bytes.empty());
  }

  SUBCASE("Invalid sequence")
  {
    auto bytes = parse_hex_bytes("00 GG 00");
    CHECK(bytes.empty());  // Should return empty on parse error
  }
}

TEST_CASE("KAT Parser: File loading")
{
  SUBCASE("Nonexistent file")
  {
    auto tests = load_kat_file("nonexistent.kat");
    CHECK(tests.empty());
  }

  SUBCASE("Valid file structure")
  {
    auto tests = load_kat_file("tests/kat/arithmetic.kat");
    REQUIRE(!tests.empty());

    // Check first test
    CHECK(!tests[0].name.empty());
    CHECK(!tests[0].source.empty());
    CHECK(!tests[0].expected_bytes.empty());
  }
}
