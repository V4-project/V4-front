# V4-front Changelog
All notable additions are listed here.

## [Unreleased] - 2025-10-25

### Added
- **Core frontend (`front_compile.cpp`)**
  - Implemented a minimal, exception-free Tier-0 compiler frontend.
  - Supports parsing of whitespace-separated integer tokens.
  - Emits bytecode sequence `[LIT imm32]* RET`.
  - Handles decimal, hex (`0x..`), and octal (`0..`) integer formats.
  - Uses explicit `malloc` / `free` allocation; returns errors instead of throwing.
  - Error messages are written into a provided buffer (`char* err`, `size_t err_cap`).
  - Fully compatible with `-fno-exceptions` and `-fno-rtti`.
  - Added support for arithmetic operations: `+`, `-`, `*`, `/`, `MOD`

- **Public API (`front_api.h`)**
  - Defined stable C ABI:
    - `int v4front_compile(const char* src, V4FrontBuf*, char* err, size_t err_cap)`
    - `int v4front_compile_word(const char* name, const char* src, V4FrontBuf*, char* err, size_t err_cap)`
    - `void v4front_free(V4FrontBuf*)`
  - Replaced the legacy `v4front_compile_single()` interface.
  - Deprecated `V4FrontError` struct (string-based error reporting now used).
  - Added optional inline shim for backward compatibility.

- **Testing**
  - Added vendored **doctest** at `tests/vendor/doctest/doctest.h`.
  - Created smoke tests verifying:
    - Empty / whitespace-only input → single `RET`
    - Integer, multiple, and negative literals
    - Hex and boundary values (`INT32_MIN`, `INT32_MAX`)
    - Unknown tokens → proper error messages
    - `v4front_compile_word()` wrapper consistency
  - **Added `test_arithmetic.cpp`** - Comprehensive arithmetic operation tests:
    - Simple addition (`10 20 +`)
    - Subtraction (`50 30 -`)
    - Multiplication (`6 7 *`)
    - Division (`42 7 /`)
    - Modulus (`43 7 MOD`)
    - Complex expressions (`1 2 + 3 *`)
    - Unknown operator error handling
    - Literal parsing validation
  - Configured doctest to work under `-fno-exceptions` via  
    `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`.
  - All tests properly define doctest macros before including headers.

---

