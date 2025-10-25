
# V4-front

A frontend for the V4 Virtual Machine.

Parses whitespace-separated integer tokens and emits V4 bytecode
as `[LIT imm32]* RET`.  
Designed for use in the early bootstrap stage of V4, with no exceptions and
a stable C API.

## Features

- Exception-free, RTTI-free C++17 implementation  
- Converts text to V4 bytecode (`LIT` / `RET` only for now)
- Supports decimal, hex (`0x..`), and octal (`0..`) integers
- C API with explicit error reporting (no C++ exceptions)
- Tested via vendored **doctest** (`tests/vendor/doctest/doctest.h`)

## API

```c
int  v4front_compile(const char* source,
                     V4FrontBuf* out_buf,
                     char* err, size_t err_cap);

int  v4front_compile_word(const char* name,
                          const char* source,
                          V4FrontBuf* out_buf,
                          char* err, size_t err_cap);

void v4front_free(V4FrontBuf* b);
````

`V4FrontBuf` holds the compiled bytecode buffer and must be freed
with `v4front_free()` after use.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DV4_FETCH=ON
cmake --build build -j
```

## Test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## License

Licensed under either of:

* MIT License ([LICENSE-MIT](LICENSE-MIT))
* Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE))

at your option.
