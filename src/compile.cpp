#include "v4front/compile.hpp"

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
#define MAX_LEAVE_DEPTH 8
#define MAX_WORDS 256

// ---------------------------------------------------------------------------
// Word definition entry (during compilation)
// ---------------------------------------------------------------------------
struct WordDefEntry
{
  char name[64];      // Word name
  uint8_t* code;      // Bytecode for this word
  uint32_t code_len;  // Length of bytecode
};

enum ControlType
{
  IF_CONTROL,
  BEGIN_CONTROL,
  DO_CONTROL
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
  // DO control fields
  uint32_t do_addr;                          // Position after DO for backward jump (for LOOP/+LOOP)
  uint32_t leave_patch_addrs[MAX_LEAVE_DEPTH];  // Positions of JMP offsets to backpatch (for LEAVE)
  int leave_count;                           // Number of LEAVE statements in this DO loop
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
  out_buf->words = nullptr;
  out_buf->word_count = 0;

  // Allocate dynamic bytecode buffer
  uint8_t* bc = nullptr;
  uint32_t bc_size = 0;
  uint32_t bc_cap = 0;
  FrontErr err = FrontErr::OK;

  // Control flow stack for IF/THEN/ELSE
  ControlFrame control_stack[MAX_CONTROL_DEPTH];
  int control_depth = 0;

  // Word dictionary (during compilation)
  WordDefEntry word_dict[MAX_WORDS];
  int word_count = 0;

  // Compilation mode state
  bool in_definition = false;       // Are we inside a : ... ; definition?
  char current_word_name[64] = {0}; // Name of word being defined
  uint8_t* word_bc = nullptr;       // Bytecode buffer for current word
  uint32_t word_bc_size = 0;        // Size of current word bytecode
  uint32_t word_bc_cap = 0;         // Capacity of current word bytecode

  // Pointers to current bytecode buffer (updated when switching modes)
  uint8_t** current_bc = &bc;
  uint32_t* current_bc_size = &bc_size;
  uint32_t* current_bc_cap = &bc_cap;

  // Helper macro for cleanup on error
  #define CLEANUP_AND_RETURN(error_code) \
    do { \
      free(bc); \
      if (word_bc) free(word_bc); \
      return (error_code); \
    } while(0)

  // Handle empty input
  if (!source || !*source)
  {
    // Empty input: just emit RET
    if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                           static_cast<uint8_t>(v4::Op::RET))) != FrontErr::OK)
      CLEANUP_AND_RETURN(err);
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

    // Check for word definition keywords first
    if (str_eq_ci(token, ":"))
    {
      // : (colon) - start word definition

      // Check for nested :
      if (in_definition)
      {
        free(bc);
        free(word_bc);
        return FrontErr::NestedColon;
      }

      // Read next token as word name
      while (*p && isspace((unsigned char)*p))
        p++;
      if (!*p)
      {
        free(bc);
        return FrontErr::ColonWithoutName;
      }

      const char* name_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t name_len = p - name_start;

      if (name_len == 0 || name_len >= sizeof(current_word_name))
      {
        free(bc);
        return FrontErr::ColonWithoutName;
      }

      memcpy(current_word_name, name_start, name_len);
      current_word_name[name_len] = '\0';

      // Check for duplicate word names
      for (int i = 0; i < word_count; i++)
      {
        if (str_eq_ci(word_dict[i].name, current_word_name))
        {
          free(bc);
          return FrontErr::DuplicateWord;
        }
      }

      // Check dictionary full
      if (word_count >= MAX_WORDS)
      {
        free(bc);
        return FrontErr::DictionaryFull;
      }

      // Enter definition mode
      in_definition = true;
      word_bc = nullptr;
      word_bc_size = 0;
      word_bc_cap = 0;

      // Switch to word bytecode buffer
      current_bc = &word_bc;
      current_bc_size = &word_bc_size;
      current_bc_cap = &word_bc_cap;

      continue;
    }
    else if (str_eq_ci(token, ";"))
    {
      // ; (semicolon) - end word definition

      // Check if in definition mode
      if (!in_definition)
      {
        free(bc);
        return FrontErr::SemicolonWithoutColon;
      }

      // Append RET to word bytecode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::RET))) != FrontErr::OK)
      {
        free(bc);
        free(word_bc);
        return err;
      }

      // Add word to dictionary
      strncpy(word_dict[word_count].name, current_word_name, sizeof(word_dict[word_count].name) - 1);
      word_dict[word_count].name[sizeof(word_dict[word_count].name) - 1] = '\0';
      word_dict[word_count].code = word_bc;
      word_dict[word_count].code_len = word_bc_size;
      word_count++;

      // Exit definition mode and switch back to main bytecode buffer
      in_definition = false;
      current_word_name[0] = '\0';
      word_bc = nullptr;  // Don't free - it's now owned by word_dict
      word_bc_size = 0;
      word_bc_cap = 0;

      // Switch back to main bytecode buffer
      current_bc = &bc;
      current_bc_size = &bc_size;
      current_bc_cap = &bc_cap;

      continue;
    }

    // Check for control flow keywords
    if (str_eq_ci(token, "BEGIN"))
    {
      // BEGIN: mark the current position for backward jump
      if (control_depth >= MAX_CONTROL_DEPTH)
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);

      // Push control frame with BEGIN position
      control_stack[control_depth].type = BEGIN_CONTROL;
      control_stack[control_depth].begin_addr = *current_bc_size;
      control_stack[control_depth].has_while = false;
      control_depth++;
      continue;
    }
    else if (str_eq_ci(token, "DO"))
    {
      // DO: ( limit index -- R: -- limit index )
      // Emit: SWAP >R >R
      if (control_depth >= MAX_CONTROL_DEPTH)
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);

      // SWAP: swap limit and index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // >R: push limit to return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // >R: push index to return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save loop start position
      control_stack[control_depth].type = DO_CONTROL;
      control_stack[control_depth].do_addr = *current_bc_size;
      control_stack[control_depth].leave_count = 0;
      control_depth++;
      continue;
    }
    else if (str_eq_ci(token, "UNTIL"))
    {
      // UNTIL: emit JZ with backward offset to BEGIN
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::UntilWithoutBegin);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::UntilWithoutBegin);
      if (frame->has_while)
        CLEANUP_AND_RETURN(FrontErr::UntilAfterWhile);

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset: target - (current + 2)
      uint32_t jz_next_ip = *current_bc_size + 2;
      int16_t offset = (int16_t)(frame->begin_addr - jz_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, offset)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "WHILE"))
    {
      // WHILE: emit JZ with placeholder offset (forward jump to after REPEAT)
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::WhileWithoutBegin);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::WhileWithoutBegin);
      if (frame->has_while)
        CLEANUP_AND_RETURN(FrontErr::DuplicateWhile);

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Update control frame
      frame->while_patch_addr = patch_pos;
      frame->has_while = true;
      continue;
    }
    else if (str_eq_ci(token, "REPEAT"))
    {
      // REPEAT: emit JMP to BEGIN, backpatch WHILE's JZ
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutBegin);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutBegin);
      if (!frame->has_while)
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutWhile);

      // Emit JMP opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset to BEGIN
      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t jmp_offset = (int16_t)(frame->begin_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, jmp_offset)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Backpatch WHILE's JZ to jump to current position
      int16_t jz_offset = (int16_t)(*current_bc_size - (frame->while_patch_addr + 2));
      backpatch_i16_le(*current_bc, frame->while_patch_addr, jz_offset);

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "AGAIN"))
    {
      // AGAIN: emit JMP with backward offset to BEGIN (infinite loop)
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::AgainWithoutBegin);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::AgainWithoutBegin);
      if (frame->has_while)
        CLEANUP_AND_RETURN(FrontErr::AgainAfterWhile);

      // Emit JMP opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset: target - (current + 2)
      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t offset = (int16_t)(frame->begin_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, offset)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "LEAVE"))
    {
      // LEAVE: exit the current DO loop early
      // Emit: R> R> DROP DROP JMP [forward]
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::LeaveWithoutDo);

      // Find the innermost DO control frame
      int do_frame_idx = -1;
      for (int i = control_depth - 1; i >= 0; i--)
      {
        if (control_stack[i].type == DO_CONTROL)
        {
          do_frame_idx = i;
          break;
        }
      }

      if (do_frame_idx < 0)
        CLEANUP_AND_RETURN(FrontErr::LeaveWithoutDo);

      ControlFrame* frame = &control_stack[do_frame_idx];

      // Check if we have space for another LEAVE
      if (frame->leave_count >= MAX_LEAVE_DEPTH)
        CLEANUP_AND_RETURN(FrontErr::LeaveDepthExceeded);

      // R>: pop index from return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // R>: pop limit from return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // DROP: discard index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // DROP: discard limit
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // JMP: jump to loop exit (to be backpatched)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save patch position and emit placeholder
      uint32_t patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Record this LEAVE for backpatching
      frame->leave_patch_addrs[frame->leave_count] = patch_pos;
      frame->leave_count++;

      continue;
    }
    else if (str_eq_ci(token, "LOOP"))
    {
      // LOOP: increment index and loop if index < limit
      // Emit: R> 1+ R> OVER OVER < JZ [forward] SWAP >R >R JMP [backward] [target] DROP
      // DROP
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::LoopWithoutDo);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != DO_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::LoopWithoutDo);

      // R>: pop index from return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // LIT 1
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_i32_le(current_bc, current_bc_size, current_bc_cap, 1)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // ADD: increment index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::ADD))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // R>: pop limit from return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // OVER OVER: ( index limit -- index limit index limit )
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // LT: compare index < limit
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // JZ: jump forward if done (exit loop)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jz_patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // SWAP: ( index limit -- limit index )
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // >R >R: push back to return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // JMP: jump backward to loop start
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t jmp_offset = (int16_t)(frame->do_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, jmp_offset)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Backpatch JZ to exit point
      int16_t jz_offset = (int16_t)(*current_bc_size - (jz_patch_pos + 2));
      backpatch_i16_le(*current_bc, jz_patch_pos, jz_offset);

      // DROP DROP: clean up index and limit
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Backpatch all LEAVE jumps to exit point (current position)
      for (int i = 0; i < frame->leave_count; i++)
      {
        int16_t leave_offset = (int16_t)(*current_bc_size - (frame->leave_patch_addrs[i] + 2));
        backpatch_i16_le(*current_bc, frame->leave_patch_addrs[i], leave_offset);
      }

      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "+LOOP"))
    {
      // +LOOP: add n to index and loop if still in range
      // Similar to LOOP but uses the value on stack instead of 1
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::PLoopWithoutDo);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != DO_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::PLoopWithoutDo);

      // R>: pop index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // ADD: add increment value to index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::ADD))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // R>: pop limit
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // OVER OVER: duplicate for comparison
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // LT: compare
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // JZ: exit if done
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jz_patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // SWAP >R >R: push back
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // JMP: loop back
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t jmp_offset = (int16_t)(frame->do_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, jmp_offset)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Backpatch JZ
      int16_t jz_offset = (int16_t)(*current_bc_size - (jz_patch_pos + 2));
      backpatch_i16_le(*current_bc, jz_patch_pos, jz_offset);

      // DROP DROP: cleanup
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Backpatch all LEAVE jumps to exit point (current position)
      for (int i = 0; i < frame->leave_count; i++)
      {
        int16_t leave_offset = (int16_t)(*current_bc_size - (frame->leave_patch_addrs[i] + 2));
        backpatch_i16_le(*current_bc, frame->leave_patch_addrs[i], leave_offset);
      }

      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "IF"))
    {
      // IF: emit JZ with placeholder offset, push to control stack
      if (control_depth >= MAX_CONTROL_DEPTH)
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

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
        CLEANUP_AND_RETURN(FrontErr::ElseWithoutIf);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::ElseWithoutIf);
      if (frame->has_else)
        CLEANUP_AND_RETURN(FrontErr::DuplicateElse);

      // Emit JMP with placeholder (to skip ELSE clause)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jmp_patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Now backpatch JZ to jump to current position (start of ELSE clause)
      // offset = current_pos - (jz_patch_addr + 2)
      int16_t jz_offset = (int16_t)(*current_bc_size - (frame->jz_patch_addr + 2));
      backpatch_i16_le(*current_bc, frame->jz_patch_addr, jz_offset);

      // Update control frame
      frame->jmp_patch_addr = jmp_patch_pos;
      frame->has_else = true;
      continue;
    }
    else if (str_eq_ci(token, "THEN"))
    {
      // THEN: backpatch the last IF or ELSE jump
      if (control_depth <= 0)
        CLEANUP_AND_RETURN(FrontErr::ThenWithoutIf);

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
        CLEANUP_AND_RETURN(FrontErr::ThenWithoutIf);

      control_depth--;

      if (frame->has_else)
      {
        // Backpatch the JMP from ELSE
        int16_t jmp_offset = (int16_t)(*current_bc_size - (frame->jmp_patch_addr + 2));
        backpatch_i16_le(*current_bc, frame->jmp_patch_addr, jmp_offset);
      }
      else
      {
        // Backpatch the JZ from IF
        int16_t jz_offset = (int16_t)(*current_bc_size - (frame->jz_patch_addr + 2));
        backpatch_i16_le(*current_bc, frame->jz_patch_addr, jz_offset);
      }
      continue;
    }
    else if (str_eq_ci(token, "EXIT"))
    {
      // EXIT: early return from word (emit RET)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::RET))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }

    // Try looking up word in dictionary
    {
      int word_idx = -1;
      for (int i = 0; i < word_count; i++)
      {
        if (str_eq_ci(token, word_dict[i].name))
        {
          word_idx = i;
          break;
        }
      }

      if (word_idx >= 0)
      {
        // Found a word - emit CALL instruction
        if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                               static_cast<uint8_t>(v4::Op::CALL))) != FrontErr::OK)
          CLEANUP_AND_RETURN(err);

        // Emit 16-bit word index (little-endian)
        if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap,
                                 static_cast<int16_t>(word_idx))) != FrontErr::OK)
          CLEANUP_AND_RETURN(err);

        continue;
      }
    }

    // Try parsing as integer
    int32_t val;
    if (try_parse_int(token, &val))
    {
      // Emit: [LIT] [imm32_le]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_i32_le(current_bc, current_bc_size, current_bc_cap, val)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
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
    // Return stack operators
    else if (str_eq_ci(token, ">R"))
    {
      opcode = v4::Op::TOR;
      found = true;
    }
    else if (str_eq_ci(token, "R>"))
    {
      opcode = v4::Op::FROMR;
      found = true;
    }
    else if (str_eq_ci(token, "R@"))
    {
      opcode = v4::Op::RFETCH;
      found = true;
    }
    else if (str_eq_ci(token, "I"))
    {
      // I: current loop index = R@
      opcode = v4::Op::RFETCH;
      found = true;
    }
    else if (str_eq_ci(token, "J"))
    {
      // J: outer loop index
      // Emit: R> R> R> DUP >R >R >R

      // R> R> R>: pop current loop (index, limit) and next index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // DUP: copy the outer loop index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // >R >R >R: restore return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      found = true;
      continue;  // J emits multiple instructions, so continue
    }
    else if (str_eq_ci(token, "K"))
    {
      // K: outer outer loop index
      // Emit: R> R> R> R> R> DUP >R >R >R >R >R

      // R> x 5: pop two loops and next index
      for (int i = 0; i < 5; i++)
      {
        if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                               static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
          CLEANUP_AND_RETURN(err);
      }

      // DUP: copy the outer outer loop index
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // >R x 5: restore return stack
      for (int i = 0; i < 5; i++)
      {
        if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                               static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
          CLEANUP_AND_RETURN(err);
      }

      found = true;
      continue;  // K emits multiple instructions, so continue
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
    // Memory access operators (symbols are case-sensitive)
    else if (strcmp(token, "@") == 0)
    {
      opcode = v4::Op::LOAD;
      found = true;
    }
    else if (strcmp(token, "!") == 0)
    {
      opcode = v4::Op::STORE;
      found = true;
    }

    if (!found)
      CLEANUP_AND_RETURN(FrontErr::UnknownToken);

    if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                           static_cast<uint8_t>(opcode))) != FrontErr::OK)
      CLEANUP_AND_RETURN(err);
  }

  // Check for unclosed control structures
  if (control_depth > 0)
  {
    // Check what kind of unclosed structure
    if (control_stack[control_depth - 1].type == IF_CONTROL)
      CLEANUP_AND_RETURN(FrontErr::UnclosedIf);
    else if (control_stack[control_depth - 1].type == DO_CONTROL)
      CLEANUP_AND_RETURN(FrontErr::UnclosedDo);
    else
      CLEANUP_AND_RETURN(FrontErr::UnclosedBegin);
  }

  // Check for unclosed word definition
  if (in_definition)
    CLEANUP_AND_RETURN(FrontErr::UnclosedColon);

  // Transfer word_dict to out_buf->words
  if (word_count > 0)
  {
    // Allocate words array
    out_buf->words = (V4FrontWord*)malloc(sizeof(V4FrontWord) * word_count);
    if (!out_buf->words)
      CLEANUP_AND_RETURN(FrontErr::OutOfMemory);

    // Copy each word definition
    for (int i = 0; i < word_count; i++)
    {
      // Copy name
      out_buf->words[i].name = strdup(word_dict[i].name);
      if (!out_buf->words[i].name)
      {
        // Cleanup already allocated names
        for (int j = 0; j < i; j++)
        {
          free(out_buf->words[j].name);
        }
        free(out_buf->words);
        CLEANUP_AND_RETURN(FrontErr::OutOfMemory);
      }

      // Transfer ownership of code (no copy needed)
      out_buf->words[i].code = word_dict[i].code;
      out_buf->words[i].code_len = word_dict[i].code_len;
    }

    out_buf->word_count = word_count;
  }
  else
  {
    out_buf->words = nullptr;
    out_buf->word_count = 0;
  }

  // Append RET only if the last instruction is not an unconditional jump
  // (unconditional JMP makes following code unreachable)
  bool needs_ret = true;
  if (*current_bc_size >= 3)
  {
    uint8_t last_opcode = (*current_bc)[*current_bc_size - 3];
    if (last_opcode == static_cast<uint8_t>(v4::Op::JMP))
    {
      // Last instruction is unconditional jump (from AGAIN or REPEAT) - no RET needed
      needs_ret = false;
    }
  }

  if (needs_ret)
  {
    if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                           static_cast<uint8_t>(v4::Op::RET))) != FrontErr::OK)
      CLEANUP_AND_RETURN(err);
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
  if (!buf)
    return;

  // Free word definitions
  if (buf->words)
  {
    for (int i = 0; i < buf->word_count; i++)
    {
      free(buf->words[i].name);
      free(buf->words[i].code);
    }
    free(buf->words);
    buf->words = nullptr;
    buf->word_count = 0;
  }

  // Free main bytecode
  if (buf->data)
  {
    free(buf->data);
    buf->data = nullptr;
    buf->size = 0;
  }
}