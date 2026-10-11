// Switch Pokemon games supported by OpenHome-NX and how their saves are laid out.
// Host-safe (no libnx includes) so it can be unit tested.
#pragma once
#include <cstdint>
#include <vector>

#include "pkm/pkm.hpp"

namespace save {

enum class Crypto {
    None,        // plain binary (Let's Go)
    SCBlock,     // SCBlock container, swish-encrypted (Sword/Shield, Legends: Arceus, Scarlet/Violet)
    BDSPPlain,   // BDSP SaveData.bin (plain-ish; check PKHeX SAV8BS)
};

struct Game {
    const char* name;
    uint64_t titleId;
    pkm::Format format;      // native PKM format for this game
    const char* mainFile;    // primary save file inside the mounted save dir
    Crypto crypto;
    bool verified;           // title ID cross-checked against community lists by the author
};

// Title IDs are the base-game application IDs. Double check against your own console
// (e.g. via JKSV / the title list) before relying on them - regional editions share IDs, but
// please confirm. Legends: Z-A is deliberately NOT listed yet: add it once you have confirmed its
// title ID and save layout.
const std::vector<Game>& games();

}  // namespace save
