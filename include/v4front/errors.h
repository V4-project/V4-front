/* V4-front Error Handling (C API)
 *
 * Error code convention:
 *   0           = success
 *   negative    = error
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* Error code type */
  typedef int32_t v4front_err_t;

/* Error code constants */
#define V4FRONT_ERR(name, val, msg) V4FRONT_ERR_##name = val,
  enum v4front_err_code
  {
#include "v4front/errors.def"
  };
#undef V4FRONT_ERR

/* Redefine for direct use without V4FRONT_ERR_ prefix in C code */
#define ERR_OK V4FRONT_ERR_OK
#define ERR_UNKNOWN_TOKEN V4FRONT_ERR_UnknownToken
#define ERR_INVALID_INTEGER V4FRONT_ERR_InvalidInteger
#define ERR_OUT_OF_MEMORY V4FRONT_ERR_OutOfMemory
#define ERR_BUFFER_TOO_SMALL V4FRONT_ERR_BufferTooSmall
#define ERR_EMPTY_INPUT V4FRONT_ERR_EmptyInput
#define ERR_INVALID_OPCODE V4FRONT_ERR_InvalidOpcode
#define ERR_UNEXPECTED_EOF V4FRONT_ERR_UnexpectedEOF
#define ERR_SYNTAX_ERROR V4FRONT_ERR_SyntaxError
#define ERR_TOO_MANY_LABELS V4FRONT_ERR_TooManyLabels
#define ERR_UNDEFINED_LABEL V4FRONT_ERR_UndefinedLabel
#define ERR_DUPLICATE_LABEL V4FRONT_ERR_DuplicateLabel
#define ERR_LOOP_WITHOUT_DO V4FRONT_ERR_LoopWithoutDo
#define ERR_PLOOP_WITHOUT_DO V4FRONT_ERR_PLoopWithoutDo
#define ERR_UNCLOSED_DO V4FRONT_ERR_UnclosedDo

  /* Get error message string for an error code */
  const char* v4front_err_str(v4front_err_t err);

  /* Check if error code represents success */
  static inline int v4front_is_ok(v4front_err_t err)
  {
    return err == V4FRONT_ERR_OK;
  }

  /* Check if error code represents an error */
  static inline int v4front_is_error(v4front_err_t err)
  {
    return err != V4FRONT_ERR_OK;
  }

#ifdef __cplusplus
}
#endif