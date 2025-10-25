#pragma once
#include <stddef.h>  // size_t
#include <stdint.h>  // uint8_t, uint32_t

#ifdef __cplusplus
extern "C"
{
#endif

  // ---------------------------------------------------------------------------
  // Public buffer type
  //  - Ownership of `data` is transferred to the caller; free with
  //    v4front_free().
  //  - `size` is the number of valid bytes in `data`.
  // ---------------------------------------------------------------------------
  typedef struct V4FrontBuf
  {
    uint8_t* data;
    uint32_t size;
  } V4FrontBuf;

  // ---------------------------------------------------------------------------
  // Error handling model (no exceptions)
  //  - Functions return 0 on success; negative value on failure.
  //  - When an error buffer (err, err_cap) is provided, a human-readable
  //    message is written into it (NUL-terminated, truncated if necessary).
  // ---------------------------------------------------------------------------

  // ---------------------------------------------------------------------------
  // v4front_compile
  //  - Tier-0 frontend with arithmetic primitive support:
  //      * Each integer token emits:  [V4_OP_LIT, imm32_le]
  //      * Recognized primitives (+, -, *, /, MOD): emit their opcode
  //      * Always appends:            [V4_OP_RET]
  //      * Unknown token: error
  //  - Accepted integer formats: decimal, 0x... (hex), 0... (oct) via strtol
  //    base=0
  //
  //  Parameters:
  //    source   : ASCII text containing whitespace-separated tokens (may be
  //               NULL/empty)
  //    out_buf  : output buffer; on success caller must free via
  //               v4front_free()
  //    err      : optional error message buffer (may be NULL)
  //    err_cap  : capacity of `err` in bytes (0 allowed)
  //
  //  Returns:
  //    0 on success; <0 on failure (e.g., invalid token, OOM).
  // ---------------------------------------------------------------------------
  int v4front_compile(const char* source, V4FrontBuf* out_buf, char* err, size_t err_cap);

  // ---------------------------------------------------------------------------
  // v4front_compile_word
  //  - Same as v4front_compile, but carries a word name for future extensions.
  //  - Current minimal implementation ignores `name` and behaves like
  //    compile().
  // ---------------------------------------------------------------------------
  int v4front_compile_word(const char* name, const char* source, V4FrontBuf* out_buf,
                           char* err, size_t err_cap);

  // ---------------------------------------------------------------------------
  // v4front_free
  //  - Frees a buffer previously returned by v4front_compile(_word).
  //  - Safe to call with NULL or with an already-freed buffer (no-op).
  // ---------------------------------------------------------------------------
  void v4front_free(V4FrontBuf* buf);

#ifdef __cplusplus
}  // extern "C"
#endif