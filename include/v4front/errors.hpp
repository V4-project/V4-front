#pragma once

// Error code definition using the same pattern as V4 errors.hpp
// Define ERR(name, val, msg) before including this file if you want to extract text or
// mapping.

#ifndef ERR
#define ERR(name, val, msg) name = val,
#endif

namespace v4front
{

enum class FrontErr : int
{
#include "v4front/errors.def"
};

#undef ERR

// Helper to get a string message for each error
inline const char* front_err_str(FrontErr e)
{
  switch (e)
  {
#define ERR(name, val, msg) \
  case FrontErr::name:      \
    return msg;
#include "v4front/errors.def"
#undef ERR
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

}  // namespace v4front