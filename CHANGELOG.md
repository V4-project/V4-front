# V4-front Changelog
All notable additions are listed here.

## [0.2.2] - 2025-01-10

### Changed
- **Code Refactoring**
  - Replaced large if-else chain (~200 lines) in operator matching with dispatch table approach
  - Introduced `OpcodeMapping` structure and `OPCODE_TABLE` for cleaner opcode lookup
  - Extracted J and K loop index instructions into separate helper functions (`emit_j_instruction`, `emit_k_instruction`)
  - Reduced `compile.cpp` from 1964 to 1914 lines (-50 lines, -2.5%)
  - Improved code maintainability: adding new operators now requires just one table entry instead of multiple if-else branches
  - Better separation of concerns with focused helper functions

## [0.2.1] - 2025-01-10

### Fixed
- **CI Build Fixes**
  - Fixed `test_integration` linking on Ubuntu by adding `mock_hal` library dependency
  - Fixed Windows MSVC builds by adding `_CRT_SECURE_NO_WARNINGS` to suppress C4996 warnings for standard C functions (`fopen`, etc.)
  - Fixed `test_bytecode_io` on Windows by using portable relative paths instead of Unix-specific `/tmp/` directory
  - All CI configurations now pass: Ubuntu Debug/Release, Windows Debug/Release, macOS Debug/Release

## [0.2.0] - 2025-01-10

### Added
- **SYS instruction support**
  - Implemented `SYS <id>` syntax for system calls with 8-bit immediate operand (0-255)
  - Support for decimal and hexadecimal SYS IDs
  - Case-insensitive SYS keyword matching
  - Works in expressions and word definitions
  - Added error codes: `MissingSysId` (-31), `InvalidSysId` (-32)
  - Comprehensive test suite (`test_sys_compile.cpp`) with 66 assertions

- **Bytecode file I/O (.v4b format)**
  - New binary file format for saving/loading compiled bytecode
  - 16-byte header with magic number ("V4BC"), version info, and metadata
  - `v4front_save_bytecode()` - Save bytecode to .v4b file
  - `v4front_load_bytecode()` - Load bytecode from .v4b file
  - Magic number validation and error handling
  - Little-endian encoding for cross-platform compatibility
  - Complete format specification (`docs/bytecode-format.md`)
  - Full test suite (`test_bytecode_io.cpp`) with 78 assertions

- **KAT (Known Answer Test) framework**
  - Declarative test specification format (.kat files)
  - KAT parser and runner implementation
  - 6 test categories with 36 test cases:
    - `arithmetic.kat` - Basic arithmetic operations (8 tests)
    - `stack.kat` - Stack manipulation (7 tests)
    - `control.kat` - Control flow structures (4 tests)
    - `memory.kat` - Memory access operations (5 tests)
    - `sys.kat` - System call instructions (7 tests)
    - `words.kat` - Word definitions (5 tests)
  - Space-separated hex bytecode format with comments
  - Automated bytecode verification
  - KAT format specification (`docs/kat-format.md`)
  - Test runner (`test_kat.cpp`) with 504 assertions

### Documentation
- Added comprehensive compiler specification (`docs/compiler-spec.md`, 342 lines)
- Added bytecode file format specification (`docs/bytecode-format.md`, 214 lines)
- Added KAT format specification (`docs/kat-format.md`, 269 lines)
- Updated README.md with new features, API examples, and syntax reference
- Added code formatting instructions (`make format`, `make format-check`)
- Documented all supported Forth syntax with examples

### Changed
- Increased test coverage to 1,400+ assertions across 23 test suites
- Enhanced error reporting with detailed position tracking
- Updated build system to support KAT tests

## [0.1.0] - 2025-10-27

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
- **Memory access operators**
  - Implemented `@` (fetch/LOAD) for reading 32-bit values from memory
  - Implemented `!` (store/STORE) for writing 32-bit values to memory
  - Both operators work in word definitions, main code, and control structures
  - Added comprehensive test suite (`test_memory_access.cpp`) with 33 assertions
- **Integration tests**
  - Added integration test suite (`test_integration.cpp`) that verifies V4-front compiled bytecode executes correctly in V4 VM
  - Tests verify compilation, word registration, and execution for arithmetic and word definitions
  - Integration tests conditionally build when V4 VM library is available (via V4_SRC_DIR or V4_FETCH)
  - CMake support for V4_SRC_DIR, V4_INCLUDE_DIR, and V4_FETCH options

## [0.1.0] - Continued

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

