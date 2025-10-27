# V4-front

A Forth compiler frontend for the V4 Virtual Machine.

Compiles Forth source code to V4 bytecode with no exceptions, using a stable C API suitable for early bootstrap stages.

## Features

- **Full Forth compiler**: Arithmetic, comparison, bitwise, stack operations, control flow (IF/THEN/ELSE, DO/LOOP, BEGIN/UNTIL/WHILE/REPEAT/AGAIN), word definitions (`:` `;`), memory access (`@` `!`), and more
- **Exception-free C++17**: No RTTI, explicit error reporting via return codes
- **Integration tested**: End-to-end validation with V4 VM
- **Well-tested**: Comprehensive test suite with 700+ assertions using doctest

## Quick Start

```bash
# Build with V4 headers from Git
cmake -B build -DV4_FETCH=ON
cmake --build build

# Run tests
ctest --test-dir build --output-on-failure
```

## API

```c
#include "v4front/compile.h"

// Compile Forth source to V4 bytecode
int v4front_compile(const char* source, V4FrontBuf* out_buf,
                    char* err, size_t err_cap);

// Free compiled bytecode
void v4front_free(V4FrontBuf* buf);
```

## Build Options

- `-DV4_FETCH=ON` - Fetch V4 from Git (enables integration tests)
- `-DV4_SRC_DIR=/path/to/V4` - Use local V4 source (enables integration tests)
- `-DV4_INCLUDE_DIR=/path/to/V4/include` - Use V4 headers only (no integration tests)

## Example

```c
V4FrontBuf buf;
char errmsg[256];

// Compile Forth code
if (v4front_compile(": DOUBLE DUP + ; 5 DOUBLE", &buf, errmsg, sizeof(errmsg)) == 0) {
    // buf.data contains V4 bytecode
    // buf.words contains defined words
    v4front_free(&buf);
}
```

## License

Licensed under either of:

* MIT License ([LICENSE-MIT](LICENSE-MIT))
* Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))

at your option.
