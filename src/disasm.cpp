#include "v4front/disasm.hpp"

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace v4front
{

// Macros to map IMM token from opcodes.def into ImmKind enum
#define NO_IMM ImmKind::None
#define IMM8 ImmKind::I8
#define IMM16 ImmKind::I16
#define IMM32 ImmKind::I32
#define REL16 ImmKind::Rel16
#define IDX16 ImmKind::Idx16

/**
 * @brief Lookup OpInfo for given opcode.
 *
 * Generated from opcodes.def at compile-time.
 */
static inline OpInfo get_info(uint8_t opcode)
{
  switch (opcode)
  {
#define OP(NAME, CODE, IMM) \
  case CODE:                \
    return OpInfo{#NAME, CODE, IMM};
#include "v4/opcodes.def"
#undef OP
    default:
      return OpInfo{"???", opcode, ImmKind::None};
  }
}

// Helpers to read little-endian values safely
static inline bool read_u8(const uint8_t* p, size_t len, size_t off, uint8_t& v)
{
  if (off + 1 > len)
    return false;
  v = p[off];
  return true;
}

static inline bool read_i8(const uint8_t* p, size_t len, size_t off, int8_t& v)
{
  uint8_t t;
  if (!read_u8(p, len, off, t))
    return false;
  v = static_cast<int8_t>(t);
  return true;
}

static inline bool read_i16(const uint8_t* p, size_t len, size_t off, int16_t& v)
{
  if (off + 2 > len)
    return false;
  v = static_cast<int16_t>(static_cast<uint16_t>(p[off]) |
                           (static_cast<uint16_t>(p[off + 1]) << 8));
  return true;
}

static inline bool read_i32(const uint8_t* p, size_t len, size_t off, int32_t& v)
{
  if (off + 4 > len)
    return false;
  v = static_cast<int32_t>(static_cast<uint32_t>(p[off]) |
                           (static_cast<uint32_t>(p[off + 1]) << 8) |
                           (static_cast<uint32_t>(p[off + 2]) << 16) |
                           (static_cast<uint32_t>(p[off + 3]) << 24));
  return true;
}

/**
 * @brief Append hexadecimal address (4 digits) to output stream.
 */
static inline void append_hex_addr(std::ostringstream& oss, size_t addr)
{
  oss << std::hex << std::setw(4) << std::setfill('0') << addr
      << std::dec;  // restore decimal
}

/**
 * @brief Disassemble one instruction at PC and produce a human-readable line.
 */
size_t disasm_one(const uint8_t* code, size_t len, size_t pc, std::string& out)
{
  std::ostringstream oss;
  if (pc >= len)
  {
    out.clear();
    return 0;
  }

  const uint8_t opcode = code[pc];
  const OpInfo info = get_info(opcode);

  // Print address and mnemonic
  append_hex_addr(oss, pc);
  oss << ": " << std::left << std::setw(8) << info.name;

  size_t consumed = 1;  // opcode itself

  // Decode immediate operand if present
  switch (info.imm)
  {
    case ImmKind::None:
      break;

    case ImmKind::I8:
    {
      int8_t v;
      if (read_i8(code, len, pc + consumed, v))
      {
        oss << " " << static_cast<int>(v);
        consumed += 1;
      }
      else
      {
        oss << " <trunc-i8>";
        consumed = len - pc;
      }
      break;
    }

    case ImmKind::I16:
    {
      int16_t v;
      if (read_i16(code, len, pc + consumed, v))
      {
        oss << " " << v;
        consumed += 2;
      }
      else
      {
        oss << " <trunc-i16>";
        consumed = len - pc;
      }
      break;
    }

    case ImmKind::I32:
    {
      int32_t v;
      if (read_i32(code, len, pc + consumed, v))
      {
        oss << " " << v;
        consumed += 4;
      }
      else
      {
        oss << " <trunc-i32>";
        consumed = len - pc;
      }
      break;
    }

    case ImmKind::Rel16:
    {
      int16_t off;
      if (read_i16(code, len, pc + consumed, off))
      {
        const size_t target = static_cast<size_t>(pc + consumed + 2 + off);
        oss << " " << (off >= 0 ? "+" : "") << off << " ; -> ";
        append_hex_addr(oss, target);
        consumed += 2;
      }
      else
      {
        oss << " <trunc-rel16>";
        consumed = len - pc;
      }
      break;
    }

    case ImmKind::Idx16:
    {
      int16_t idx;
      if (read_i16(code, len, pc + consumed, idx))
      {
        oss << " @" << idx;  // CALL idx16
        consumed += 2;
      }
      else
      {
        oss << " <trunc-idx16>";
        consumed = len - pc;
      }
      break;
    }
  }

  out = oss.str();
  return consumed;
}

/**
 * @brief Disassemble all instructions in a bytecode buffer.
 */
std::vector<std::string> disasm_all(const uint8_t* code, size_t len)
{
  std::vector<std::string> lines;
  size_t pc = 0;
  while (pc < len)
  {
    std::string line;
    const size_t c = disasm_one(code, len, pc, line);
    if (c == 0)
      break;
    lines.push_back(std::move(line));
    pc += c;
  }
  return lines;
}

/**
 * @brief Print formatted disassembly to a file stream.
 */
void disasm_print(const uint8_t* code, size_t len, std::FILE* fp)
{
  size_t pc = 0;
  while (pc < len)
  {
    std::string line;
    const size_t c = disasm_one(code, len, pc, line);
    if (c == 0)
      break;
    std::fputs(line.c_str(), fp);
    std::fputc('\n', fp);
    pc += c;
  }
}

}  // namespace v4front
