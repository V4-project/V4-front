# V4-front Changelog
All notable additions are listed here.

## [Unreleased] - 2025-10-27

### Added
- **Word definitions**
  - Implemented `:` (colon) and `;` (semicolon) keywords for defining custom words
  - Added CALL instruction generation for invoking defined words
  - Support for multiple word definitions in a single compilation unit
  - Words can call other words (recursive and mutual recursion supported)
  - Control flow structures (IF/THEN/ELSE, DO/LOOP, BEGIN/UNTIL, etc.) work inside word definitions
  - Case-insensitive word name lookup
  - Proper error handling for duplicate words, unclosed definitions, nested colons, etc.
  - Extended `V4FrontBuf` to include `words` array and `word_count` field
  - Added comprehensive test suite (`test_word_definitions.cpp`) with 72 assertions
- **EXIT keyword**
  - Implemented EXIT for early return from words (emits RET instruction)
  - Works in both word definitions and main code
  - Can be used multiple times in a single word
  - Commonly used with IF for conditional early returns

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
  - Added support for arithmetic operations
  - Added support for comparison operations
  - Added support for bitwise operations
  - Added support for stack operations
  - Added support for return stack operations

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
  - Added `test_arithmetic.cpp
  - Added `test_comparison.cpp
  - Added `test_bitwise.cpp
  - Added `test_stack.cpp
  - Configured doctest to work under `-fno-exceptions` via  
    `DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS`.
  - All tests properly define doctest macros before including headers.

---

