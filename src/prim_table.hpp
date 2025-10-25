#pragma once

#include <v4/opcodes.h>

#include <array>
#include <cstdint>
#include <string_view>

// Primitive opcode table entry
struct PrimitiveEntry
{
  const char* name;
  uint8_t opcode;
};

// Table of all primitive operations
static constexpr PrimitiveEntry kPrimitiveTable[] = {
#define OP(name, val, kind) MAP_##kind(name, val)
#define MAP_PRIM(n, v) {#n, V4_OP_##n},
#define MAP_PRIM_SYS(n, v) {#n, V4_OP_##n},
#define MAP_CTRL(n, v)  // Skip control flow ops

#include <v4/opcodes.def>

#undef MAP_CTRL
#undef MAP_PRIM_SYS
#undef MAP_PRIM
#undef OP
};

// Look up a primitive by name
// Returns true if found, with opcode written to 'out'
inline bool lookup_primitive(std::string_view token, uint8_t& out)
{
  for (const auto& entry : kPrimitiveTable)
  {
    if (token == entry.name)
    {
      out = entry.opcode;
      return true;
    }
  }
  return false;
}