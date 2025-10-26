#include "v4front/compile.h"

#include <cassert>
#include <cctype>
#include <cstdlib>
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/errors.hpp"

using namespace v4front;

// ---------------------------------------------------------------------------
// Helper: case-insensitive string comparison
// ---------------------------------------------------------------------------
static bool str_eq_ci(const char* a, const char* b)
{
  while (*a && *b)
  {
    if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
      return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

// ---------------------------------------------------------------------------
// Helper: write error message to buffer
// ---------------------------------------------------------------------------
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

static void write_error(char* err, size_t err_cap, FrontErr code)
{
  write_error_msg(err, err_cap, front_err_str(code));
}

// ---------------------------------------------------------------------------
// Dynamic bytecode buffer management
// ---------------------------------------------------------------------------

// Append a single byte to the bytecode buffer
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

// Append a 16-bit integer in little-endian format
static FrontErr append_i16_le(uint8_t** buf, uint32_t* size, uint32_t* cap, int16_t val)
{
  FrontErr err;
  if ((err = append_byte(buf, size, cap, (uint8_t)(val & 0xFF))) != FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, (uint8_t)((val >> 8) & 0xFF))) != FrontErr::OK)
    return err;
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

// Backpatch a 16-bit offset at a specific position
static void backpatch_i16_le(uint8_t* buf, uint32_t pos, int16_t val)
{
  buf[pos] = (uint8_t)(val & 0xFF);
  buf[pos + 1] = (uint8_t)((val >> 8) & 0xFF);
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
// Control flow stack for IF/THEN/ELSE and BEGIN/UNTIL backpatching
// ---------------------------------------------------------------------------
#define MAX_CONTROL_DEPTH 32

enum ControlType
{
  IF_CONTROL,
  BEGIN_CONTROL
};

struct ControlFrame
{
  ControlType type;
  // IF control fields
  uint32_t jz_patch_addr;   // Position of JZ offset to backpatch (for IF)
  uint32_t jmp_patch_addr;  // Position of JMP offset to backpatch (for ELSE)
  bool has_else;            // Whether this IF has an ELSE clause
  // BEGIN control fields
  uint32_t begin_addr;        // Position of BEGIN for backward jump (for UNTIL/REPEAT)
  uint32_t while_patch_addr;  // Position of JZ offset to backpatch (for WHILE)
  bool has_while;             // Whether this BEGIN has a WHILE clause
};

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

  // Control flow stack for IF/THEN/ELSE
  ControlFrame control_stack[MAX_CONTROL_DEPTH];
  int control_depth = 0;

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

    // Check for control flow keywords first
    if (str_eq_ci(token, "BEGIN"))
    {
      // BEGIN: mark the current position for backward jump
      if (control_depth >= MAX_CONTROL_DEPTH)
      {
        free(bc);
        return FrontErr::ControlDepthExceeded;
      }

      // Push control frame with BEGIN position
      control_stack[control_depth].type = BEGIN_CONTROL;
      control_stack[control_depth].begin_addr = bc_size;
      control_stack[control_depth].has_while = false;
      control_depth++;
      continue;
    }
    else if (str_eq_ci(token, "UNTIL"))
    {
      // UNTIL: emit JZ with backward offset to BEGIN
      if (control_depth <= 0)
      {
        free(bc);
        return FrontErr::UntilWithoutBegin;
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        free(bc);
        return FrontErr::UntilWithoutBegin;
      }
      if (frame->has_while)
      {
        free(bc);
        return FrontErr::UntilAfterWhile;
      }

      // Emit JZ opcode
      if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(v4::Op::JZ))) !=
          FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Calculate backward offset: target - (current + 2)
      uint32_t jz_next_ip = bc_size + 2;
      int16_t offset = (int16_t)(frame->begin_addr - jz_next_ip);

      if ((err = append_i16_le(&bc, &bc_size, &bc_cap, offset)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "WHILE"))
    {
      // WHILE: emit JZ with placeholder offset (forward jump to after REPEAT)
      if (control_depth <= 0)
      {
        free(bc);
        return FrontErr::WhileWithoutBegin;
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        free(bc);
        return FrontErr::WhileWithoutBegin;
      }
      if (frame->has_while)
      {
        free(bc);
        return FrontErr::DuplicateWhile;
      }

      // Emit JZ opcode
      if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(v4::Op::JZ))) !=
          FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = bc_size;
      if ((err = append_i16_le(&bc, &bc_size, &bc_cap, 0)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Update control frame
      frame->while_patch_addr = patch_pos;
      frame->has_while = true;
      continue;
    }
    else if (str_eq_ci(token, "REPEAT"))
    {
      // REPEAT: emit JMP to BEGIN, backpatch WHILE's JZ
      if (control_depth <= 0)
      {
        free(bc);
        return FrontErr::RepeatWithoutBegin;
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        free(bc);
        return FrontErr::RepeatWithoutBegin;
      }
      if (!frame->has_while)
      {
        free(bc);
        return FrontErr::RepeatWithoutWhile;
      }

      // Emit JMP opcode
      if ((err = append_byte(&bc, &bc_size, &bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Calculate backward offset to BEGIN
      uint32_t jmp_next_ip = bc_size + 2;
      int16_t jmp_offset = (int16_t)(frame->begin_addr - jmp_next_ip);

      if ((err = append_i16_le(&bc, &bc_size, &bc_cap, jmp_offset)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Backpatch WHILE's JZ to jump to current position
      int16_t jz_offset = (int16_t)(bc_size - (frame->while_patch_addr + 2));
      backpatch_i16_le(bc, frame->while_patch_addr, jz_offset);

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "IF"))
    {
      // IF: emit JZ with placeholder offset, push to control stack
      if (control_depth >= MAX_CONTROL_DEPTH)
      {
        free(bc);
        return FrontErr::ControlDepthExceeded;
      }

      // Emit JZ opcode
      if ((err = append_byte(&bc, &bc_size, &bc_cap, static_cast<uint8_t>(v4::Op::JZ))) !=
          FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = bc_size;
      if ((err = append_i16_le(&bc, &bc_size, &bc_cap, 0)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Push control frame
      control_stack[control_depth].type = IF_CONTROL;
      control_stack[control_depth].jz_patch_addr = patch_pos;
      control_stack[control_depth].jmp_patch_addr = 0;
      control_stack[control_depth].has_else = false;
      control_depth++;
      continue;
    }
    else if (str_eq_ci(token, "ELSE"))
    {
      // ELSE: emit JMP, then backpatch JZ to jump past the JMP
      if (control_depth <= 0)
      {
        free(bc);
        return FrontErr::ElseWithoutIf;
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
      {
        free(bc);
        return FrontErr::ElseWithoutIf;
      }
      if (frame->has_else)
      {
        free(bc);
        return FrontErr::DuplicateElse;
      }

      // Emit JMP with placeholder (to skip ELSE clause)
      if ((err = append_byte(&bc, &bc_size, &bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      uint32_t jmp_patch_pos = bc_size;
      if ((err = append_i16_le(&bc, &bc_size, &bc_cap, 0)) != FrontErr::OK)
      {
        free(bc);
        return err;
      }

      // Now backpatch JZ to jump to current position (start of ELSE clause)
      // offset = current_pos - (jz_patch_addr + 2)
      int16_t jz_offset = (int16_t)(bc_size - (frame->jz_patch_addr + 2));
      backpatch_i16_le(bc, frame->jz_patch_addr, jz_offset);

      // Update control frame
      frame->jmp_patch_addr = jmp_patch_pos;
      frame->has_else = true;
      continue;
    }
    else if (str_eq_ci(token, "THEN"))
    {
      // THEN: backpatch the last IF or ELSE jump
      if (control_depth <= 0)
      {
        free(bc);
        return FrontErr::ThenWithoutIf;
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
      {
        free(bc);
        return FrontErr::ThenWithoutIf;
      }

      control_depth--;

      if (frame->has_else)
      {
        // Backpatch the JMP from ELSE
        int16_t jmp_offset = (int16_t)(bc_size - (frame->jmp_patch_addr + 2));
        backpatch_i16_le(bc, frame->jmp_patch_addr, jmp_offset);
      }
      else
      {
        // Backpatch the JZ from IF
        int16_t jz_offset = (int16_t)(bc_size - (frame->jz_patch_addr + 2));
        backpatch_i16_le(bc, frame->jz_patch_addr, jz_offset);
      }
      continue;
    }

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

    // Try matching operators (case-insensitive for word operators)
    v4::Op opcode = v4::Op::RET;  // placeholder
    bool found = false;

    // Stack operators
    if (str_eq_ci(token, "DUP"))
    {
      opcode = v4::Op::DUP;
      found = true;
    }
    else if (str_eq_ci(token, "DROP"))
    {
      opcode = v4::Op::DROP;
      found = true;
    }
    else if (str_eq_ci(token, "SWAP"))
    {
      opcode = v4::Op::SWAP;
      found = true;
    }
    else if (str_eq_ci(token, "OVER"))
    {
      opcode = v4::Op::OVER;
      found = true;
    }
    // Arithmetic operators (symbols are case-sensitive)
    else if (strcmp(token, "+") == 0)
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
    else if (str_eq_ci(token, "MOD"))
    {
      opcode = v4::Op::MOD;
      found = true;
    }
    // Comparison operators (symbols are case-sensitive)
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
    else if (str_eq_ci(token, "AND"))
    {
      opcode = v4::Op::AND;
      found = true;
    }
    else if (str_eq_ci(token, "OR"))
    {
      opcode = v4::Op::OR;
      found = true;
    }
    else if (str_eq_ci(token, "XOR"))
    {
      opcode = v4::Op::XOR;
      found = true;
    }
    else if (str_eq_ci(token, "INVERT"))
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

  // Check for unclosed control structures
  if (control_depth > 0)
  {
    free(bc);
    // Check what kind of unclosed structure
    if (control_stack[control_depth - 1].type == IF_CONTROL)
      return FrontErr::UnclosedIf;
    else
      return FrontErr::UnclosedBegin;
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