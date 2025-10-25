#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace v4front
{

/**
 * @brief Immediate value kind for each opcode.
 */
enum class ImmKind : uint8_t
{
  None,   ///< No immediate operand
  I8,     ///< 8-bit immediate value
  I16,    ///< 16-bit immediate value
  I32,    ///< 32-bit immediate value
  Rel16,  ///< 16-bit relative offset (for JMP/JZ/JNZ)
  Idx16,  ///< 16-bit word index (for CALL)
};

/**
 * @brief Metadata for each opcode, derived from opcodes.def.
 */
struct OpInfo
{
  const char* name;  ///< Mnemonic string (e.g., "LIT", "ADD", "JMP")
  uint8_t opcode;    ///< Opcode value (0x00 - 0xFF)
  ImmKind imm;       ///< Immediate type
};

/**
 * @brief Disassemble a single instruction at given PC.
 *
 * @param code  Pointer to bytecode buffer.
 * @param len   Total buffer length in bytes.
 * @param pc    Current byte offset.
 * @param out   Output string (human-readable disassembly line, without newline).
 * @return Number of bytes consumed. Returns >=1 to advance, 0 if nothing consumed.
 *
 * Example output: `"0040: JMP +6 ; -> 0048"`
 */
size_t disasm_one(const uint8_t* code, size_t len, size_t pc, std::string& out);

/**
 * @brief Disassemble entire buffer and return vector of lines.
 *
 * @param code Pointer to bytecode buffer.
 * @param len  Length in bytes.
 * @return Vector of disassembled lines.
 */
std::vector<std::string> disasm_all(const uint8_t* code, size_t len);

/**
 * @brief Print disassembly to a FILE stream.
 *
 * @param code Pointer to bytecode buffer.
 * @param len  Length in bytes.
 * @param fp   Output file handle (stdout, file, etc.)
 */
void disasm_print(const uint8_t* code, size_t len, std::FILE* fp);

}  // namespace v4front
