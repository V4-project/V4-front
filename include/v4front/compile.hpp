#pragma once
#include <cstddef>
#include <cstdint>

#include "v4front/compile.h"

namespace v4front
{

/**
 * @brief RAII wrapper for bytecode buffer (no exceptions).
 *
 * Provides automatic memory management while maintaining compatibility
 * with -fno-exceptions builds. Follows V4's design philosophy of C API
 * as primary interface with C++ convenience wrappers.
 *
 * Example usage:
 * @code
 *   v4front::BytecodeBuffer buf;
 *   if (buf.compile("10 20 +") == 0) {
 *       // Use buf.data(), buf.size()
 *   } else {
 *       // Handle error
 *   }
 *   // Automatic cleanup on scope exit
 * @endcode
 */
class BytecodeBuffer
{
 private:
  V4FrontBuf buf_;

 public:
  /**
   * @brief Construct an empty buffer.
   */
  BytecodeBuffer() noexcept : buf_{nullptr, 0, nullptr, 0} {}

  /**
   * @brief Destructor - automatically frees allocated bytecode.
   */
  ~BytecodeBuffer()
  {
    v4front_free(&buf_);
  }

  // Non-copyable (prevent double-free)
  BytecodeBuffer(const BytecodeBuffer&) = delete;
  BytecodeBuffer& operator=(const BytecodeBuffer&) = delete;

  // Movable (transfer ownership)
  BytecodeBuffer(BytecodeBuffer&& other) noexcept : buf_(other.buf_)
  {
    other.buf_ = {nullptr, 0, nullptr, 0};
  }

  BytecodeBuffer& operator=(BytecodeBuffer&& other) noexcept
  {
    if (this != &other)
    {
      v4front_free(&buf_);
      buf_ = other.buf_;
      other.buf_ = {nullptr, 0, nullptr, 0};
    }
    return *this;
  }

  /**
   * @brief Compile source code to bytecode.
   *
   * @param source Source code string (null-terminated).
   * @param err Optional error message buffer (may be nullptr).
   * @param err_cap Capacity of error buffer in bytes.
   * @return 0 on success, negative error code on failure.
   *
   * Example:
   * @code
   *   BytecodeBuffer buf;
   *   char err[256];
   *   if (buf.compile("1 2 +", err, sizeof(err)) != 0) {
   *       fprintf(stderr, "Error: %s\n", err);
   *   }
   * @endcode
   */
  int compile(const char* source, char* err = nullptr, size_t err_cap = 0) noexcept
  {
    v4front_free(&buf_);  // Free any existing data
    return v4front_compile(source, &buf_, err, err_cap);
  }

  /**
   * @brief Compile a named word (for future use).
   *
   * @param name Word name (currently ignored).
   * @param source Source code string.
   * @param err Optional error message buffer.
   * @param err_cap Capacity of error buffer.
   * @return 0 on success, negative error code on failure.
   */
  int compile_word(const char* name, const char* source, char* err = nullptr,
                   size_t err_cap = 0) noexcept
  {
    v4front_free(&buf_);
    return v4front_compile_word(name, source, &buf_, err, err_cap);
  }

  /**
   * @brief Get pointer to bytecode data.
   * @return Pointer to bytecode, or nullptr if empty.
   */
  uint8_t* data() noexcept
  {
    return buf_.data;
  }

  /**
   * @brief Get pointer to bytecode data (const version).
   * @return Const pointer to bytecode, or nullptr if empty.
   */
  const uint8_t* data() const noexcept
  {
    return buf_.data;
  }

  /**
   * @brief Get size of bytecode in bytes.
   * @return Size in bytes, or 0 if empty.
   */
  size_t size() const noexcept
  {
    return buf_.size;
  }

  /**
   * @brief Check if buffer is empty.
   * @return true if no bytecode is stored, false otherwise.
   */
  bool empty() const noexcept
  {
    return buf_.data == nullptr || buf_.size == 0;
  }

  /**
   * @brief Explicitly release ownership of the buffer.
   *
   * Caller is responsible for calling v4front_free() on the returned buffer.
   *
   * @return The internal buffer. This object becomes empty.
   */
  V4FrontBuf release() noexcept
  {
    V4FrontBuf result = buf_;
    buf_ = {nullptr, 0, nullptr, 0};
    return result;
  }

  /**
   * @brief Clear the buffer (free memory).
   */
  void clear() noexcept
  {
    v4front_free(&buf_);
    buf_ = {nullptr, 0, nullptr, 0};
  }
};

/**
 * @brief Free function: compile source to bytecode (no exceptions).
 *
 * Convenience function that returns error code instead of throwing exceptions.
 * Memory must be freed by caller using v4front_free().
 *
 * @param source Source code string.
 * @param out_buf Output buffer (caller must free with v4front_free).
 * @param err Optional error message buffer.
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, negative error code on failure.
 *
 * Example:
 * @code
 *   V4FrontBuf buf;
 *   if (v4front::compile("1 2 +", &buf) == 0) {
 *       // Use buf.data, buf.size
 *       v4front_free(&buf);
 *   }
 * @endcode
 */
inline int compile(const char* source, V4FrontBuf* out_buf, char* err = nullptr,
                   size_t err_cap = 0) noexcept
{
  return v4front_compile(source, out_buf, err, err_cap);
}

/**
 * @brief Free function: compile named word (no exceptions).
 *
 * @param name Word name (currently ignored).
 * @param source Source code string.
 * @param out_buf Output buffer (caller must free with v4front_free).
 * @param err Optional error message buffer.
 * @param err_cap Capacity of error buffer.
 * @return 0 on success, negative error code on failure.
 */
inline int compile_word(const char* name, const char* source, V4FrontBuf* out_buf,
                        char* err = nullptr, size_t err_cap = 0) noexcept
{
  return v4front_compile_word(name, source, out_buf, err, err_cap);
}

/**
 * @brief Get human-readable error message for error code.
 *
 * @param code Error code from compile functions.
 * @return Error message string (never nullptr).
 */
inline const char* err_str(int code) noexcept
{
  return v4front_err_str(code);
}

}  // namespace v4front
