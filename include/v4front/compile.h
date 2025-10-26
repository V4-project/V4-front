#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>

  // ---------------------------------------------------------------------------
  // V4FrontBuf
  //  - Holds dynamically allocated bytecode output.
  //  - The caller must call v4front_free() when done.
  // ---------------------------------------------------------------------------
  typedef struct
  {
    uint8_t* data;
    size_t size;
  } V4FrontBuf;

  // ---------------------------------------------------------------------------
  // v4front_err
  //  - Error code type (int).
  //  - 0 = success, negative = error.
  // ---------------------------------------------------------------------------
  typedef int v4front_err;

  // Error code constants
  enum
  {
    V4FRONT_OK = 0,
    V4FRONT_UNKNOWN_TOKEN = -1,
    V4FRONT_INVALID_INTEGER = -2,
    V4FRONT_OUT_OF_MEMORY = -3,
    V4FRONT_BUFFER_TOO_SMALL = -4,
    V4FRONT_EMPTY_INPUT = -5,
    V4FRONT_CONTROL_DEPTH_EXCEEDED = -6,
    V4FRONT_ELSE_WITHOUT_IF = -7,
    V4FRONT_DUPLICATE_ELSE = -8,
    V4FRONT_THEN_WITHOUT_IF = -9,
    V4FRONT_UNCLOSED_IF = -10,
    V4FRONT_UNTIL_WITHOUT_BEGIN = -11,
    V4FRONT_UNCLOSED_BEGIN = -12,
    V4FRONT_WHILE_WITHOUT_BEGIN = -13,
    V4FRONT_DUPLICATE_WHILE = -14,
    V4FRONT_REPEAT_WITHOUT_BEGIN = -15,
    V4FRONT_REPEAT_WITHOUT_WHILE = -16,
    V4FRONT_UNTIL_AFTER_WHILE = -17,
  };

  // ---------------------------------------------------------------------------
  // v4front_err_str
  //  - Returns a string message for the given error code.
  // ---------------------------------------------------------------------------
  const char* v4front_err_str(v4front_err code);

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