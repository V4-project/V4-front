#pragma once

#include <cstdint>
#include <string_view>
#include <v4/opcodes.hpp>

// Look up a primitive by name
// Returns true if found, with opcode written to 'out'
inline bool lookup_primitive(std::string_view token, uint8_t& out)
{
  for (const auto& entry : v4::kPrimitiveTable)
  {
    if (token == entry.name)
    {
      out = entry.opcode;
      return true;
    }
  }
  return false;
}