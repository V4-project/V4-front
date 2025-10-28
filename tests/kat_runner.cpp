#include "kat_runner.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace v4front
{
namespace kat
{

// Helper: trim whitespace from both ends
static std::string trim(const std::string& str)
{
  size_t start = 0;
  size_t end = str.length();

  while (start < end && std::isspace(static_cast<unsigned char>(str[start])))
    start++;
  while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
    end--;

  return str.substr(start, end - start);
}

// Helper: check if string starts with prefix (case-sensitive)
static bool starts_with(const std::string& str, const std::string& prefix)
{
  if (str.length() < prefix.length())
    return false;
  return str.compare(0, prefix.length(), prefix) == 0;
}

bool parse_hex_byte(const char* str, uint8_t* out)
{
  if (!str || !out)
    return false;

  // Skip whitespace
  while (*str && std::isspace(static_cast<unsigned char>(*str)))
    str++;

  if (!*str)
    return false;

  // Parse two hex digits
  char* endptr = nullptr;
  long val = strtol(str, &endptr, 16);

  // Check that exactly 1 or 2 hex digits were consumed
  if (endptr == str || (endptr - str) > 2)
    return false;

  // Check value range
  if (val < 0 || val > 255)
    return false;

  *out = static_cast<uint8_t>(val);
  return true;
}

std::vector<uint8_t> parse_hex_bytes(const std::string& hex_str)
{
  std::vector<uint8_t> bytes;
  std::istringstream iss(hex_str);
  std::string token;

  while (iss >> token)
  {
    // Skip comments (anything after #)
    if (token[0] == '#')
      break;

    uint8_t byte;
    if (parse_hex_byte(token.c_str(), &byte))
    {
      bytes.push_back(byte);
    }
    else
    {
      // Invalid hex byte - return empty to signal error
      return std::vector<uint8_t>();
    }
  }

  return bytes;
}

std::vector<KatTest> load_kat_file(const char* filename)
{
  std::vector<KatTest> tests;

  std::ifstream file(filename);
  if (!file.is_open())
  {
    return tests;  // Empty vector indicates error
  }

  std::string line;
  KatTest current_test;
  bool in_test = false;

  while (std::getline(file, line))
  {
    line = trim(line);

    // Skip empty lines
    if (line.empty())
      continue;

    // Skip comment lines (but not test headers)
    if (starts_with(line, "#") && !starts_with(line, "## Test:"))
      continue;

    // Test header
    if (starts_with(line, "## Test:"))
    {
      // Save previous test if exists
      if (in_test && !current_test.name.empty())
      {
        tests.push_back(current_test);
      }

      // Start new test
      current_test = KatTest();
      current_test.name = trim(line.substr(8));  // Skip "## Test:"
      in_test = true;
      continue;
    }

    // SOURCE line
    if (starts_with(line, "SOURCE:"))
    {
      if (!in_test)
        continue;  // SOURCE without test header - skip

      current_test.source = trim(line.substr(7));  // Skip "SOURCE:"
      continue;
    }

    // BYTECODE line
    if (starts_with(line, "BYTECODE:"))
    {
      if (!in_test)
        continue;  // BYTECODE without test header - skip

      std::string hex_str = trim(line.substr(9));  // Skip "BYTECODE:"
      current_test.expected_bytes = parse_hex_bytes(hex_str);

      if (current_test.expected_bytes.empty() && !hex_str.empty())
      {
        // Parse error - skip this test
        in_test = false;
        current_test = KatTest();
      }

      continue;
    }
  }

  // Save last test if exists
  if (in_test && !current_test.name.empty())
  {
    tests.push_back(current_test);
  }

  file.close();
  return tests;
}

}  // namespace kat
}  // namespace v4front
