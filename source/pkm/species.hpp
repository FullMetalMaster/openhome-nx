// Name lookups backed by the generated tables in data_tables.cpp (tools/gen_data.py).
// All return "???" for out-of-range IDs.
#pragma once
#include <cstdint>

namespace species {
const char* name(uint16_t nationalDex);   // 0 = "Egg"
uint16_t count();
const char* moveName(uint16_t id);
const char* abilityName(uint16_t id);
const char* itemName(uint16_t id);
}  // namespace species
