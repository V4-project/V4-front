#pragma once

/* V4-front Error Handling (C++ API)
 *
 * Error code definition using the same pattern as V4 errors.hpp
 */

// Map V4FRONT_ERR to enum values
#define V4FRONT_ERR(name, val, msg) name = val,

namespace v4front
{

enum class FrontErr : int
{
#include "v4front/errors.def"
};

#undef V4FRONT_ERR

// Helper to get a string message for each error
inline const char* front_err_str(FrontErr e)
{
  switch (e)
  {
#define V4FRONT_ERR(name, val, msg) \
  case FrontErr::name:              \
    return msg;
#include "v4front/errors.def"
#undef V4FRONT_ERR
    default:
      return "unknown error";
  }
}

// Helper to convert FrontErr to int (for C API compatibility)
inline int front_err_to_int(FrontErr e)
{
  return static_cast<int>(e);
}

// Helper to convert int to FrontErr
inline FrontErr int_to_front_err(int code)
{
  return static_cast<FrontErr>(code);
}

// Check if error code represents success
inline bool is_ok(FrontErr e)
{
  return e == FrontErr::OK;
}

// Check if error code represents an error
inline bool is_error(FrontErr e)
{
  return e != FrontErr::OK;
}

}  // namespace v4front

// ---------------------------------------------------------------------------
// C API interoperability helpers
// These allow seamless use of FrontErr with C API functions
// ---------------------------------------------------------------------------

// Forward declare v4front_err_t from errors.h
#ifndef V4FRONT_ERRORS_H_INCLUDED
extern "C"
{
  typedef int v4front_err_t;
  const char* v4front_err_str(v4front_err_t err);
}
#endif

// C++ overload for v4front_err_str that accepts FrontErr
inline const char* v4front_err_str(v4front::FrontErr err)
{
  return v4front_err_str(v4front::front_err_to_int(err));
}

// ---------------------------------------------------------------------------
// Comparison operators for seamless C/C++ API interop
// These allow direct comparison between v4front_err (int) and FrontErr
// ---------------------------------------------------------------------------

// Allow comparing v4front_err_t (int) with FrontErr enum
inline bool operator==(v4front_err_t lhs, v4front::FrontErr rhs)
{
  return lhs == v4front::front_err_to_int(rhs);
}

inline bool operator==(v4front::FrontErr lhs, v4front_err_t rhs)
{
  return v4front::front_err_to_int(lhs) == rhs;
}

inline bool operator!=(v4front_err_t lhs, v4front::FrontErr rhs)
{
  return !(lhs == rhs);
}

inline bool operator!=(v4front::FrontErr lhs, v4front_err_t rhs)
{
  return !(lhs == rhs);
}