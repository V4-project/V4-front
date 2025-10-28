# V4-front Compiler Specification

## Overview

V4-front is a Forth-to-bytecode compiler for the V4 virtual machine. It compiles Forth source code into V4 bytecode, following a subset of the ANS Forth standard with extensions for system-level programming.

## Compilation Process

### Input

- **Source**: UTF-8 text string containing Forth source code
- **Context** (optional): Compiler context for incremental compilation (REPL support)

### Output

- **Bytecode**: Array of bytes representing V4 instructions
- **Word definitions**: Array of named word definitions (if any)
- **Error information**: Error code and optional detailed error message with position

### Phases

1. **Tokenization**: Split source into whitespace-separated tokens
2. **Parsing**: Recognize keywords, literals, operators, and word references
3. **Code generation**: Emit V4 bytecode instructions
4. **Word registration**: Register defined words with the context (if provided)

## Supported Syntax

### Literals

| Syntax | Example | Description |
|--------|---------|-------------|
| Decimal | `42`, `-10` | Signed 32-bit integer |
| Hexadecimal | `0xFF`, `0x10` | Hex literals (case-insensitive) |
| Octal | `077` | Octal literals (leading 0) |

Literals compile to: `LIT <value:i32>`

### Stack Operations

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `DUP` | `0x02` | `( a -- a a )` | Duplicate top item |
| `DROP` | `0x03` | `( a -- )` | Remove top item |
| `SWAP` | `0x04` | `( a b -- b a )` | Swap top two items |
| `OVER` | `0x05` | `( a b -- a b a )` | Copy second item to top |

### Arithmetic

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `+` | `0x10` | `( a b -- a+b )` | Addition |
| `-` | `0x11` | `( a b -- a-b )` | Subtraction |
| `*` | `0x12` | `( a b -- a*b )` | Multiplication |
| `/` | `0x13` | `( a b -- a/b )` | Division (signed) |
| `MOD` | `0x14` | `( a b -- a%b )` | Modulo |

### Comparison

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `=` | `0x20` | `( a b -- flag )` | Equal |
| `<>` | `0x21` | `( a b -- flag )` | Not equal |
| `<` | `0x22` | `( a b -- flag )` | Less than (signed) |
| `>` | `0x23` | `( a b -- flag )` | Greater than (signed) |
| `<=` | `0x24` | `( a b -- flag )` | Less or equal (signed) |
| `>=` | `0x25` | `( a b -- flag )` | Greater or equal (signed) |

Flags: `-1` (true), `0` (false)

### Bitwise Operations

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `AND` | `0x30` | `( a b -- a&b )` | Bitwise AND |
| `OR` | `0x31` | `( a b -- a\|b )` | Bitwise OR |
| `XOR` | `0x32` | `( a b -- a^b )` | Bitwise XOR |
| `INVERT` | `0x33` | `( a -- ~a )` | Bitwise NOT |

### Memory Access

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `@` | `0x40` | `( addr -- value )` | Load 32-bit value |
| `!` | `0x41` | `( value addr -- )` | Store 32-bit value |

### Return Stack

| Word | Bytecode | Stack Effect | Description |
|------|----------|--------------|-------------|
| `>R` | `0x06` | `( a -- ) R: ( -- a )` | Push to return stack |
| `R>` | `0x07` | `( -- a ) R: ( a -- )` | Pop from return stack |
| `R@` | `0x08` | `( -- a ) R: ( a -- a )` | Copy from return stack |
| `I` | `0x08` | `( -- index )` | Loop index (alias for R@) |
| `J` | - | `( -- index )` | Outer loop index (macro) |

### Control Flow

#### IF/THEN/ELSE

```forth
condition IF true-branch THEN
condition IF true-branch ELSE false-branch THEN
```

Compiles to:
- `IF`: `JZ <offset>` (jump if zero/false)
- `ELSE`: `JMP <offset>` (unconditional jump)
- Offsets are backpatched when `THEN` is encountered

#### BEGIN/UNTIL/AGAIN/WHILE/REPEAT

```forth
BEGIN ... condition UNTIL    \ Loop until condition is true
BEGIN ... AGAIN              \ Infinite loop
BEGIN ... condition WHILE ... REPEAT  \ Loop while condition is true
```

Compiles to:
- `UNTIL`: `JZ <back-offset>` (jump back to BEGIN if false)
- `AGAIN`: `JMP <back-offset>` (jump back to BEGIN)
- `WHILE`: `JZ <forward-offset>` (exit loop if false)
- `REPEAT`: `JMP <back-offset>` (jump back to BEGIN)

#### DO/LOOP

```forth
limit index DO ... LOOP       \ Loop from index to limit-1
limit index DO ... n +LOOP    \ Loop with step n
```

Compiles to:
- `DO`: `SWAP >R >R` (push limit and index to return stack)
- `LOOP`: Increment index, compare with limit, loop or exit
- `+LOOP`: Add step to index, compare with limit, loop or exit

Loop control:
- `I`: Current loop index
- `J`: Outer loop index (for nested loops)
- `LEAVE`: Exit loop early (not yet implemented)

#### Early Return

```forth
EXIT  \ Return from current word immediately
```

Compiles to: `RET`

### Word Definitions

```forth
: NAME ... ;
```

Defines a new word named `NAME`. The word's bytecode is compiled separately and registered with the VM.

**Compilation**:
1. `:` starts word definition
2. Next token is the word name
3. Following tokens are compiled as the word's body
4. `;` ends definition and emits `RET`

**Calling**: When `NAME` is encountered, compiles to `CALL <word-index>`

### System Calls

```forth
SYS <id>
```

Invokes a system call with the given ID (0-255). Used for HAL (Hardware Abstraction Layer) access.

**Examples**:
```forth
1 SYS 1       \ EMIT (output character)
SYS 2         \ KEY (input character)
13 1 SYS 0x01 \ GPIO_WRITE (write to GPIO pin)
```

Compiles to: `SYS <id:u8>`

**Standard SYS IDs**:
- `0x01`: GPIO operations
- `0x02`: UART operations
- `0x03`: Timer operations
- See V4 VM documentation for complete list

## Error Handling

### Error Codes

| Code | Name | Description |
|------|------|-------------|
| 0 | OK | Success |
| -1 | UnknownToken | Unrecognized word or syntax |
| -2 | InvalidInteger | Invalid number format |
| -3 | OutOfMemory | Memory allocation failed |
| -6 | ControlDepthExceeded | Too many nested control structures |
| -7 to -19 | Control flow errors | Mismatched IF/BEGIN/DO/etc. |
| -20 to -24 | Loop errors | LOOP without DO, etc. |
| -25 to -30 | Word definition errors | Invalid `:` or `;` |
| -31 | MissingSysId | SYS without ID |
| -32 | InvalidSysId | SYS ID out of range (0-255) |

### Error Reporting

**Basic API**:
```c
char errmsg[256];
v4front_err err = v4front_compile(source, &buf, errmsg, sizeof(errmsg));
if (err != 0) {
  printf("Error: %s\n", errmsg);
}
```

**Detailed API**:
```c
V4FrontError error;
v4front_err err = v4front_compile_ex(source, &buf, &error);
if (err != 0) {
  printf("Error at line %d, column %d: %s\n",
         error.line, error.column, error.message);
  printf("Token: %s\n", error.token);
}
```

## Compilation Limits

| Limit | Default | Description |
|-------|---------|-------------|
| `MAX_CONTROL_DEPTH` | 32 | Maximum nesting depth for control structures |
| `MAX_LEAVE_DEPTH` | 8 | Maximum nesting depth for LEAVE statements |
| `MAX_WORDS` | 256 | Maximum number of word definitions |
| `MAX_WORD_NAME_LEN` | 64 | Maximum word name length (including null) |
| `MAX_TOKEN_LEN` | 256 | Maximum token length (including null) |

These can be overridden at compile time with `-D` flags.

## Case Sensitivity

- **Keywords and operators**: Case-insensitive (`DUP`, `dup`, `Dup` are equivalent)
- **Word names**: Case-insensitive (`:  FOO  ...  ;` can be called as `foo`)
- **Hex literals**: Case-insensitive (`0xFF` = `0xff`)

## Stateful Compilation (REPL Support)

The compiler supports incremental compilation via `V4FrontContext`:

```c
// Create context
V4FrontContext* ctx = v4front_context_create();

// Compile first line
v4front_compile_with_context(ctx, ": DOUBLE DUP + ;", &buf1, err, sizeof(err));
// Register word with VM...

// Compile second line (can reference DOUBLE)
v4front_compile_with_context(ctx, "5 DOUBLE", &buf2, err, sizeof(err));

// Cleanup
v4front_context_destroy(ctx);
```

**Context operations**:
- `v4front_context_register_word()`: Register a word after VM registration
- `v4front_context_find_word()`: Look up word by name
- `v4front_context_reset()`: Clear all registered words

## Bytecode Generation Rules

### Literal Encoding

All literals are encoded as 32-bit little-endian integers:

```
LIT:  [0x00] [byte0] [byte1] [byte2] [byte3]
```

Example: `42` → `00 2A 00 00 00`

### Jump Offsets

All jump offsets are 16-bit signed little-endian, relative to the instruction **after** the jump:

```
JZ:   [0x50] [offset_low] [offset_high]
JMP:  [0x52] [offset_low] [offset_high]
JNZ:  [0x51] [offset_low] [offset_high]
```

Offset calculation: `target_address - (jump_instruction_address + 3)`

### Word Calls

```
CALL: [0x53] [index_low] [index_high]
```

Word index is 16-bit little-endian.

### Implicit RET

All compiled code ends with an implicit `RET` instruction (`0x51`), even if not explicitly written in the source.

## Integration with V4 VM

### Bytecode Format

Compiled bytecode can be:
1. **Executed directly**: Pass `buf.data` to `vm_load_bytecode()`
2. **Saved to file**: Use `v4front_save_bytecode()` to create `.v4b` files
3. **Loaded from file**: Use `v4front_load_bytecode()` to read `.v4b` files

### Word Registration

When compiling word definitions, register them with the VM:

```c
for (int i = 0; i < buf.word_count; i++) {
  int vm_idx = vm_register_word(vm, buf.words[i].name,
                                  buf.words[i].code,
                                  buf.words[i].code_len);
  v4front_context_register_word(ctx, buf.words[i].name, vm_idx);
}
```

## Future Extensions

Planned features:
- [ ] `LEAVE` statement for DO loops
- [ ] `VARIABLE` and `CONSTANT` definitions
- [ ] String literals
- [ ] Comments (`\` and `( ... )`)
- [ ] Floating-point literals
- [ ] Assembly-level control (inline VM opcodes)

## References

- [ANS Forth Standard](https://forth-standard.org/)
- [V4 VM Instruction Set](https://github.com/kirisaki/v4/docs/isa.md)
- [Bytecode File Format](bytecode-format.md)
