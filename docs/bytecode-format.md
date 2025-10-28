# V4 Bytecode File Format (.v4b)

## Overview

The `.v4b` file format is a binary format for storing compiled V4 bytecode. It allows compiled Forth programs to be saved to disk and loaded later for execution by the V4 virtual machine.

## File Structure

```
+------------------+
| Header (16 bytes)|
+------------------+
| Bytecode         |
+------------------+
```

### File Header

The file header is exactly 16 bytes and contains metadata about the bytecode:

```c
typedef struct {
  uint8_t  magic[4];      // Magic number: "V4BC" (0x56 0x34 0x42 0x43)
  uint8_t  version_major; // Major version number
  uint8_t  version_minor; // Minor version number
  uint16_t flags;         // Reserved flags (must be 0)
  uint32_t code_size;     // Size of bytecode in bytes
  uint32_t reserved;      // Reserved for future use (must be 0)
} V4BytecodeHeader;
```

#### Field Descriptions

| Field | Size | Offset | Description |
|-------|------|--------|-------------|
| `magic` | 4 bytes | 0 | Magic number identifying the file format. Must be `"V4BC"` (ASCII: 0x56 0x34 0x42 0x43) |
| `version_major` | 1 byte | 4 | Major version number. Currently `0` |
| `version_minor` | 1 byte | 5 | Minor version number. Currently `1` |
| `flags` | 2 bytes | 6 | Reserved for future use. Must be `0` |
| `code_size` | 4 bytes | 8 | Size of bytecode section in bytes (little-endian) |
| `reserved` | 4 bytes | 12 | Reserved for future use. Must be `0` |

**Total header size**: 16 bytes

#### Endianness

- Multi-byte fields (`flags`, `code_size`, `reserved`) are stored in **little-endian** format
- This matches the V4 VM's internal byte order

### Bytecode Section

Immediately following the header is the bytecode section containing the compiled V4 instructions. The size of this section is specified in the header's `code_size` field.

The bytecode format follows the V4 instruction set architecture (ISA):
- Single-byte opcodes
- Variable-length instructions (some opcodes have immediate operands)
- Little-endian encoding for multi-byte operands

## File Format Version

**Current version**: 0.1

### Version History

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2025-01 | Initial format specification |

### Version Compatibility

- **Major version** changes indicate incompatible format changes
- **Minor version** changes indicate backward-compatible additions
- Loaders should reject files with a different major version
- Loaders may accept files with a lower minor version

## Example: Hexdump of Simple Program

Compiling the Forth program `42 DUP +` produces the following `.v4b` file:

```
Offset  Hex                                          ASCII
------  -------------------------------------------  ----------------
0x0000  56 34 42 43 01 00 00 00 0C 00 00 00 00 00  V4BC............
0x000E  00 00 00 2A 00 00 00 02 00 10 51           ...*......Q
```

Breaking this down:

```
Header (16 bytes):
  56 34 42 43    - Magic: "V4BC"
  00             - Major version: 0
  01             - Minor version: 1
  00 00          - Flags: 0
  0C 00 00 00    - Code size: 12 bytes (little-endian)
  00 00 00 00    - Reserved: 0

Bytecode (12 bytes):
  00 2A 00 00 00 - LIT 42  (opcode 0x00, operand 0x0000002A)
  02             - DUP     (opcode 0x02)
  10             - ADD     (opcode 0x10)
  51             - RET     (opcode 0x51)
```

## API Usage

### Saving Bytecode

```c
#include "v4front/compile.h"

// Compile Forth source
V4FrontBuf buf;
char errmsg[256];
v4front_err err = v4front_compile("42 DUP +", &buf, errmsg, sizeof(errmsg));
if (err != 0) {
  // Handle compilation error
}

// Save to .v4b file
err = v4front_save_bytecode(&buf, "program.v4b");
if (err != 0) {
  // Handle save error
}

// Cleanup
v4front_free(&buf);
```

### Loading Bytecode

```c
#include "v4front/compile.h"

// Load from .v4b file
V4FrontBuf buf;
v4front_err err = v4front_load_bytecode("program.v4b", &buf);
if (err != 0) {
  // Handle load error
  // err == -4: Invalid magic number
  // err == -2: File not found
  // etc.
}

// Use bytecode (e.g., execute in V4 VM)
// ...

// Cleanup
v4front_free(&buf);
```

## Error Codes

| Error Code | Value | Description |
|------------|-------|-------------|
| Success | 0 | Operation completed successfully |
| Invalid Parameters | -1 | NULL buffer or filename |
| File Open Error | -2 | Failed to open file |
| Read/Write Error | -3 | Failed to read/write header |
| Invalid Magic | -4 | File does not start with "V4BC" |
| Out of Memory | -5 | Failed to allocate memory |
| Bytecode Read Error | -6 | Failed to read bytecode |

## Implementation Notes

### File I/O

- Files are opened in binary mode (`"wb"` for writing, `"rb"` for reading)
- Header is written/read as a single 16-byte block
- Bytecode is written/read in a single operation
- Files should be closed properly even on error

### Memory Management

- Loaded bytecode is allocated with `malloc()`
- Caller must call `v4front_free()` to deallocate
- Failed loads do not require cleanup (no allocation on error)

### Validation

Loaders must validate:
1. Magic number is exactly `"V4BC"`
2. File size is at least 16 bytes (header size)
3. File contains at least `code_size` bytes after header
4. Major version matches expected version

Optional validations:
- Minor version compatibility check
- Flags field is zero (reserved)
- Reserved field is zero

### Security Considerations

- Always validate `code_size` to prevent integer overflow
- Limit maximum bytecode size to prevent excessive memory allocation
- Validate file size matches header `code_size` before allocation

## Future Extensions

Potential future additions (backward-compatible):

1. **Metadata Section**: Store source file name, compilation timestamp, etc.
2. **Symbol Table**: Preserve word names for debugging
3. **Debug Information**: Line numbers, source positions
4. **Compression**: Optional compression flag in `flags` field
5. **Checksums**: CRC32 or similar for integrity verification

These would use the `flags` field to indicate presence and use bytes after the bytecode section.

## Related Documentation

- [V4 Instruction Set Architecture](https://github.com/kirisaki/v4/docs/isa.md)
- [V4-front Compiler Specification](compiler-spec.md)
- [V4 VM API Reference](https://github.com/kirisaki/v4/docs/api.md)
