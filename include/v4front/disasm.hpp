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
 * No exceptions thrown. Returns 0 on error instead of throwing.
 * Compatible with -fno-exceptions builds.
 *
 * @param code  Pointer to bytecode buffer.
 * @param len   Total buffer length in bytes.
 * @param pc    Current byte offset.
 * @param out   Output string (human-readable disassembly line, without newline).
 * @return Number of bytes consumed. Returns >=1 to advance, 0 if nothing consumed.
 *
 * Example output: `"0040: JMP +6 ; -> 0048"`
 *
 * Thread-safety: Thread-safe (no shared state).
 * Exception-safety: No exceptions (noexcept-compatible).
 */
size_t disasm_one(const uint8_t* code, size_t len, size_t pc, std::string& out);

/**
 * @brief Disassemble entire buffer and return vector of lines.
 *
 * No exceptions thrown on disassembly errors (stops at invalid instruction).
 * May throw std::bad_alloc on memory allocation failure (from std::vector).
 *
 * @param code Pointer to bytecode buffer.
 * @param len  Length in bytes.
 * @return Vector of disassembled lines.
 *
 * Example:
 * @code
 *   auto lines = disasm_all(code, len);
 *   for (const auto& line : lines) {
 *       std::cout << line << '\n';
 *   }
 * @endcode
 *
 * Thread-safety: Thread-safe (no shared state).
 * Exception-safety: Basic guarantee (may throw std::bad_alloc).
 */
std::vector<std::string> disasm_all(const uint8_t* code, size_t len);

/**
 * @brief Print disassembly to a FILE stream.
 *
 * No exceptions thrown.
 * Compatible with -fno-exceptions builds.
 *
 * @param code Pointer to bytecode buffer.
 * @param len  Length in bytes.
 * @param fp   Output file handle (stdout, file, etc.)
 *
 * Example:
 * @code
 *   // Print to stdout
 *   disasm_print(code, len, stdout);
 *
 *   // Print to file
 *   FILE* fp = fopen("output.txt", "w");
 *   disasm_print(code, len, fp);
 *   fclose(fp);
 * @endcode
 *
 * Thread-safety: Not thread-safe (uses FILE* which is not thread-safe).
 * Exception-safety: No exceptions (noexcept-compatible).
 * NULL-safety: Safe to call with NULL code or fp (no-op).
 */
void disasm_print(const uint8_t* code, size_t len, std::FILE* fp);

}  // namespace v4front
