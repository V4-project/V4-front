#pragma once
#include <cstdint>
#include <string_view>
#include <array>
#include <v4/opcodes.h> // enum V4_OP_*

struct PrimEnt
{
  const char *name;
  uint8_t opcode;
};

static constexpr PrimEnt kPrim[] = {
#define OP(name, val, kind) MAP_##kind(name, val)
#define MAP_PRIM(n, v) {#n, V4_OP_##n},
#define MAP_PRIM_SYS(n, v) {#n, V4_OP_##n},
#define MAP_CTRL(n, v) /* skip */

#include <v4/opcodes.def>

#undef MAP_CTRL
#undef MAP_PRIM_SYS
#undef MAP_PRIM
#undef OP
};

inline bool prim_lookup(std::string_view tok, uint8_t &out)
{
  for (const auto &e : kPrim)
  {
    if (tok == e.name)
    {
      out = e.opcode;
      return true;
    }
  }
  return false;
}
