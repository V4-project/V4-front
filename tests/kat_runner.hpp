#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace v4front
{
namespace kat
{

// Single KAT test case
struct KatTest
{
  std::string name;                     // Test name
  std::string source;                   // Forth source code
  std::vector<uint8_t> expected_bytes;  // Expected bytecode
};

// Load all tests from a KAT file
// Returns empty vector on parse error
std::vector<KatTest> load_kat_file(const char* filename);

// Parse a single hex byte string (e.g., "FF" -> 0xFF)
// Returns true on success, false on parse error
bool parse_hex_byte(const char* str, uint8_t* out);

// Parse a space-separated hex byte sequence
// Example: "00 0A 00 00 00" -> {0x00, 0x0A, 0x00, 0x00, 0x00}
std::vector<uint8_t> parse_hex_bytes(const std::string& hex_str);

}  // namespace kat
}  // namespace v4front
