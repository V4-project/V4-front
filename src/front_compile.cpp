#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <v4/opcodes.hpp>
#include <vector>

#include "v4front/front_api.h"

namespace
{

// Symbol-to-primitive-name mapping for arithmetic operators
// Maps Forth-style symbols to their primitive names in kPrimitiveTable
struct SymbolMapping
{
  const char* symbol;
  const char* prim_name;
};

static constexpr SymbolMapping kSymbolMap[] = {
    {"+", "ADD"},
    {"-", "SUB"},
    {"*", "MUL"},
    {"/", "DIV"},
    // MOD is used as-is, no mapping needed
};

// Look up primitive by symbol or name
// Returns true if found, with opcode written to 'out'
inline bool lookup_primitive(std::string_view token, uint8_t& out)
{
  // First, try symbol mapping
  for (const auto& mapping : kSymbolMap)
  {
    if (token == mapping.symbol)
    {
      token = mapping.prim_name;
      break;
    }
  }

  // Look up in V4's primitive table
  for (const auto& entry : v4::kPrimitiveTable)
  {
    if (token == entry.name)
    {
      out = entry.opcode;
      return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Simple byte buffer that never throws
// ---------------------------------------------------------------------------
struct CodeBuffer
{
  uint8_t* data = nullptr;
  size_t size = 0;
  size_t capacity = 0;

  // Ensure capacity >= size + needed. Returns false on OOM.
  bool reserve(size_t needed)
  {
    if (capacity - size >= needed)
      return true;

    size_t new_capacity = capacity ? capacity : 64;
    while (new_capacity - size < needed)
    {
      size_t next = new_capacity * 2;
      if (next <= new_capacity)
      {
        // Overflow guard
        return false;
      }
      new_capacity = next;
    }

    void* ptr = std::realloc(data, new_capacity);
    if (!ptr)
      return false;

    data = static_cast<uint8_t*>(ptr);
    capacity = new_capacity;
    return true;
  }

  bool push_u8(uint8_t value)
  {
    if (!reserve(1))
      return false;
    data[size++] = value;
    return true;
  }

  bool push_u32(uint32_t value)
  {
    if (!reserve(4))
      return false;

    // Little-endian layout
    data[size + 0] = static_cast<uint8_t>(value & 0xFFu);
    data[size + 1] = static_cast<uint8_t>((value >> 8) & 0xFFu);
    data[size + 2] = static_cast<uint8_t>((value >> 16) & 0xFFu);
    data[size + 3] = static_cast<uint8_t>((value >> 24) & 0xFFu);
    size += 4;
    return true;
  }
};

// ---------------------------------------------------------------------------
// Error handling helpers (no exceptions)
// ---------------------------------------------------------------------------
inline int set_error(char* err, size_t capacity, const char* message)
{
  if (err && capacity)
  {
    size_t length = std::strlen(message);
    if (length >= capacity)
      length = capacity - 1;
    std::memcpy(err, message, length);
    err[length] = '\0';
  }
  return -1;
}

inline int set_error_with_token(char* err, size_t capacity, const char* prefix,
                                const char* token)
{
  if (!err || capacity == 0)
    return -1;

  size_t prefix_len = std::strlen(prefix);
  size_t token_len = std::strlen(token);
  size_t total_len = prefix_len + token_len;

  if (total_len >= capacity)
    total_len = capacity - 1;

  size_t copy_prefix = (prefix_len < total_len) ? prefix_len : total_len;
  std::memcpy(err, prefix, copy_prefix);

  size_t copy_token = (total_len > copy_prefix) ? (total_len - copy_prefix) : 0;
  if (copy_token)
    std::memcpy(err + copy_prefix, token, copy_token);

  err[total_len] = '\0';
  return -1;
}

// ---------------------------------------------------------------------------
// Parse int32_t without throwing
// Accepts: decimal, 0x... (hex), 0... (oct) via strtol base=0
// ---------------------------------------------------------------------------
inline bool parse_int32(const char* str, int32_t& out)
{
  if (!str)
    return false;

  // Skip leading whitespace
  while (*str && std::isspace(static_cast<unsigned char>(*str)))
    ++str;

  errno = 0;
  char* end = nullptr;
  long value = std::strtol(str, &end, 0);

  if (str == end)
    return false;  // No digits parsed

  if (errno == ERANGE)
    return false;  // Overflow/underflow

  // Skip trailing whitespace
  while (*end && std::isspace(static_cast<unsigned char>(*end)))
    ++end;

  if (*end != '\0')
    return false;  // Trailing garbage

  if (value < INT32_MIN || value > INT32_MAX)
    return false;

  out = static_cast<int32_t>(value);
  return true;
}

// ---------------------------------------------------------------------------
// Tokenization (whitespace-separated)
// ---------------------------------------------------------------------------
static void tokenize(const char* source, std::vector<std::string>& tokens)
{
  tokens.clear();
  if (!source)
    return;

  const char* ptr = source;
  while (*ptr)
  {
    // Skip whitespace
    while (*ptr && std::isspace(static_cast<unsigned char>(*ptr)))
      ++ptr;

    if (!*ptr)
      break;

    // Extract token
    const char* start = ptr;
    while (*ptr && !std::isspace(static_cast<unsigned char>(*ptr)))
      ++ptr;

    tokens.emplace_back(start, static_cast<size_t>(ptr - start));
  }
}

}  // namespace

// -----------------------------------------------------------------------------
// C API implementation (no exceptions)
// -----------------------------------------------------------------------------
extern "C"
{
  // Compile source into flat bytecode buffer.
  // Semantics (Tier-0 with arithmetic primitives):
  //   - For each integer token: emit V4_OP_LIT <imm32>
  //   - For recognized primitives (+, -, *, /, MOD): emit their opcode
  //   - Unknown tokens -> error
  //   - Always append V4_OP_RET at the end
  int v4front_compile(const char* source, V4FrontBuf* out_buf, char* err, size_t err_cap)
  {
    if (!out_buf)
      return set_error(err, err_cap, "out_buf is null");

    out_buf->data = nullptr;
    out_buf->size = 0;

    CodeBuffer code;
    std::vector<std::string> tokens;
    tokenize(source, tokens);

    for (size_t i = 0; i < tokens.size(); ++i)
    {
      const std::string& token = tokens[i];

      // Try integer literal first
      int32_t immediate = 0;
      if (parse_int32(token.c_str(), immediate))
      {
        if (!code.push_u8(static_cast<uint8_t>(v4::Op::LIT)))
          return set_error(err, err_cap, "out of memory (emit LIT)");

        if (!code.push_u32(static_cast<uint32_t>(immediate)))
          return set_error(err, err_cap, "out of memory (emit imm32)");

        continue;
      }

      // Try primitive lookup
      uint8_t opcode;
      if (lookup_primitive(token, opcode))
      {
        if (!code.push_u8(opcode))
          return set_error(err, err_cap, "out of memory (emit opcode)");
        continue;
      }

      // Unknown token
      return set_error_with_token(err, err_cap, "unknown token: ", token.c_str());
    }

    // Append RET
    if (!code.push_u8(static_cast<uint8_t>(v4::Op::RET)))
      return set_error(err, err_cap, "out of memory (emit RET)");

    // Transfer ownership to caller
    out_buf->data = code.data;
    out_buf->size = code.size;

    // Detach to avoid double-free
    code.data = nullptr;
    code.size = 0;
    code.capacity = 0;

    // Success
    if (err && err_cap)
      err[0] = '\0';
    return 0;
  }

  // Optional "compile a named word" variant. Currently a thin wrapper.
  int v4front_compile_word(const char* name, const char* source, V4FrontBuf* out_buf,
                           char* err, size_t err_cap)
  {
    // Name is currently unused in this minimal Tier-0 implementation
    (void)name;
    return v4front_compile(source, out_buf, err, err_cap);
  }

  // Free a buffer allocated by v4front_compile[_word]
  void v4front_free(V4FrontBuf* buf)
  {
    if (buf && buf->data)
    {
      std::free(buf->data);
      buf->data = nullptr;
      buf->size = 0;
    }
  }

}  // extern "C"