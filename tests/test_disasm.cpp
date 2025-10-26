/**
 * @file test_disasm.cpp
 * @brief Doctest-based unit tests for the V4 bytecode disassembler.
 */
#define DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN

#include <cstdint>
#include <string>
#include <vector>

#include "v4front/disasm.hpp"
#include "vendor/doctest/doctest.h"

/// Bring symbols into scope
using v4front::disasm_all;
using v4front::disasm_one;
using v4front::disasm_print;

/**
 * @brief Import opcode constants from opcodes.def as enum entries.
 *
 * This creates names like OP_LIT, OP_ADD, OP_JMP, etc. whose values
 * match the actual opcode bytes defined in the project.
 */
enum : uint16_t
{
#define OP(NAME, CODE, IMM) OP_##NAME = CODE,
#include "v4/opcodes.def"
#undef OP
};

/**
 * @brief Append a 16-bit little-endian immediate to a buffer.
 */
static inline void append_i16(std::vector<uint8_t>& bc, int16_t v)
{
  bc.push_back(static_cast<uint8_t>(v & 0xFF));
  bc.push_back(static_cast<uint8_t>((static_cast<uint16_t>(v) >> 8) & 0xFF));
}

/**
 * @brief Append a 32-bit little-endian immediate to a buffer.
 */
static inline void append_i32(std::vector<uint8_t>& bc, int32_t v)
{
  bc.push_back(static_cast<uint8_t>(v & 0xFF));
  bc.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
  bc.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
  bc.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}

/**
 * @brief Append a signed 8-bit immediate to a buffer.
 */
static inline void append_i8(std::vector<uint8_t>& bc, int8_t v)
{
  bc.push_back(static_cast<uint8_t>(v));
}

/**
 * @brief Helper: check that a line contains all required substrings.
 */
static inline void expect_contains_all(const std::string& line,
                                       std::initializer_list<const char*> subs)
{
  for (const char* s : subs)
  {
    INFO("line: " << line);
    CHECK(line.find(s) != std::string::npos);
  }
}

/**
 * @test Disassemble a simple sequence with IMM32 and NO_IMM.
 * Bytecode: LIT 1234; DUP; ADD
 */
TEST_CASE("disasm: LIT(I32) + DUP + ADD")
{
  std::vector<uint8_t> bc;
  // LIT 1234
  bc.push_back(static_cast<uint8_t>(OP_LIT));
  append_i32(bc, 1234);
  // DUP
  bc.push_back(static_cast<uint8_t>(OP_DUP));
  // ADD
  bc.push_back(static_cast<uint8_t>(OP_ADD));

  auto lines = disasm_all(bc.data(), bc.size());
  REQUIRE(lines.size() == 3);

  expect_contains_all(lines[0], {"LIT", "1234"});
  expect_contains_all(lines[1], {"DUP"});
  expect_contains_all(lines[2], {"ADD"});
}

/**
 * @test Disassemble relative branches (REL16).
 * Bytecode: JMP +3; JZ -2; JNZ +0
 *
 * Note: We only assert substrings and the presence of the arrow form " ; -> ".
 */
TEST_CASE("disasm: REL16 branches (JMP/JZ/JNZ)")
{
  std::vector<uint8_t> bc;
  // 0000: JMP +3
  bc.push_back(static_cast<uint8_t>(OP_JMP));
  append_i16(bc, +3);
  // 0003: JZ -2
  bc.push_back(static_cast<uint8_t>(OP_JZ));
  append_i16(bc, -2);
  // 0006: JNZ +0
  bc.push_back(static_cast<uint8_t>(OP_JNZ));
  append_i16(bc, 0);

  auto lines = disasm_all(bc.data(), bc.size());
  REQUIRE(lines.size() == 3);

  expect_contains_all(lines[0], {"JMP", "+3", " ; -> "});
  expect_contains_all(lines[1], {"JZ", "-2", " ; -> "});
  expect_contains_all(lines[2], {"JNZ", "+0", " ; -> "});
}

/**
 * @test Disassemble CALL with IDX16 and SYS with IMM8.
 * Bytecode: CALL @321; SYS 7
 */
TEST_CASE("disasm: CALL(Idx16) and SYS(I8)")
{
  std::vector<uint8_t> bc;
  // CALL @321
  bc.push_back(static_cast<uint8_t>(OP_CALL));
  append_i16(bc, 321);
  // SYS 7
  bc.push_back(static_cast<uint8_t>(OP_SYS));
  append_i8(bc, 7);

  auto lines = disasm_all(bc.data(), bc.size());
  REQUIRE(lines.size() == 2);

  expect_contains_all(lines[0], {"CALL", "@321"});
  expect_contains_all(lines[1], {"SYS", "7"});
}

/**
 * @test Truncation handling: immediate operands cut off by end of buffer.
 * Expect markers like "<trunc-i32>" or "<trunc-rel16>".
 */
TEST_CASE("disasm: truncated immediates")
{
  // Case 1: LIT imm32 with only 3 imm bytes available
  {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(OP_LIT));
    // push only 3 bytes, missing the high byte
    bc.push_back(0x2A);
    bc.push_back(0x00);
    bc.push_back(0x00);

    auto lines = disasm_all(bc.data(), bc.size());
    REQUIRE(lines.size() == 1);
    expect_contains_all(lines[0], {"LIT", "<trunc-i32>"});
  }

  // Case 2: JMP rel16 with 0 imm bytes available
  {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(OP_JMP));
    auto lines = disasm_all(bc.data(), bc.size());
    REQUIRE(lines.size() == 1);
    expect_contains_all(lines[0], {"JMP", "<trunc-rel16>"});
  }

  // Case 3: CALL idx16 with only 1 imm byte available
  {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(OP_CALL));
    bc.push_back(0x01);  // missing high byte
    auto lines = disasm_all(bc.data(), bc.size());
    REQUIRE(lines.size() == 1);
    expect_contains_all(lines[0], {"CALL", "<trunc-idx16>"});
  }

  // Case 4: SYS imm8 with 0 imm bytes available
  {
    std::vector<uint8_t> bc;
    bc.push_back(static_cast<uint8_t>(OP_SYS));
    auto lines = disasm_all(bc.data(), bc.size());
    REQUIRE(lines.size() == 1);
    expect_contains_all(lines[0], {"SYS", "<trunc-i8>"});
  }
}

/**
 * @test Multi-instruction stream: ensure PC advances by exact encoded size.
 * Sequence: LIT 42 (5 bytes) ; ADD (1) ; JMP -1 (3) ; RET (1)
 */
TEST_CASE("disasm: PC advancing across mixed sizes")
{
  std::vector<uint8_t> bc;
  // LIT 42
  bc.push_back(static_cast<uint8_t>(OP_LIT));
  append_i32(bc, 42);
  // ADD
  bc.push_back(static_cast<uint8_t>(OP_ADD));
  // JMP -1
  bc.push_back(static_cast<uint8_t>(OP_JMP));
  append_i16(bc, -1);
  // RET
  bc.push_back(static_cast<uint8_t>(OP_RET));

  auto lines = disasm_all(bc.data(), bc.size());
  REQUIRE(lines.size() == 4);

  // Check each mnemonic shows up in order, with expected adornments.
  expect_contains_all(lines[0], {"LIT", "42"});
  expect_contains_all(lines[1], {"ADD"});
  expect_contains_all(lines[2], {"JMP", "-1", " ; -> "});
  expect_contains_all(lines[3], {"RET"});
}