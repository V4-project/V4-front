// front_compile.cpp (no-exceptions / minimal frontend)
// - Parses a whitespace-separated sequence of tokens
// - Supports: integer literals -> emits [V4_OP_LIT, imm32] for each
// - Automatically appends V4_OP_RET at the end
// - No exceptions: all failures are reported via return codes and error buffer

#include "v4front/front_api.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <cctype>
#include <cerrno>

#include <v4/opcodes.h>   // expects V4_OP_LIT / V4_OP_RET to be defined
#include "prim_table.hpp" // currently unused, kept as a future hook

namespace
{

  // -----------------------------------------------------------------------------
  // Simple byte buffer that never throws
  // -----------------------------------------------------------------------------
  struct CodeBuf
  {
    uint8_t *data = nullptr;
    size_t size = 0;
    size_t cap = 0;

    // Ensure capacity >= size + need. Returns false on OOM.
    bool reserve(size_t need)
    {
      if (cap - size >= need)
        return true;
      size_t newcap = cap ? cap : 64;
      while (newcap - size < need)
      {
        size_t next = newcap * 2;
        if (next <= newcap)
        { // overflow guard
          return false;
        }
        newcap = next;
      }
      void *p = std::realloc(data, newcap);
      if (!p)
        return false;
      data = static_cast<uint8_t *>(p);
      cap = newcap;
      return true;
    }

    bool push8(uint8_t v)
    {
      if (!reserve(1))
        return false;
      data[size++] = v;
      return true;
    }

    bool push32(uint32_t v)
    {
      if (!reserve(4))
        return false;
      // Little-endian layout
      data[size + 0] = static_cast<uint8_t>(v & 0xFFu);
      data[size + 1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
      data[size + 2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
      data[size + 3] = static_cast<uint8_t>((v >> 24) & 0xFFu);
      size += 4;
      return true;
    }
  };

  // -----------------------------------------------------------------------------
  // Error helpers (no exceptions)
  // -----------------------------------------------------------------------------
  inline int set_error(char *err, size_t cap, const char *msg)
  {
    if (err && cap)
    {
      size_t n = std::strlen(msg);
      if (n >= cap)
        n = cap - 1;
      std::memcpy(err, msg, n);
      err[n] = '\0';
    }
    return -1;
  }

  inline int set_error_fmt(char *err, size_t cap, const char *head, const char *tail)
  {
    if (!err || cap == 0)
      return -1;
    size_t h = std::strlen(head);
    size_t t = std::strlen(tail);
    size_t n = (h + t < cap - 1) ? (h + t) : (cap - 1);
    size_t hn = (h < n) ? h : n;
    std::memcpy(err, head, hn);
    size_t tn = (n > hn) ? (n - hn) : 0;
    if (tn)
      std::memcpy(err + hn, tail, tn);
    err[n] = '\0';
    return -1;
  }

  // Parse int32 without throwing. Accepts 0x.. (hex) and 0.. (oct) as strtol base=0.
  inline bool parse_i32(const char *s, int32_t &out)
  {
    if (!s)
      return false;
    // Skip leading spaces
    while (*s && std::isspace(static_cast<unsigned char>(*s)))
      ++s;

    errno = 0;
    char *end = nullptr;
    long v = std::strtol(s, &end, 0);
    if (s == end)
      return false; // no digits
    if (errno == ERANGE)
      return false; // overflow/underflow
    // Skip trailing spaces
    while (*end && std::isspace(static_cast<unsigned char>(*end)))
      ++end;
    if (*end != '\0')
      return false; // trailing junk
    if (v < INT32_MIN || v > INT32_MAX)
      return false;

    out = static_cast<int32_t>(v);
    return true;
  }

  // -----------------------------------------------------------------------------
  // Tokenization (whitespace-separated)
  // -----------------------------------------------------------------------------
  static void tokenize(const char *src, std::vector<std::string> &out)
  {
    out.clear();
    if (!src)
      return;
    const char *p = src;

    while (*p)
    {
      // skip spaces
      while (*p && std::isspace(static_cast<unsigned char>(*p)))
        ++p;
      if (!*p)
        break;

      // begin token
      const char *start = p;
      while (*p && !std::isspace(static_cast<unsigned char>(*p)))
        ++p;
      out.emplace_back(start, static_cast<size_t>(p - start));
    }
  }

} // namespace

// -----------------------------------------------------------------------------
// C API (no exceptions)
// -----------------------------------------------------------------------------
extern "C"
{

  // Compile source into a flat bytecode buffer.
  // Semantics (minimal Tier-0 here):
  //   - For each integer token: emit V4_OP_LIT <imm32>
  //   - Unknown non-integer tokens -> error
  //   - Always append V4_OP_RET at the end
  int v4front_compile(const char *source,
                      V4FrontBuf *out_buf,
                      char *err, size_t err_cap)
  {
    if (!out_buf)
      return set_error(err, err_cap, "out_buf is null");

    out_buf->data = nullptr;
    out_buf->size = 0;

    CodeBuf code;
    std::vector<std::string> toks;
    tokenize(source, toks);

    // TODO (future): use prim_table.hpp to recognize non-immediate words
    // and emit their opcodes. For now, only integer literals are accepted.

    for (size_t i = 0; i < toks.size(); ++i)
    {
      const std::string &tk = toks[i];

      // Try integer literal
      int32_t imm = 0;
      if (parse_i32(tk.c_str(), imm))
      {
        if (!code.push8(static_cast<uint8_t>(V4_OP_LIT)))
          return set_error(err, err_cap, "out of memory (emit LIT)");
        if (!code.push32(static_cast<uint32_t>(imm)))
          return set_error(err, err_cap, "out of memory (emit imm32)");
        continue;
      }

      // If you want to add builtins later, do it here:
      //   if (tk == "RET") { ... }
      //   else if (lookup_prim(tk.c_str(), &op)) { ... }
      // For now, reject non-integers to keep behavior explicit.
      return set_error_fmt(err, err_cap, "unknown token: ", tk.c_str());
    }

    // Append RET
    if (!code.push8(static_cast<uint8_t>(V4_OP_RET)))
      return set_error(err, err_cap, "out of memory (emit RET)");

    // Hand over ownership to caller
    out_buf->data = code.data;
    out_buf->size = code.size;

    // Detach to avoid double-free in our destructor-less struct
    code.data = nullptr;
    code.size = 0;
    code.cap = 0;

    // Success
    if (err && err_cap)
      err[0] = '\0';
    return 0;
  }

  // Optional “compile a named word” variant. Currently a thin wrapper.
  // Kept to match potential front_api.h declaration.
  int v4front_compile_word(const char *name,
                           const char *source,
                           V4FrontBuf *out_buf,
                           char *err, size_t err_cap)
  {
    // Name is currently unused in this minimal Tier-0 implementation.
    (void)name;
    return v4front_compile(source, out_buf, err, err_cap);
  }

  // Free a buffer allocated by v4front_compile[_word].
  void v4front_free(V4FrontBuf *b)
  {
    if (b && b->data)
    {
      std::free(b->data);
      b->data = nullptr;
      b->size = 0;
    }
  }

} // extern "C"
