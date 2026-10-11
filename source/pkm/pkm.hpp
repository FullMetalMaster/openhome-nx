// PKM file detection and core-field reading.
// Operates on raw .pk* files (decrypted "box"/stored format), the same files
// desktop OpenHome and PKHeX import/export, so boxes stay interchangeable.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pkm {

enum class Format {
    Unknown,
    PK3,  // Gen 3  (.pk3)  80-byte stored, 100 party
    PK4,  // Gen 4  (.pk4)  136 / 236
    PK5,  // Gen 5  (.pk5)  136 / 220
    PK6,  // Gen 6  (.pk6)  232 / 260
    PK7,  // Gen 7  (.pk7)  232 / 260
    PB7,  // Let's Go (.pb7) 260
    PK8,  // Sword/Shield (.pk8) 328
    PA8,  // Legends: Arceus (.pa8) 360
    PB8,  // Brilliant Diamond / Shining Pearl (.pb8) 328 (verify vs PKHeX)
    PK9,  // Scarlet/Violet (.pk9) 344
};

struct Info {
    Format format = Format::Unknown;
    uint16_t species = 0;       // National Dex number (0 if not readable)
    uint16_t form = 0;
    std::u16string nickname;    // may be empty when not readable
    std::u16string otName;
    bool valid = false;
};

const char* formatName(Format f);
const char* formatExt(Format f);
size_t formatStoredSize(Format f);

// Detect from file extension (lowercase, no dot) and byte length.
Format detect(const std::string& extension, size_t size);

// Parse core fields from a raw buffer. Never reads out of bounds.
Info parse(Format f, const uint8_t* data, size_t size);

// Little-endian helpers (public so tests/other modules can reuse them).
uint16_t rd16(const uint8_t* p);
uint32_t rd32(const uint8_t* p);

std::string utf16ToUtf8(const std::u16string& s);

}  // namespace pkm
