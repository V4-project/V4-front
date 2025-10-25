#pragma once
#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t
#include <stdio.h>   // FILE

#ifdef __cplusplus
extern "C"
{
#endif

  /**
   * @brief Disassemble bytecode and print to file stream (C API).
   *
   * Prints human-readable disassembly line-by-line to the given FILE*.
   * Each line includes address, mnemonic, and operands.
   *
   * @param code Pointer to bytecode buffer.
   * @param len  Length of bytecode in bytes.
   * @param fp   Output file stream (e.g., stdout, stderr, or file).
   *
   * Example output:
   * ```
   * 0000: LIT      42
   * 0005: DUP
   * 0006: ADD
   * 0007: RET
   * ```
   *
   * Thread-safety: Not thread-safe (uses fprintf internally).
   * NULL-safety: Safe to call with NULL code or fp (no-op).
   */
  void v4front_disasm_print(const uint8_t* code, size_t len, FILE* fp);

  /**
   * @brief Disassemble a single instruction and write to buffer (C API).
   *
   * Disassembles one instruction at the given program counter (PC) offset.
   * The output string is written to the provided buffer with null termination.
   *
   * @param code     Pointer to bytecode buffer.
   * @param len      Total length of bytecode in bytes.
   * @param pc       Current program counter (byte offset).
   * @param out_buf  Output buffer for disassembly string.
   * @param buf_size Size of output buffer.
   * @return Number of bytes consumed (instruction length), or 0 on error/EOF.
   *
   * Example usage:
   * @code
   * char line[128];
   * size_t pc = 0;
   * while (pc < len) {
   *     size_t consumed = v4front_disasm_one(code, len, pc, line, sizeof(line));
   *     if (consumed == 0) break;
   *     printf("%s\n", line);
   *     pc += consumed;
   * }
   * @endcode
   *
   * Thread-safety: Thread-safe (no shared state).
   * NULL-safety: Returns 0 if code or out_buf is NULL.
   */
  size_t v4front_disasm_one(const uint8_t* code, size_t len, size_t pc, char* out_buf,
                            size_t buf_size);

  /**
   * @brief Get the total number of instructions in bytecode (C API).
   *
   * Scans the entire bytecode buffer and counts the number of valid instructions.
   *
   * @param code Pointer to bytecode buffer.
   * @param len  Length of bytecode in bytes.
   * @return Number of instructions, or 0 if the buffer is empty or invalid.
   *
   * Example:
   * @code
   * size_t count = v4front_disasm_count(code, len);
   * printf("Total instructions: %zu\n", count);
   * @endcode
   *
   * Thread-safety: Thread-safe (no shared state).
   * NULL-safety: Returns 0 if code is NULL.
   */
  size_t v4front_disasm_count(const uint8_t* code, size_t len);

#ifdef __cplusplus
}  // extern "C"
#endif
