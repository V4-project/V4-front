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
    char* name;        // Word name (dynamically allocated)
    uint8_t* code;     // Bytecode (dynamically allocated)
    uint32_t code_len; // Length of bytecode
  } V4FrontWord;

  // ---------------------------------------------------------------------------
  // V4FrontBuf
  //  - Holds dynamically allocated bytecode output.
  //  - Can contain multiple word definitions and main code.
  //  - The caller must call v4front_free() when done.
  // ---------------------------------------------------------------------------
  typedef struct
  {
    V4FrontWord* words; // Array of compiled words (NULL if no words defined)
    int word_count;     // Number of words in array
    uint8_t* data;      // Main bytecode (may be NULL if only words defined)
    size_t size;        // Size of main bytecode
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

#ifdef __cplusplus
}  // extern "C"
#endif