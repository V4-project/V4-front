#include "v4front/compile.hpp"

#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "v4/opcodes.hpp"
#include "v4front/errors.hpp"

using namespace v4front;

// ---------------------------------------------------------------------------
// Platform-specific string duplication
// ---------------------------------------------------------------------------
static char* portable_strdup(const char* s)
{
#ifdef _MSC_VER
  return _strdup(s);
#else
  return strdup(s);
#endif
}

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
// Forward declarations
// ---------------------------------------------------------------------------
static FrontErr append_byte(uint8_t** buf, uint32_t* size, uint32_t* cap, uint8_t byte);

// ---------------------------------------------------------------------------
// Opcode dispatch table for simple single-byte instructions
// ---------------------------------------------------------------------------
struct OpcodeMapping
{
  const char* token;
  v4::Op opcode;
  bool case_sensitive;  // true for symbols like +, -, @, !
};

static const OpcodeMapping OPCODE_TABLE[] = {
    // Stack operations
    {"DUP", v4::Op::DUP, false},
    {"DROP", v4::Op::DROP, false},
    {"SWAP", v4::Op::SWAP, false},
    {"OVER", v4::Op::OVER, false},

    // Return stack operations
    {">R", v4::Op::TOR, false},
    {"R>", v4::Op::FROMR, false},
    {"R@", v4::Op::RFETCH, false},
    {"I", v4::Op::RFETCH, false},  // I is alias for R@

    // Arithmetic operators
    {"+", v4::Op::ADD, true},
    {"-", v4::Op::SUB, true},
    {"*", v4::Op::MUL, true},
    {"/", v4::Op::DIV, true},
    {"MOD", v4::Op::MOD, false},
    {"1+", v4::Op::INC, false},
    {"1-", v4::Op::DEC, false},
    {"U/", v4::Op::DIVU, false},
    {"UMOD", v4::Op::MODU, false},

    // Comparison operators
    {"=", v4::Op::EQ, true},
    {"==", v4::Op::EQ, true},
    {"<>", v4::Op::NE, true},
    {"!=", v4::Op::NE, true},
    {"<", v4::Op::LT, true},
    {"<=", v4::Op::LE, true},
    {">", v4::Op::GT, true},
    {">=", v4::Op::GE, true},
    {"U<", v4::Op::LTU, false},
    {"U<=", v4::Op::LEU, false},

    // Bitwise operators
    {"AND", v4::Op::AND, false},
    {"OR", v4::Op::OR, false},
    {"XOR", v4::Op::XOR, false},
    {"INVERT", v4::Op::INVERT, false},
    {"LSHIFT", v4::Op::SHL, false},
    {"RSHIFT", v4::Op::SHR, false},
    {"ARSHIFT", v4::Op::SAR, false},

    // Memory access
    {"@", v4::Op::LOAD, true},
    {"!", v4::Op::STORE, true},
    {"C@", v4::Op::LOAD8U, false},
    {"C!", v4::Op::STORE8, false},
    {"W@", v4::Op::LOAD16U, false},
    {"W!", v4::Op::STORE16, false},

    // Local variable access (optimized for indices 0 and 1)
    {"L@0", v4::Op::LGET0, false},
    {"L@1", v4::Op::LGET1, false},
    {"L!0", v4::Op::LSET0, false},
    {"L!1", v4::Op::LSET1, false},

    // Task management
    {"SPAWN", v4::Op::TASK_SPAWN, false},
    {"TASK-EXIT", v4::Op::TASK_EXIT, false},
    {"SLEEP", v4::Op::TASK_SLEEP, false},
    {"MS", v4::Op::TASK_SLEEP, false},  // alias for SLEEP
    {"YIELD", v4::Op::TASK_YIELD, false},
    {"PAUSE", v4::Op::TASK_YIELD, false},  // alias for YIELD
    {"CRITICAL", v4::Op::CRITICAL_ENTER, false},
    {"UNCRITICAL", v4::Op::CRITICAL_EXIT, false},
    {"SEND", v4::Op::TASK_SEND, false},
    {"RECEIVE", v4::Op::TASK_RECEIVE, false},
    {"RECEIVE-BLOCKING", v4::Op::TASK_RECEIVE_BLOCKING, false},
    {"ME", v4::Op::TASK_SELF, false},
    {"TASKS", v4::Op::TASK_COUNT, false},

    // Sentinel (end of table)
    {nullptr, v4::Op::RET, false}};

// Helper: Look up simple opcode in dispatch table
// Returns true and sets opcode if found, false otherwise
static bool lookup_simple_opcode(const char* token, v4::Op* opcode)
{
  for (const OpcodeMapping* entry = OPCODE_TABLE; entry->token != nullptr; ++entry)
  {
    bool match = entry->case_sensitive ? (strcmp(token, entry->token) == 0)
                                       : str_eq_ci(token, entry->token);
    if (match)
    {
      *opcode = entry->opcode;
      return true;
    }
  }
  return false;
}

// Helper: Emit J instruction (outer loop index)
// Emits: R> R> R> DUP >R >R >R
static FrontErr emit_j_instruction(uint8_t** buf, uint32_t* size, uint32_t* cap)
{
  FrontErr err;

  // R> R> R>: pop current loop (index, limit) and next index
  for (int i = 0; i < 3; i++)
  {
    if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::FROMR))) !=
        FrontErr::OK)
      return err;
  }

  // DUP: copy the outer loop index
  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::DUP))) !=
      FrontErr::OK)
    return err;

  // >R >R >R: restore return stack
  for (int i = 0; i < 3; i++)
  {
    if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::TOR))) !=
        FrontErr::OK)
      return err;
  }

  return FrontErr::OK;
}

// Helper: Emit ROT instruction (rotate three elements)
// Emits: >R SWAP R> SWAP
static FrontErr emit_rot_instruction(uint8_t** buf, uint32_t* size, uint32_t* cap)
{
  FrontErr err;

  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::TOR))) !=
      FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::SWAP))) !=
      FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::FROMR))) !=
      FrontErr::OK)
    return err;
  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::SWAP))) !=
      FrontErr::OK)
    return err;

  return FrontErr::OK;
}

// Helper: Emit K instruction (outer outer loop index)
// Emits: R> R> R> R> R> DUP >R >R >R >R >R
static FrontErr emit_k_instruction(uint8_t** buf, uint32_t* size, uint32_t* cap)
{
  FrontErr err;

  // R> x 5: pop two loops and next index
  for (int i = 0; i < 5; i++)
  {
    if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::FROMR))) !=
        FrontErr::OK)
      return err;
  }

  // DUP: copy the outer outer loop index
  if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::DUP))) !=
      FrontErr::OK)
    return err;

  // >R x 5: restore return stack
  for (int i = 0; i < 5; i++)
  {
    if ((err = append_byte(buf, size, cap, static_cast<uint8_t>(v4::Op::TOR))) !=
        FrontErr::OK)
      return err;
  }

  return FrontErr::OK;
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
// Compilation limits (can be overridden at compile time with -D flags)
// ---------------------------------------------------------------------------

// Maximum nesting depth for control structures (IF/THEN/ELSE, BEGIN/UNTIL, DO/LOOP)
#ifndef MAX_CONTROL_DEPTH
#define MAX_CONTROL_DEPTH 32
#endif

// Maximum nesting depth for LEAVE statements within DO loops
#ifndef MAX_LEAVE_DEPTH
#define MAX_LEAVE_DEPTH 8
#endif

// Maximum number of word definitions
#ifndef MAX_WORDS
#define MAX_WORDS 256
#endif

// Maximum length of word names (including null terminator)
#ifndef MAX_WORD_NAME_LEN
#define MAX_WORD_NAME_LEN 64
#endif

// Maximum length of tokens (including null terminator)
#ifndef MAX_TOKEN_LEN
#define MAX_TOKEN_LEN 256
#endif

// ---------------------------------------------------------------------------
// Word definition entry (during compilation)
// ---------------------------------------------------------------------------
struct WordDefEntry
{
  char name[MAX_WORD_NAME_LEN];  // Word name
  uint8_t* code;                 // Bytecode for this word
  uint32_t code_len;             // Length of bytecode
};

// ---------------------------------------------------------------------------
// Compiler context structures (for stateful compilation)
// ---------------------------------------------------------------------------

// Word entry in compiler context (maps name to VM word index)
struct ContextWordEntry
{
  char* name;       // Word name (dynamically allocated)
  int vm_word_idx;  // VM word index
};

// Compiler context for stateful compilation (opaque to C API users)
struct V4FrontContext
{
  ContextWordEntry* words;  // Array of registered words
  int word_count;           // Number of registered words
  int word_capacity;        // Capacity of words array
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
  uint32_t do_addr;  // Position after DO for backward jump (for LOOP/+LOOP)
  uint32_t leave_patch_addrs[MAX_LEAVE_DEPTH];  // Positions of JMP offsets to backpatch
                                                // (for LEAVE)
  int leave_count;  // Number of LEAVE statements in this DO loop
};

// ---------------------------------------------------------------------------
// Main compilation logic
// ---------------------------------------------------------------------------

// Helper function to clean up allocated resources during compilation
static void cleanup_compile_state(uint8_t* bc, uint8_t* word_bc, WordDefEntry* word_dict,
                                  int word_count)
{
  free(bc);
  if (word_bc)
    free(word_bc);
  for (int i = 0; i < word_count; i++)
  {
    if (word_dict[i].code)
      free(word_dict[i].code);
  }
}

// Helper function to handle : (colon) - start word definition
static FrontErr handle_colon_start(const char** p, bool* in_definition,
                                   char* current_word_name, uint8_t** word_bc,
                                   uint32_t* word_bc_size, uint32_t* word_bc_cap,
                                   uint8_t*** current_bc, uint32_t** current_bc_size,
                                   uint32_t** current_bc_cap, WordDefEntry* word_dict,
                                   int word_count, const char** error_pos)
{
  // Check for nested :
  if (*in_definition)
  {
    if (error_pos)
      *error_pos = *p;
    return FrontErr::NestedColon;
  }

  // Read next token as word name
  while (**p && isspace((unsigned char)**p))
    (*p)++;
  if (!**p)
  {
    if (error_pos)
      *error_pos = *p;
    return FrontErr::ColonWithoutName;
  }

  const char* name_start = *p;
  while (**p && !isspace((unsigned char)**p))
    (*p)++;
  size_t name_len = *p - name_start;

  if (name_len == 0 || name_len >= MAX_WORD_NAME_LEN)
  {
    if (error_pos)
      *error_pos = name_start;
    return FrontErr::ColonWithoutName;
  }

  memcpy(current_word_name, name_start, name_len);
  current_word_name[name_len] = '\0';

  // Check for duplicate word names
  for (int i = 0; i < word_count; i++)
  {
    if (str_eq_ci(word_dict[i].name, current_word_name))
    {
      if (error_pos)
        *error_pos = name_start;
      return FrontErr::DuplicateWord;
    }
  }

  // Check dictionary full
  if (word_count >= MAX_WORDS)
  {
    if (error_pos)
      *error_pos = name_start;
    return FrontErr::DictionaryFull;
  }

  // Enter definition mode
  *in_definition = true;
  *word_bc = nullptr;
  *word_bc_size = 0;
  *word_bc_cap = 0;

  // Switch to word bytecode buffer
  *current_bc = word_bc;
  *current_bc_size = word_bc_size;
  *current_bc_cap = word_bc_cap;

  return FrontErr::OK;
}

// Helper function to handle ; (semicolon) - end word definition
static FrontErr handle_semicolon_end(bool* in_definition, char* current_word_name,
                                     uint8_t** word_bc, uint32_t* word_bc_size,
                                     uint32_t* word_bc_cap, uint8_t*** current_bc,
                                     uint32_t** current_bc_size,
                                     uint32_t** current_bc_cap, uint8_t** bc_main,
                                     uint32_t* bc_main_size, uint32_t* bc_main_cap,
                                     WordDefEntry* word_dict, int* word_count,
                                     const char** error_pos, const char* token_pos)
{
  // Check if in definition mode
  if (!*in_definition)
  {
    if (error_pos)
      *error_pos = token_pos;
    return FrontErr::SemicolonWithoutColon;
  }

  // Append RET to word bytecode
  FrontErr err;
  if ((err = append_byte(*current_bc, *current_bc_size, *current_bc_cap,
                         static_cast<uint8_t>(v4::Op::RET))) != FrontErr::OK)
    return err;

  // Add word to dictionary
  size_t name_len = strlen(current_word_name);
  if (name_len >= MAX_WORD_NAME_LEN)
    name_len = MAX_WORD_NAME_LEN - 1;
  memcpy(word_dict[*word_count].name, current_word_name, name_len);
  word_dict[*word_count].name[name_len] = '\0';
  word_dict[*word_count].code = *word_bc;
  word_dict[*word_count].code_len = *word_bc_size;
  (*word_count)++;

  // Exit definition mode and switch back to main bytecode buffer
  *in_definition = false;
  current_word_name[0] = '\0';
  *word_bc = nullptr;  // Don't free - it's now owned by word_dict
  *word_bc_size = 0;
  *word_bc_cap = 0;

  // Switch back to main bytecode buffer
  *current_bc = bc_main;
  *current_bc_size = bc_main_size;
  *current_bc_cap = bc_main_cap;

  return FrontErr::OK;
}

static FrontErr compile_internal(const char* source, V4FrontBuf* out_buf,
                                 V4FrontContext* ctx, V4FrontError* error_out,
                                 const char** error_pos)
{
  assert(out_buf);

  // Silence unused parameter warning (error_out is used by caller to fill error info)
  (void)error_out;

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

  // Initialize code pointers to nullptr for safe cleanup
  for (int i = 0; i < MAX_WORDS; i++)
    word_dict[i].code = nullptr;

  // Compilation mode state
  bool in_definition = false;                       // Are we inside a : ... ; definition?
  char current_word_name[MAX_WORD_NAME_LEN] = {0};  // Name of word being defined
  uint8_t* word_bc = nullptr;                       // Bytecode buffer for current word
  uint32_t word_bc_size = 0;                        // Size of current word bytecode
  uint32_t word_bc_cap = 0;                         // Capacity of current word bytecode

  // Pointers to current bytecode buffer (updated when switching modes)
  uint8_t** current_bc = &bc;
  uint32_t* current_bc_size = &bc_size;
  uint32_t* current_bc_cap = &bc_cap;

// Helper macro for cleanup on error
#define CLEANUP_AND_RETURN(error_code)                         \
  do                                                           \
  {                                                            \
    cleanup_compile_state(bc, word_bc, word_dict, word_count); \
    return (error_code);                                       \
  } while (0)

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
  char token[MAX_TOKEN_LEN];

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
      if ((err = handle_colon_start(&p, &in_definition, current_word_name, &word_bc,
                                    &word_bc_size, &word_bc_cap, &current_bc,
                                    &current_bc_size, &current_bc_cap, word_dict,
                                    word_count, error_pos)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, ";"))
    {
      // ; (semicolon) - end word definition
      if ((err = handle_semicolon_end(
               &in_definition, current_word_name, &word_bc, &word_bc_size, &word_bc_cap,
               &current_bc, &current_bc_size, &current_bc_cap, &bc, &bc_size, &bc_cap,
               word_dict, &word_count, error_pos, token_start)) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }

    // Check for control flow keywords
    if (str_eq_ci(token, "BEGIN"))
    {
      // BEGIN: mark the current position for backward jump
      if (control_depth >= MAX_CONTROL_DEPTH)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);
      }

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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);
      }

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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::UntilWithoutBegin);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::UntilWithoutBegin);
      }
      if (frame->has_while)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::UntilAfterWhile);
      }

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset: target - (current + 2)
      uint32_t jz_next_ip = *current_bc_size + 2;
      int16_t offset = (int16_t)(frame->begin_addr - jz_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, offset)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Pop control frame
      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "WHILE"))
    {
      // WHILE: emit JZ with placeholder offset (forward jump to after REPEAT)
      if (control_depth <= 0)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::WhileWithoutBegin);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::WhileWithoutBegin);
      }
      if (frame->has_while)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::DuplicateWhile);
      }

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutBegin);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutBegin);
      }
      if (!frame->has_while)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::RepeatWithoutWhile);
      }

      // Emit JMP opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset to BEGIN
      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t jmp_offset = (int16_t)(frame->begin_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap,
                               jmp_offset)) != FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::AgainWithoutBegin);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != BEGIN_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::AgainWithoutBegin);
      }
      if (frame->has_while)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::AgainAfterWhile);
      }

      // Emit JMP opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Calculate backward offset: target - (current + 2)
      uint32_t jmp_next_ip = *current_bc_size + 2;
      int16_t offset = (int16_t)(frame->begin_addr - jmp_next_ip);

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, offset)) !=
          FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::LeaveWithoutDo);
      }

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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::LeaveWithoutDo);
      }

      ControlFrame* frame = &control_stack[do_frame_idx];

      // Check if we have space for another LEAVE
      if (frame->leave_count >= MAX_LEAVE_DEPTH)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::LeaveDepthExceeded);
      }

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
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::LoopWithoutDo);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != DO_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::LoopWithoutDo);
      }

      // R>: pop index from return stack
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // LIT 1
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_i32_le(current_bc, current_bc_size, current_bc_cap, 1)) !=
          FrontErr::OK)
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
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap,
                               jmp_offset)) != FrontErr::OK)
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
        int16_t leave_offset =
            (int16_t)(*current_bc_size - (frame->leave_patch_addrs[i] + 2));
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::PLoopWithoutDo);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != DO_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::PLoopWithoutDo);
      }

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
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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

      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap,
                               jmp_offset)) != FrontErr::OK)
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
        int16_t leave_offset =
            (int16_t)(*current_bc_size - (frame->leave_patch_addrs[i] + 2));
        backpatch_i16_le(*current_bc, frame->leave_patch_addrs[i], leave_offset);
      }

      control_depth--;
      continue;
    }
    else if (str_eq_ci(token, "IF"))
    {
      // IF: emit JZ with placeholder offset, push to control stack
      if (control_depth >= MAX_CONTROL_DEPTH)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ControlDepthExceeded);
      }

      // Emit JZ opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Save position for backpatching and emit placeholder
      uint32_t patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ElseWithoutIf);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ElseWithoutIf);
      }
      if (frame->has_else)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::DuplicateElse);
      }

      // Emit JMP with placeholder (to skip ELSE clause)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      uint32_t jmp_patch_pos = *current_bc_size;
      if ((err = append_i16_le(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
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
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ThenWithoutIf);
      }

      ControlFrame* frame = &control_stack[control_depth - 1];
      if (frame->type != IF_CONTROL)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::ThenWithoutIf);
      }

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
    else if (str_eq_ci(token, "SYS"))
    {
      // SYS: system call with 8-bit ID
      // Skip whitespace and get next token (the SYS ID)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after SYS
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingSysId);
      }

      // Extract ID token
      const char* id_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t id_token_len = p - id_token_start;
      if (id_token_len >= sizeof(token))
        id_token_len = sizeof(token) - 1;

      char id_token[MAX_TOKEN_LEN];
      memcpy(id_token, id_token_start, id_token_len);
      id_token[id_token_len] = '\0';

      // Parse ID as integer
      int32_t sys_id;
      if (!try_parse_int(id_token, &sys_id) || sys_id < 0 || sys_id > 255)
      {
        if (error_pos)
          *error_pos = id_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidSysId);
      }

      // Emit: [SYS] [id8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SYS))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(sys_id))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "EMIT"))
    {
      // EMIT: output one character ( c -- )
      // Emits: SYS 0x30
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SYS))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0x30)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "KEY"))
    {
      // KEY: input one character ( -- c )
      // Emits: SYS 0x31
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SYS))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0x31)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "L++"))
    {
      // L++: increment local variable (LINC)
      // Skip whitespace and get next token (the local variable index)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after L++
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingLocalIdx);
      }

      // Extract index token
      const char* idx_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t idx_token_len = p - idx_token_start;
      if (idx_token_len >= sizeof(token))
        idx_token_len = sizeof(token) - 1;

      char idx_token[MAX_TOKEN_LEN];
      memcpy(idx_token, idx_token_start, idx_token_len);
      idx_token[idx_token_len] = '\0';

      // Parse index as integer
      int32_t local_idx;
      if (!try_parse_int(idx_token, &local_idx) || local_idx < 0 || local_idx > 255)
      {
        if (error_pos)
          *error_pos = idx_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidLocalIdx);
      }

      // Emit: [LINC] [idx8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LINC))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(local_idx))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "L--"))
    {
      // L--: decrement local variable (LDEC)
      // Skip whitespace and get next token (the local variable index)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after L--
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingLocalIdx);
      }

      // Extract index token
      const char* idx_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t idx_token_len = p - idx_token_start;
      if (idx_token_len >= sizeof(token))
        idx_token_len = sizeof(token) - 1;

      char idx_token[MAX_TOKEN_LEN];
      memcpy(idx_token, idx_token_start, idx_token_len);
      idx_token[idx_token_len] = '\0';

      // Parse index as integer
      int32_t local_idx;
      if (!try_parse_int(idx_token, &local_idx) || local_idx < 0 || local_idx > 255)
      {
        if (error_pos)
          *error_pos = idx_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidLocalIdx);
      }

      // Emit: [LDEC] [idx8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LDEC))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(local_idx))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "L@"))
    {
      // L@: get local variable (LGET)
      // Skip whitespace and get next token (the local variable index)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after L@
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingLocalIdx);
      }

      // Extract index token
      const char* idx_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t idx_token_len = p - idx_token_start;
      if (idx_token_len >= sizeof(token))
        idx_token_len = sizeof(token) - 1;

      char idx_token[MAX_TOKEN_LEN];
      memcpy(idx_token, idx_token_start, idx_token_len);
      idx_token[idx_token_len] = '\0';

      // Parse index as integer
      int32_t local_idx;
      if (!try_parse_int(idx_token, &local_idx) || local_idx < 0 || local_idx > 255)
      {
        if (error_pos)
          *error_pos = idx_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidLocalIdx);
      }

      // Emit: [LGET] [idx8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LGET))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(local_idx))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "L!"))
    {
      // L!: set local variable (LSET)
      // Skip whitespace and get next token (the local variable index)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after L!
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingLocalIdx);
      }

      // Extract index token
      const char* idx_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t idx_token_len = p - idx_token_start;
      if (idx_token_len >= sizeof(token))
        idx_token_len = sizeof(token) - 1;

      char idx_token[MAX_TOKEN_LEN];
      memcpy(idx_token, idx_token_start, idx_token_len);
      idx_token[idx_token_len] = '\0';

      // Parse index as integer
      int32_t local_idx;
      if (!try_parse_int(idx_token, &local_idx) || local_idx < 0 || local_idx > 255)
      {
        if (error_pos)
          *error_pos = idx_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidLocalIdx);
      }

      // Emit: [LSET] [idx8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LSET))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(local_idx))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "L>!"))
    {
      // L>!: tee local variable (LTEE) - store and keep value on stack
      // Skip whitespace and get next token (the local variable index)
      while (*p && isspace((unsigned char)*p))
        p++;

      if (!*p)
      {
        // No token after L>!
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::MissingLocalIdx);
      }

      // Extract index token
      const char* idx_token_start = p;
      while (*p && !isspace((unsigned char)*p))
        p++;
      size_t idx_token_len = p - idx_token_start;
      if (idx_token_len >= sizeof(token))
        idx_token_len = sizeof(token) - 1;

      char idx_token[MAX_TOKEN_LEN];
      memcpy(idx_token, idx_token_start, idx_token_len);
      idx_token[idx_token_len] = '\0';

      // Parse index as integer
      int32_t local_idx;
      if (!try_parse_int(idx_token, &local_idx) || local_idx < 0 || local_idx > 255)
      {
        if (error_pos)
          *error_pos = idx_token_start;
        CLEANUP_AND_RETURN(FrontErr::InvalidLocalIdx);
      }

      // Emit: [LTEE] [idx8]
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LTEE))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(local_idx))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }
    else if (str_eq_ci(token, "RECURSE"))
    {
      // RECURSE: call the currently-being-defined word recursively
      if (!in_definition)
      {
        if (error_pos)
          *error_pos = token_start;
        CLEANUP_AND_RETURN(FrontErr::RecurseOutsideWord);
      }

      // Emit CALL to the current word (which will be at index word_count)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::CALL))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      // Emit the index as 16-bit little-endian
      int16_t word_idx = static_cast<int16_t>(word_count);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(word_idx & 0xFF))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>((word_idx >> 8) & 0xFF))) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);

      continue;
    }

    // Try looking up word in dictionary
    {
      int word_idx = -1;

      // First, search in local word_dict (words defined in this compilation)
      for (int i = 0; i < word_count; i++)
      {
        if (str_eq_ci(token, word_dict[i].name))
        {
          word_idx = i;
          break;
        }
      }

      // If not found locally, search in context (words from previous compilations)
      if (word_idx < 0 && ctx)
      {
        int vm_idx = v4front_context_find_word(ctx, token);
        if (vm_idx >= 0)
        {
          word_idx = vm_idx;
        }
      }

      if (word_idx >= 0)
      {
        // Found a word - emit CALL instruction
        if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                               static_cast<uint8_t>(v4::Op::CALL))) != FrontErr::OK)
          CLEANUP_AND_RETURN(err);

        // Emit 16-bit word index (little-endian)
        // If found in context, use VM word index directly
        // If found locally, use local index (will be adjusted when registered to VM)
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
      if ((err = append_i32_le(current_bc, current_bc_size, current_bc_cap, val)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }

    // Special handling for J and K (multi-instruction sequences)
    if (str_eq_ci(token, "J"))
    {
      if ((err = emit_j_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "K"))
    {
      if ((err = emit_k_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    // Composite words (expanded to multiple opcodes)
    else if (str_eq_ci(token, "ROT"))
    {
      // ROT ( a b c -- b c a ): >R SWAP R> SWAP
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "NIP"))
    {
      // NIP ( a b -- b ): SWAP DROP
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "TUCK"))
    {
      // TUCK ( a b -- b a b ): SWAP OVER
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "NEGATE"))
    {
      // NEGATE ( n -- -n ): 0 SWAP -
      // Emit: LIT 0, SWAP, SUB
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SUB))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "?DUP"))
    {
      // ?DUP ( x -- 0 | x x ): if x is zero, leave it; if non-zero, duplicate
      // Bytecode: DUP, DUP, JZ +1 (skip next), DUP
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JZ offset +1 (skip the next DUP instruction, which is 1 byte)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Offset: +1 byte (skip DUP), little-endian int16
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 1)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // The DUP that gets executed if non-zero
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "ABS"))
    {
      // ABS ( n -- |n| ): if n < 0, negate it
      // Bytecode: DUP, LIT0, LT, JZ skip_negate, LIT0, SWAP, SUB, skip_negate:
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JZ offset +3 (skip LIT0, SWAP, SUB)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 3)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // NEGATE sequence: LIT0, SWAP, SUB
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SUB))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "MIN"))
    {
      // MIN ( a b -- min ): OVER OVER < IF DROP ELSE SWAP DROP THEN
      // Bytecode: OVER, OVER, LT, JZ else_branch, DROP, JMP end, else_branch: SWAP, DROP,
      // end:
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JZ offset to else_branch (+4 bytes: DROP + JMP 3)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 4)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // True branch: DROP (keep first)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JMP to end (+2 bytes: SWAP, DROP)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 2)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Else branch: SWAP, DROP (keep second)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "MAX"))
    {
      // MAX ( a b -- max ): OVER OVER > IF DROP ELSE SWAP DROP THEN
      // Same as MIN but with GT instead of LT
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::GT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JZ offset to else_branch (+4 bytes: DROP + JMP 3)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JZ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 4)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // True branch: DROP (keep first)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // JMP to end (+2 bytes: SWAP, DROP)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::JMP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 2)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap, 0)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Else branch: SWAP, DROP (keep second)
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::SWAP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "0="))
    {
      // 0= ( n -- flag ): test if zero
      // Bytecode: LIT0, EQ
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::EQ))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "0<"))
    {
      // 0< ( n -- flag ): test if less than zero (negative)
      // Bytecode: LIT0, LT
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "0>"))
    {
      // 0> ( n -- flag ): test if greater than zero (positive)
      // Bytecode: LIT0, GT
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::GT))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "2DUP"))
    {
      // 2DUP ( a b -- a b a b ): duplicate top two items
      // Bytecode: OVER, OVER
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "2DROP"))
    {
      // 2DROP ( a b -- ): drop top two items
      // Bytecode: DROP, DROP
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DROP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "2SWAP"))
    {
      // 2SWAP ( a b c d -- c d a b ): swap top two pairs
      // Bytecode: ROT >R ROT R>
      if ((err = emit_rot_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = emit_rot_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "2OVER"))
    {
      // 2OVER ( a b c d -- a b c d a b ): copy second pair to top
      // Bytecode: >R >R OVER OVER R> R> 2SWAP
      // First, save top pair
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Duplicate the now-top pair
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::OVER))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Restore saved pair
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      // Now we have: a b a b c d, need: a b c d a b
      // Use 2SWAP: ROT >R ROT R>
      if ((err = emit_rot_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = emit_rot_instruction(current_bc, current_bc_size, current_bc_cap)) !=
          FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "+!"))
    {
      // +! ( n addr -- ): add n to value at addr
      // Bytecode: DUP >R @ + R> !
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::DUP))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::TOR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LOAD))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::ADD))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::FROMR))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::STORE))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "TRUE"))
    {
      // TRUE ( -- -1 ): true flag value
      // Bytecode: LITN1
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LITN1))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }
    else if (str_eq_ci(token, "FALSE"))
    {
      // FALSE ( -- 0 ): false flag value
      // Bytecode: LIT0
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(v4::Op::LIT0))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }

    // Try matching simple opcodes using dispatch table
    v4::Op opcode;
    if (lookup_simple_opcode(token, &opcode))
    {
      // Found in dispatch table - emit the opcode
      if ((err = append_byte(current_bc, current_bc_size, current_bc_cap,
                             static_cast<uint8_t>(opcode))) != FrontErr::OK)
        CLEANUP_AND_RETURN(err);
      continue;
    }

    // Token not recognized
    if (error_pos)
      *error_pos = token_start;
    CLEANUP_AND_RETURN(FrontErr::UnknownToken);
  }

  // Check for unclosed control structures
  if (control_depth > 0)
  {
    // Set error position to end of source (we don't know where the structure started)
    if (error_pos)
      *error_pos = p;

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
  {
    // Set error position to end of source
    if (error_pos)
      *error_pos = p;
    CLEANUP_AND_RETURN(FrontErr::UnclosedColon);
  }

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
      out_buf->words[i].name = portable_strdup(word_dict[i].name);
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
      word_dict[i].code = nullptr;  // Ownership transferred, don't free in cleanup
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

  const char* error_pos = nullptr;
  FrontErr result = compile_internal(source, out_buf, nullptr, nullptr, &error_pos);

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

// ===========================================================================
// Stateful Compiler Context Implementation
// ===========================================================================

extern "C" V4FrontContext* v4front_context_create(void)
{
  V4FrontContext* ctx = (V4FrontContext*)malloc(sizeof(V4FrontContext));
  if (!ctx)
    return nullptr;

  ctx->words = nullptr;
  ctx->word_count = 0;
  ctx->word_capacity = 0;

  return ctx;
}

extern "C" void v4front_context_destroy(V4FrontContext* ctx)
{
  if (!ctx)
    return;

  // Free all word names
  for (int i = 0; i < ctx->word_count; i++)
  {
    free(ctx->words[i].name);
  }

  // Free words array
  free(ctx->words);

  // Free context
  free(ctx);
}

extern "C" void v4front_context_reset(V4FrontContext* ctx)
{
  if (!ctx)
    return;

  // Free all word names
  for (int i = 0; i < ctx->word_count; i++)
  {
    free(ctx->words[i].name);
  }

  // Clear word list
  ctx->word_count = 0;
}

extern "C" v4front_err v4front_context_register_word(V4FrontContext* ctx,
                                                     const char* name, int vm_word_idx)
{
  if (!ctx || !name)
    return -1;  // Invalid argument

  // Check if word already exists (case-insensitive)
  for (int i = 0; i < ctx->word_count; i++)
  {
    if (str_eq_ci(ctx->words[i].name, name))
    {
      // Update existing entry
      ctx->words[i].vm_word_idx = vm_word_idx;
      return front_err_to_int(FrontErr::OK);
    }
  }

  // Grow array if needed
  if (ctx->word_count >= ctx->word_capacity)
  {
    int new_capacity = (ctx->word_capacity == 0) ? 16 : (ctx->word_capacity * 2);
    ContextWordEntry* new_words =
        (ContextWordEntry*)realloc(ctx->words, sizeof(ContextWordEntry) * new_capacity);
    if (!new_words)
      return front_err_to_int(FrontErr::OutOfMemory);

    ctx->words = new_words;
    ctx->word_capacity = new_capacity;
  }

  // Add new entry
  ctx->words[ctx->word_count].name = portable_strdup(name);
  if (!ctx->words[ctx->word_count].name)
    return front_err_to_int(FrontErr::OutOfMemory);

  ctx->words[ctx->word_count].vm_word_idx = vm_word_idx;
  ctx->word_count++;

  return front_err_to_int(FrontErr::OK);
}

extern "C" int v4front_context_get_word_count(const V4FrontContext* ctx)
{
  if (!ctx)
    return 0;
  return ctx->word_count;
}

extern "C" const char* v4front_context_get_word_name(const V4FrontContext* ctx, int idx)
{
  if (!ctx || idx < 0 || idx >= ctx->word_count)
    return nullptr;
  return ctx->words[idx].name;
}

extern "C" int v4front_context_find_word(const V4FrontContext* ctx, const char* name)
{
  if (!ctx || !name)
    return -1;

  for (int i = 0; i < ctx->word_count; i++)
  {
    if (str_eq_ci(ctx->words[i].name, name))
      return ctx->words[i].vm_word_idx;
  }

  return -1;
}

extern "C" v4front_err v4front_compile_with_context(V4FrontContext* ctx,
                                                    const char* source,
                                                    V4FrontBuf* out_buf, char* err,
                                                    size_t err_cap)
{
  if (!out_buf)
  {
    write_error_msg(err, err_cap, "output buffer is NULL");
    return front_err_to_int(FrontErr::BufferTooSmall);
  }

  // Compile with context (may be NULL for stateless compilation)
  const char* error_pos = nullptr;
  FrontErr result = compile_internal(source, out_buf, ctx, nullptr, &error_pos);

  if (result != FrontErr::OK)
  {
    write_error(err, err_cap, result);
  }

  return front_err_to_int(result);
}

// ===========================================================================
// Error Position Tracking and Detailed Error Information
// ===========================================================================

// Helper: Calculate line and column from position in source
static void calculate_line_column(const char* source, const char* error_pos, int* line,
                                  int* column)
{
  if (!source || !error_pos || error_pos < source)
  {
    *line = -1;
    *column = -1;
    return;
  }

  *line = 1;
  *column = 1;

  for (const char* p = source; p < error_pos; p++)
  {
    if (*p == '\n')
    {
      (*line)++;
      *column = 1;
    }
    else
    {
      (*column)++;
    }
  }
}

// Helper: Extract token at error position
static void extract_error_token(const char* source, const char* error_pos,
                                char* token_out, size_t token_cap)
{
  if (!source || !error_pos || !token_out || token_cap == 0 || error_pos < source)
  {
    if (token_out && token_cap > 0)
      token_out[0] = '\0';
    return;
  }

  // Find start of token (skip back over non-whitespace)
  const char* token_start = error_pos;
  while (token_start > source && !isspace((unsigned char)*(token_start - 1)))
    token_start--;

  // Find end of token
  const char* token_end = error_pos;
  while (*token_end && !isspace((unsigned char)*token_end))
    token_end++;

  // Copy token
  size_t token_len = token_end - token_start;
  if (token_len >= token_cap)
    token_len = token_cap - 1;

  memcpy(token_out, token_start, token_len);
  token_out[token_len] = '\0';
}

// Helper: Extract surrounding context
static void extract_context(const char* source, const char* error_pos, char* context_out,
                            size_t context_cap)
{
  if (!source || !error_pos || !context_out || context_cap == 0 || error_pos < source)
  {
    if (context_out && context_cap > 0)
      context_out[0] = '\0';
    return;
  }

  // Find start of line
  const char* line_start = error_pos;
  while (line_start > source && *(line_start - 1) != '\n')
    line_start--;

  // Find end of line
  const char* line_end = error_pos;
  while (*line_end && *line_end != '\n')
    line_end++;

  // Copy context (trimmed to fit)
  size_t line_len = line_end - line_start;
  if (line_len >= context_cap)
    line_len = context_cap - 1;

  memcpy(context_out, line_start, line_len);
  context_out[line_len] = '\0';
}

// Helper: Fill V4FrontError structure
static void fill_error_info(V4FrontError* error, const char* source,
                            const char* error_pos, FrontErr code)
{
  if (!error)
    return;

  error->code = front_err_to_int(code);

  // Copy error message
  const char* msg = front_err_str(code);
  size_t msg_len = strlen(msg);
  if (msg_len >= sizeof(error->message))
    msg_len = sizeof(error->message) - 1;
  memcpy(error->message, msg, msg_len);
  error->message[msg_len] = '\0';

  // Calculate position
  if (source && error_pos && error_pos >= source)
  {
    error->position = (int)(error_pos - source);
    calculate_line_column(source, error_pos, &error->line, &error->column);
    extract_error_token(source, error_pos, error->token, sizeof(error->token));
    extract_context(source, error_pos, error->context, sizeof(error->context));
  }
  else
  {
    error->position = -1;
    error->line = -1;
    error->column = -1;
    error->token[0] = '\0';
    error->context[0] = '\0';
  }
}

extern "C" v4front_err v4front_compile_ex(const char* source, V4FrontBuf* out_buf,
                                          V4FrontError* error_out)
{
  if (!out_buf)
  {
    if (error_out)
    {
      error_out->code = front_err_to_int(FrontErr::BufferTooSmall);
      snprintf(error_out->message, sizeof(error_out->message), "output buffer is NULL");
      error_out->position = -1;
      error_out->line = -1;
      error_out->column = -1;
      error_out->token[0] = '\0';
      error_out->context[0] = '\0';
    }
    return front_err_to_int(FrontErr::BufferTooSmall);
  }

  const char* error_pos = nullptr;
  FrontErr result = compile_internal(source, out_buf, nullptr, error_out, &error_pos);

  if (result != FrontErr::OK && error_out)
  {
    fill_error_info(error_out, source, error_pos, result);
  }

  return front_err_to_int(result);
}

extern "C" v4front_err v4front_compile_with_context_ex(V4FrontContext* ctx,
                                                       const char* source,
                                                       V4FrontBuf* out_buf,
                                                       V4FrontError* error_out)
{
  if (!out_buf)
  {
    if (error_out)
    {
      error_out->code = front_err_to_int(FrontErr::BufferTooSmall);
      snprintf(error_out->message, sizeof(error_out->message), "output buffer is NULL");
      error_out->position = -1;
      error_out->line = -1;
      error_out->column = -1;
      error_out->token[0] = '\0';
      error_out->context[0] = '\0';
    }
    return front_err_to_int(FrontErr::BufferTooSmall);
  }

  const char* error_pos = nullptr;
  FrontErr result = compile_internal(source, out_buf, ctx, error_out, &error_pos);

  if (result != FrontErr::OK && error_out)
  {
    fill_error_info(error_out, source, error_pos, result);
  }

  return front_err_to_int(result);
}

extern "C" void v4front_format_error(const V4FrontError* error, const char* source,
                                     char* out_buf, size_t out_cap)
{
  if (!error || !out_buf || out_cap == 0)
    return;

  char* p = out_buf;
  size_t remaining = out_cap;

  // Format error message with position
  int written;
  if (error->line > 0 && error->column > 0)
  {
    written = snprintf(p, remaining, "Error: %s at line %d, column %d\n", error->message,
                       error->line, error->column);
  }
  else
  {
    written = snprintf(p, remaining, "Error: %s\n", error->message);
  }

  if (written < 0 || (size_t)written >= remaining)
    return;

  p += written;
  remaining -= written;

  // Show context line if available
  if (error->context[0] != '\0' && source)
  {
    written = snprintf(p, remaining, "  %s\n", error->context);
    if (written < 0 || (size_t)written >= remaining)
      return;

    p += written;
    remaining -= written;

    // Add position indicator (^) under the error
    if (error->column > 0)
    {
      written = snprintf(p, remaining, "  ");
      if (written < 0 || (size_t)written >= remaining)
        return;

      p += written;
      remaining -= written;

      // Add spaces up to error column
      for (int i = 1; i < error->column && remaining > 1; i++)
      {
        *p++ = ' ';
        remaining--;
      }

      // Add caret
      if (remaining > 1)
      {
        *p++ = '^';
        remaining--;
      }

      // Add tildes under token if available
      size_t token_len = strlen(error->token);
      for (size_t i = 1; i < token_len && remaining > 1; i++)
      {
        *p++ = '~';
        remaining--;
      }

      if (remaining > 1)
      {
        *p++ = '\n';
        remaining--;
      }

      *p = '\0';
    }
  }

  // Null-terminate
  if (out_cap > 0)
    out_buf[out_cap - 1] = '\0';
}