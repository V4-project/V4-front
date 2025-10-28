#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

// Include unified error definitions
#include "v4front/errors.h"

  // ---------------------------------------------------------------------------
  // V4FrontWord
  //  - Represents a single compiled word definition.
  // ---------------------------------------------------------------------------
  typedef struct
  {
    char* name;         // Word name (dynamically allocated)
    uint8_t* code;      // Bytecode (dynamically allocated)
    uint32_t code_len;  // Length of bytecode
  } V4FrontWord;

  // ---------------------------------------------------------------------------
  // V4FrontBuf
  //  - Holds dynamically allocated bytecode output.
  //  - Can contain multiple word definitions and main code.
  //  - The caller must call v4front_free() when done.
  // ---------------------------------------------------------------------------
  typedef struct
  {
    V4FrontWord* words;  // Array of compiled words (NULL if no words defined)
    int word_count;      // Number of words in array
    uint8_t* data;       // Main bytecode (may be NULL if only words defined)
    size_t size;         // Size of main bytecode
  } V4FrontBuf;

  // ---------------------------------------------------------------------------
  // v4front_err
  //  - Error code type (int).
  //  - 0 = success, negative = error.
  //  - Now aliased to v4front_err_t for compatibility
  // ---------------------------------------------------------------------------
  typedef v4front_err_t v4front_err;

  // ---------------------------------------------------------------------------
  // v4front_compile
  //  - Compiles a string of space-separated tokens into V4 bytecode.
  //  - Returns 0 on success, negative on error.
  //  - On success, out_buf->data points to allocated memory with bytecode.
  //  - On error, writes a message to err (if err != NULL).
  // ---------------------------------------------------------------------------
  v4front_err v4front_compile(const char* source, V4FrontBuf* out_buf, char* err,
                              size_t err_cap);

  // ---------------------------------------------------------------------------
  // v4front_compile_word
  //  - Same as v4front_compile, but carries a word name for future extensions.
  //  - Current minimal implementation ignores `name` and behaves like
  //    compile().
  // ---------------------------------------------------------------------------
  v4front_err v4front_compile_word(const char* name, const char* source,
                                   V4FrontBuf* out_buf, char* err, size_t err_cap);

  // ---------------------------------------------------------------------------
  // v4front_free
  //  - Frees a buffer previously returned by v4front_compile(_word).
  //  - Safe to call with NULL or with an already-freed buffer (no-op).
  // ---------------------------------------------------------------------------
  void v4front_free(V4FrontBuf* buf);

  // ===========================================================================
  // Stateful Compiler Context (for REPL support)
  // ===========================================================================

  // ---------------------------------------------------------------------------
  // V4FrontContext
  //  - Opaque context for stateful compilation.
  //  - Tracks previously defined words to enable incremental compilation.
  //  - Allows REPL to remember word definitions across multiple lines.
  // ---------------------------------------------------------------------------
  typedef struct V4FrontContext V4FrontContext;

  // ---------------------------------------------------------------------------
  // v4front_context_create
  //  - Creates a new compiler context.
  //  - Returns NULL on allocation failure.
  // ---------------------------------------------------------------------------
  V4FrontContext* v4front_context_create(void);

  // ---------------------------------------------------------------------------
  // v4front_context_destroy
  //  - Destroys a compiler context and frees all associated resources.
  //  - Safe to call with NULL (no-op).
  // ---------------------------------------------------------------------------
  void v4front_context_destroy(V4FrontContext* ctx);

  // ---------------------------------------------------------------------------
  // v4front_context_register_word
  //  - Registers a word definition with the compiler context.
  //  - This synchronizes the compiler with the VM's word dictionary.
  //  - After registration, the word can be referenced in subsequent compilations.
  //
  //  @param ctx         Compiler context
  //  @param name        Word name (must not be NULL, will be copied)
  //  @param vm_word_idx VM word index (from vm_register_word)
  //  @return 0 on success, negative on error
  // ---------------------------------------------------------------------------
  v4front_err v4front_context_register_word(V4FrontContext* ctx, const char* name,
                                            int vm_word_idx);

  // ---------------------------------------------------------------------------
  // v4front_compile_with_context
  //  - Compiles source code using a compiler context.
  //  - Can reference previously registered words.
  //  - Otherwise identical to v4front_compile().
  //
  //  @param ctx     Compiler context (may be NULL for stateless compilation)
  //  @param source  Source code to compile
  //  @param out_buf Output buffer
  //  @param err     Error message buffer (may be NULL)
  //  @param err_cap Error buffer capacity
  //  @return 0 on success, negative on error
  // ---------------------------------------------------------------------------
  v4front_err v4front_compile_with_context(V4FrontContext* ctx, const char* source,
                                           V4FrontBuf* out_buf, char* err,
                                           size_t err_cap);

  // ---------------------------------------------------------------------------
  // v4front_context_reset
  //  - Clears all registered words from the context.
  //  - Should be called when the VM dictionary is reset.
  // ---------------------------------------------------------------------------
  void v4front_context_reset(V4FrontContext* ctx);

  // ---------------------------------------------------------------------------
  // v4front_context_get_word_count
  //  - Returns the number of words registered in the context.
  // ---------------------------------------------------------------------------
  int v4front_context_get_word_count(const V4FrontContext* ctx);

  // ---------------------------------------------------------------------------
  // v4front_context_get_word_name
  //  - Returns the name of the word at the given index.
  //  - Returns NULL if index is out of range.
  //
  //  @param ctx Compiler context
  //  @param idx Word index (0-based)
  //  @return Word name or NULL
  // ---------------------------------------------------------------------------
  const char* v4front_context_get_word_name(const V4FrontContext* ctx, int idx);

  // ---------------------------------------------------------------------------
  // v4front_context_find_word
  //  - Finds a word by name and returns its VM word index.
  //  - Returns negative value if not found.
  //
  //  @param ctx  Compiler context
  //  @param name Word name to find
  //  @return VM word index or negative if not found
  // ---------------------------------------------------------------------------
  int v4front_context_find_word(const V4FrontContext* ctx, const char* name);

  // ===========================================================================
  // Detailed Error Information (for improved error messages)
  // ===========================================================================

  // ---------------------------------------------------------------------------
  // V4FrontError
  //  - Detailed error information structure.
  //  - Contains error code, message, position, and context.
  //  - Used by v4front_compile_ex() and v4front_compile_with_context_ex().
  // ---------------------------------------------------------------------------
  typedef struct
  {
    v4front_err_t code;  // Error code (same as return value)
    char message[256];   // Human-readable error message
    int position;        // Byte offset in source where error occurred (-1 if unknown)
    int line;            // Line number (1-based, -1 if unknown)
    int column;          // Column number (1-based, -1 if unknown)
    char token[64];      // Token that caused the error (empty if not applicable)
    char context[128];   // Surrounding source context (empty if not applicable)
  } V4FrontError;

  // ---------------------------------------------------------------------------
  // v4front_compile_ex
  //  - Compiles source with detailed error information.
  //  - On error, populates error_out with position and context.
  //  - Otherwise identical to v4front_compile().
  //
  //  @param source    Source code to compile
  //  @param out_buf   Output buffer
  //  @param error_out Error information output (may be NULL)
  //  @return 0 on success, negative on error
  // ---------------------------------------------------------------------------
  v4front_err v4front_compile_ex(const char* source, V4FrontBuf* out_buf,
                                 V4FrontError* error_out);

  // ---------------------------------------------------------------------------
  // v4front_compile_with_context_ex
  //  - Compiles source with context and detailed error information.
  //  - Combines features of v4front_compile_with_context() and v4front_compile_ex().
  //
  //  @param ctx       Compiler context (may be NULL)
  //  @param source    Source code to compile
  //  @param out_buf   Output buffer
  //  @param error_out Error information output (may be NULL)
  //  @return 0 on success, negative on error
  // ---------------------------------------------------------------------------
  v4front_err v4front_compile_with_context_ex(V4FrontContext* ctx, const char* source,
                                              V4FrontBuf* out_buf,
                                              V4FrontError* error_out);

  // ---------------------------------------------------------------------------
  // v4front_format_error
  //  - Formats error information into a human-readable string.
  //  - Includes source context with position indicator (^).
  //
  //  @param error   Error information
  //  @param source  Original source code (for context display)
  //  @param out_buf Output buffer for formatted message
  //  @param out_cap Output buffer capacity
  // ---------------------------------------------------------------------------
  void v4front_format_error(const V4FrontError* error, const char* source, char* out_buf,
                            size_t out_cap);

  // ===========================================================================
  // Bytecode File I/O (.v4b format)
  // ===========================================================================

  // ---------------------------------------------------------------------------
  // V4BytecodeHeader
  //  - File header structure for .v4b bytecode files.
  //  - Contains magic number, version, and metadata.
  // ---------------------------------------------------------------------------
  typedef struct
  {
    uint8_t magic[4];       // Magic number: "V4BC" (0x56 0x34 0x42 0x43)
    uint8_t version_major;  // Major version number (currently 0)
    uint8_t version_minor;  // Minor version number (currently 1)
    uint16_t flags;         // Reserved flags (must be 0)
    uint32_t code_size;     // Size of bytecode in bytes
    uint32_t reserved;      // Reserved for future use (must be 0)
  } V4BytecodeHeader;

  // ---------------------------------------------------------------------------
  // v4front_save_bytecode
  //  - Saves bytecode to a .v4b file.
  //  - Writes V4BytecodeHeader followed by bytecode data.
  //
  //  @param buf      Bytecode buffer to save
  //  @param filename Output file path
  //  @return 0 on success, negative on error
  //    -1: Invalid parameters (NULL buf or filename)
  //    -2: Failed to open file for writing
  //    -3: Failed to write header
  //    -4: Failed to write bytecode
  // ---------------------------------------------------------------------------
  v4front_err v4front_save_bytecode(const V4FrontBuf* buf, const char* filename);

  // ---------------------------------------------------------------------------
  // v4front_load_bytecode
  //  - Loads bytecode from a .v4b file.
  //  - Reads and validates V4BytecodeHeader, then loads bytecode.
  //  - Caller must call v4front_free() on out_buf when done.
  //
  //  @param filename Input file path
  //  @param out_buf  Output buffer (will be populated with bytecode)
  //  @return 0 on success, negative on error
  //    -1: Invalid parameters (NULL filename or out_buf)
  //    -2: Failed to open file for reading
  //    -3: Failed to read header
  //    -4: Invalid magic number (not "V4BC")
  //    -5: Failed to allocate memory for bytecode
  //    -6: Failed to read bytecode
  // ---------------------------------------------------------------------------
  v4front_err v4front_load_bytecode(const char* filename, V4FrontBuf* out_buf);

#ifdef __cplusplus
}  // extern "C"
#endif