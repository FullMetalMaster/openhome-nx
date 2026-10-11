#include "gamedb.hpp"

namespace save {

const std::vector<Game>& games() {
    static const std::vector<Game> db = {
        {"Let's Go, Pikachu!",     0x010003F003A34000ULL, pkm::Format::PB7, "main",         Crypto::None,      true},
        {"Let's Go, Eevee!",       0x0100187003A36000ULL, pkm::Format::PB7, "main",         Crypto::None,      true},
        {"Sword",                  0x0100ABF008968000ULL, pkm::Format::PK8, "main",         Crypto::SCBlock,   true},
        {"Shield",                 0x01008DB008C2C000ULL, pkm::Format::PK8, "main",         Crypto::SCBlock,   true},
        {"Brilliant Diamond",      0x0100000011D90000ULL, pkm::Format::PB8, "SaveData.bin", Crypto::BDSPPlain, true},
        {"Shining Pearl",          0x010018E011D92000ULL, pkm::Format::PB8, "SaveData.bin", Crypto::BDSPPlain, true},
        {"Legends: Arceus",        0x01001F5010DFA000ULL, pkm::Format::PA8, "main",         Crypto::SCBlock,   true},
        {"Scarlet",                0x0100A3D008C5C000ULL, pkm::Format::PK9, "main",         Crypto::SCBlock,   true},
        {"Violet",                 0x01008F6008C5E000ULL, pkm::Format::PK9, "main",         Crypto::SCBlock,   true},
    };
    return db;
}

}  // namespace save
