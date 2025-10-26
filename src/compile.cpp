#include "v4front/compile.hpp"

#include <cassert>
#include <cctype>   // isspace
#include <cstdlib>  // malloc, free, strtol
#include <cstring>  // strcmp, strlen, strncpy

#include "v4/opcodes.hpp"  // v4::Op
#include "v4front/errors.hpp"

using namespace v4front;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Write an error message into the user-provided buffer
static void write_error(char* err, size_t err_cap, FrontErr code)
{
  if (err && err_cap > 0)
  {
    const char* msg = front_err_str(code);
    size_t len = strlen(msg);
    if (len >= err_cap)
      len = err_cap - 1;
    memcpy(err, msg, len);
    err[len] = '\0';
  }
}

// Write a custom error message (for detailed errors)
static void write_error_msg(char* err, size_t err_cap, const char* msg)
{
  if (err && err_cap > 0)
  {
    size_t len = strlen(msg);
    if (len >= err_cap)
      len = err_cap - 1;
    memcpy(err, msg, len);
    err[len] = '\0';
  }
}

// Append a byte to a dynamically growing buffer
static FrontErr append_byte(uint8_t** buf, uint32_t* size, uint32_t* cap, uint8_t byte)
{
  if (*size >= *cap)
  {
    uint32_t new_cap = (*cap == 0) ? 64 : (*cap * 2);
    uint8_t* new_buf = (uint8_t*)realloc(*buf, new_cap);
    if (!new_buf)
      return FrontErr::OutOfMemory;
    *buf = new_buf;
    *cap = new_cap;
  }
  (*buf)[(*size)++] = byte;
  return FrontErr::OK;
}

// Append a 32-bit integer in little-endian format
static FrontErr append_i32_le(uint8_t** buf, uint32_t* size, uint32_t* cap, int32_t val)
{
  FrontErr err;
  if ((err = append_byte(buf, size, cap, (uint8_t)(val & 0xFF))) != FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, (uint8_t)((val >> 8) & 0xFF))) != FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, (uint8_t)((val >> 16) & 0xFF))) != FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, (uint8_t)((val >> 24) & 0xFF))) != FrontErr::OK)
    return err;
  return FrontErr::OK;
}

// Try parsing a token as an integer
static bool try_parse_int(const char* token, int32_t* out)
{
  char* endptr = nullptr;
  long val = strtol(token, &endptr, 0);  // base=0 auto-detects hex/oct
  if (endptr == token || *endptr != '\0')
    return false;
  *out = (int32_t)val;
  return true;
}

// ---------------------------------------------------------------------------
// Main compilation logic
// ---------------------------------------------------------------------------

static FrontErr compile_internal(const char* source, V4FrontBuf* out_buf)
{
  assert(out_buf);

  // Initialize output buffer
  out_buf->data = nullptr;
  out_buf->size = 0;

  // Allocate dynamic bytecode buffer
  uint8_t* bc = nullptr;
  uint32_t bc_size = 0;
  uint32_t bc_cap = 0;
  FrontErr err = FrontErr::OK;

  // Handle empty input
  if (!source || !*source)
  {
    // Empty input: just emit RET
    if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(v4::Op::RET))) !=
        FrontErr::OK)
    {
      free(bc);
      return err;
    }
    out_buf->data = bc;
    out_buf->size = bc_size;
    return FrontErr::OK;
  }

  // Tokenization and code generation
  const char* p = source;
  char token[256];

  while (*p)
  {
    // Skip whitespace
    while (*p && isspace((unsigned char)*p))
      p++;
    if (!*p)
      break;

    // Extract token
    const char* token_start = p;
    while (*p && !isspace((unsigned char)*p))
      p++;
    size_t token_len = p - token_start;
    if (token_len >= sizeof(token))
      token_len = sizeof(token) - 1;
    memcpy(token, token_start, token_len);
    token[token_len] = '\0';

    // Try parsing as integer
    int32_t val;
    if (try_parse_int(token, &val))
    {
      // Emit: [LIT] [imm32_le]
      if ((err = append_byte(&bc, &bc_size, &bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT))) != FrontErr::OK)
      {
        free(bc);
        return err;
      }
      if ((err = append_i32_le(&bc, &bc_size, &bc_cap, val)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }
      continue;
    }

    // Try matching operators
    v4::Op opcode = v4::Op::RET;  // placeholder
    bool found = false;

    // Stack operators
    if (strcmp(token, "DUP") == 0)
    {
      opcode = v4::Op::DUP;
      found = true;
    }
    else if (strcmp(token, "DROP") == 0)
    {
      opcode = v4::Op::DROP;
      found = true;
    }
    else if (strcmp(token, "SWAP") == 0)
    {
      opcode = v4::Op::SWAP;
      found = true;
    }
    else if (strcmp(token, "OVER") == 0)
    {
      opcode = v4::Op::OVER;
      found = true;
    }
    // Arithmetic operators
    if (strcmp(token, "+") == 0)
    {
      opcode = v4::Op::ADD;
      found = true;
    }
    else if (strcmp(token, "-") == 0)
    {
      opcode = v4::Op::SUB;
      found = true;
    }
    else if (strcmp(token, "*") == 0)
    {
      opcode = v4::Op::MUL;
      found = true;
    }
    else if (strcmp(token, "/") == 0)
    {
      opcode = v4::Op::DIV;
      found = true;
    }
    else if (strcmp(token, "MOD") == 0)
    {
      opcode = v4::Op::MOD;
      found = true;
    }
    // Comparison operators
    else if (strcmp(token, "=") == 0 || strcmp(token, "==") == 0)
    {
      opcode = v4::Op::EQ;
      found = true;
    }
    else if (strcmp(token, "<>") == 0 || strcmp(token, "!=") == 0)
    {
      opcode = v4::Op::NE;
      found = true;
    }
    else if (strcmp(token, "<") == 0)
    {
      opcode = v4::Op::LT;
      found = true;
    }
    else if (strcmp(token, "<=") == 0)
    {
      opcode = v4::Op::LE;
      found = true;
    }
    else if (strcmp(token, ">") == 0)
    {
      opcode = v4::Op::GT;
      found = true;
    }
    else if (strcmp(token, ">=") == 0)
    {
      opcode = v4::Op::GE;
      found = true;
    }
    // Bitwise operators
    else if (strcmp(token, "AND") == 0)
    {
      opcode = v4::Op::AND;
      found = true;
    }
    else if (strcmp(token, "OR") == 0)
    {
      opcode = v4::Op::OR;
      found = true;
    }
    else if (strcmp(token, "XOR") == 0)
    {
      opcode = v4::Op::XOR;
      found = true;
    }
    else if (strcmp(token, "INVERT") == 0)
    {
      opcode = v4::Op::INVERT;
      found = true;
    }

    if (!found)
    {
      // Unknown token
      free(bc);
      return FrontErr::UnknownToken;
    }

    if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(opcode))) !=
        FrontErr::OK)
    {
      free(bc);
      return err;
    }
  }

  // Append RET
  if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(v4::Op::RET))) !=
      FrontErr::OK)
  {
    free(bc);
    return err;
  }

  out_buf->data = bc;
  out_buf->size = bc_size;
  return FrontErr::OK;
}

// ---------------------------------------------------------------------------
// Public C API implementations
// ---------------------------------------------------------------------------

extern "C" const char* v4front_err_str(v4front_err code)
{
  return front_err_str(static_cast<FrontErr>(code));
}

extern "C" v4front_err v4front_compile(const char* source, V4FrontBuf* out_buf, char* err,
                                       size_t err_cap)
{
  if (!out_buf)
  {
    write_error_msg(err, err_cap, "output buffer is NULL");
    return front_err_to_int(FrontErr::BufferTooSmall);
  }

  FrontErr result = compile_internal(source, out_buf);

  if (result != FrontErr::OK)
  {
    write_error(err, err_cap, result);
  }

  return front_err_to_int(result);
}

extern "C" v4front_err v4front_compile_word(const char* name, const char* source,
                                            V4FrontBuf* out_buf, char* err,
                                            size_t err_cap)
{
  // Current implementation ignores 'name' parameter
  (void)name;
  return v4front_compile(source, out_buf, err, err_cap);
}

extern "C" void v4front_free(V4FrontBuf* buf)
{
  if (buf && buf->data)
  {
    free(buf->data);
    buf->data = nullptr;
    buf->size = 0;
  }
}