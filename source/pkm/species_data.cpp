#include "species.hpp"

#include "data.h"

namespace species {
static const char* lookup(const char* const* t, unsigned n, unsigned i) { return i < n ? t[i] : "???"; }
uint16_t count() { return (uint16_t)ohnx::kSpeciesNamesCount; }
const char* name(uint16_t n) { return lookup(ohnx::kSpeciesNames, ohnx::kSpeciesNamesCount, n); }
const char* moveName(uint16_t i) { return lookup(ohnx::kMoveNames, ohnx::kMoveNamesCount, i); }
const char* abilityName(uint16_t i) { return lookup(ohnx::kAbilityNames, ohnx::kAbilityNamesCount, i); }
const char* itemName(uint16_t i) { return lookup(ohnx::kItemNames, ohnx::kItemNamesCount, i); }
}  // namespace species
