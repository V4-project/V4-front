# KAT (Known Answer Test) Format

## Overview

KAT (Known Answer Test) files provide a declarative way to specify compiler test cases. Each test consists of Forth source code and the expected bytecode output, allowing automatic verification of the compiler's correctness.

## File Format

### Structure

```
# Comment
## Test: Test Name
SOURCE: <forth-source>
BYTECODE: <hex-bytes>
# Optional comment explaining bytecode
```

### Elements

#### Comments

Lines starting with `#` are comments and are ignored by the parser (unless they start with `##`).

```
# This is a comment
```

#### Test Header

Test definitions start with `## Test:` followed by a descriptive name:

```
## Test: Simple addition
```

#### Source Line

The `SOURCE:` line contains the Forth source code to compile:

```
SOURCE: 10 20 +
```

- Source can contain any valid Forth syntax
- Whitespace is normalized during parsing
- Source should be a single line

#### Bytecode Line

The `BYTECODE:` line contains the expected bytecode as space-separated hex bytes:

```
BYTECODE: 00 0A 00 00 00  00 14 00 00 00  10  51
```

- Bytes are in hexadecimal (case-insensitive)
- Spaces between bytes are optional (but recommended for readability)
- Extra spaces and grouping are allowed for clarity
- Comments can follow on the same line after the bytecode

### Example

```
## Test: Simple addition
SOURCE: 10 20 +
BYTECODE: 00 0A 00 00 00  00 14 00 00 00  10  51
# Breakdown:
# 00 0A 00 00 00  - LIT 10
# 00 14 00 00 00  - LIT 20
# 10              - ADD
# 51              - RET
```

## Complete Example File

```
# arithmetic.kat - Arithmetic operation tests

## Test: Simple addition
SOURCE: 10 20 +
BYTECODE: 00 0A 00 00 00  00 14 00 00 00  10  51

## Test: Subtraction
SOURCE: 100 50 -
BYTECODE: 00 64 00 00 00  00 32 00 00 00  11  51

## Test: Multiplication
SOURCE: 7 6 *
BYTECODE: 00 07 00 00 00  00 06 00 00 00  12  51

## Test: Division
SOURCE: 100 5 /
BYTECODE: 00 64 00 00 00  00 05 00 00 00  13  51

## Test: Modulo
SOURCE: 17 5 MOD
BYTECODE: 00 11 00 00 00  00 05 00 00 00  14  51

## Test: Complex expression
SOURCE: 10 20 + 5 *
BYTECODE: 00 0A 00 00 00  00 14 00 00 00  10  00 05 00 00 00  12  51
```

## Bytecode Reference

### Common Opcodes

| Opcode | Mnemonic | Format |
|--------|----------|--------|
| `00` | LIT | `00 <i32_le>` - Push 32-bit literal |
| `02` | DUP | `02` - Duplicate top item |
| `03` | DROP | `03` - Drop top item |
| `04` | SWAP | `04` - Swap top two items |
| `05` | OVER | `05` - Copy second item to top |
| `06` | TOR | `06` - Push to return stack (>R) |
| `07` | FROMR | `07` - Pop from return stack (R>) |
| `08` | RFETCH | `08` - Fetch from return stack (R@) |
| `10` | ADD | `10` - Addition (+) |
| `11` | SUB | `11` - Subtraction (-) |
| `12` | MUL | `12` - Multiplication (*) |
| `13` | DIV | `13` - Division (/) |
| `14` | MOD | `14` - Modulo |
| `20` | EQ | `20` - Equal (=) |
| `21` | NE | `21` - Not equal (<>) |
| `22` | LT | `22` - Less than (<) |
| `23` | GT | `23` - Greater than (>) |
| `24` | LE | `24` - Less or equal (<=) |
| `25` | GE | `25` - Greater or equal (>=) |
| `30` | AND | `30` - Bitwise AND |
| `31` | OR | `31` - Bitwise OR |
| `32` | XOR | `32` - Bitwise XOR |
| `33` | INVERT | `33` - Bitwise NOT |
| `40` | LOAD | `40` - Load from memory (@) |
| `41` | STORE | `41` - Store to memory (!) |
| `50` | JZ | `50 <i16_le>` - Jump if zero |
| `51` | JNZ | `51 <i16_le>` - Jump if not zero |
| `52` | JMP | `52 <i16_le>` - Unconditional jump |
| `53` | CALL | `53 <i16_le>` - Call word |
| `51` | RET | `51` - Return |
| `60` | SYS | `60 <u8>` - System call |

### Encoding Rules

#### Literals (LIT)

```
Opcode: 00
Operand: 32-bit signed integer (little-endian)
Total: 5 bytes

Example: 42 (0x0000002A)
Bytecode: 00 2A 00 00 00
```

#### Jump Offsets

```
Opcodes: JZ (50), JNZ (51), JMP (52)
Operand: 16-bit signed offset (little-endian)
Total: 3 bytes

Offset is relative to the next instruction after the jump.
```

#### Word Calls

```
Opcode: CALL (53)
Operand: 16-bit word index (little-endian)
Total: 3 bytes
```

#### SYS Calls

```
Opcode: SYS (60)
Operand: 8-bit system call ID
Total: 2 bytes

Example: SYS 1
Bytecode: 60 01
```

## Usage

### Running KAT Tests

```cpp
#include "kat_runner.hpp"

// Load and run all tests in a KAT file
auto tests = load_kat_file("tests/kat/arithmetic.kat");
for (const auto& test : tests) {
  run_kat_test(test);
}
```

### Test Execution

For each test case:
1. Compile the SOURCE using `v4front_compile()`
2. Compare the output bytecode with expected BYTECODE
3. Report pass/fail with test name

### Error Handling

- **Parse errors**: Invalid KAT file format
- **Compilation errors**: SOURCE fails to compile
- **Bytecode mismatch**: Compiled bytecode doesn't match expected

## Best Practices

### Writing KAT Files

1. **One category per file**: Group related tests (arithmetic, control flow, etc.)
2. **Descriptive names**: Use clear test names that describe what is being tested
3. **Comments**: Add comments explaining complex bytecode sequences
4. **Incremental complexity**: Start with simple cases, add more complex ones
5. **Edge cases**: Include boundary conditions and corner cases

### Bytecode Formatting

Use consistent formatting for readability:

```
# Group by instruction (5 bytes for LIT, 1 for opcodes)
BYTECODE: 00 0A 00 00 00  00 14 00 00 00  10  51

# Add comments for complex sequences
BYTECODE: 50 03 00  00 05 00 00 00  52 00 00  51
# JZ +3     LIT 5            JMP +0   RET
```

### Test Organization

Organize tests by category:

```
tests/kat/
  arithmetic.kat    - Basic arithmetic operations
  stack.kat         - Stack manipulation
  control.kat       - Control flow structures
  memory.kat        - Memory access operations
  sys.kat           - System call instructions
  words.kat         - Word definitions and calls
```

## Limitations

- Source must be a single line (no multi-line definitions yet)
- Word definitions require context (may need special handling)
- Jump offsets must be calculated manually
- No support for relative bytecode (all offsets are absolute within the compiled code)

## Future Extensions

Potential additions:
- Multi-line SOURCE support
- Context setup for word definitions
- Bytecode templates (symbolic offsets)
- Error test cases (expected compilation failures)
- Performance benchmarks

## Related Documentation

- [Compiler Specification](compiler-spec.md)
- [Bytecode File Format](bytecode-format.md)
- [V4 Instruction Set](https://github.com/kirisaki/v4/docs/isa.md)
