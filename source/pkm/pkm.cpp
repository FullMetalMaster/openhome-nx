#include "pkm.hpp"

#include <algorithm>

namespace pkm {

uint16_t rd16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t rd32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

const char* formatName(Format f) {
    switch (f) {
        case Format::PK3: return "PK3";
        case Format::PK4: return "PK4";
        case Format::PK5: return "PK5";
        case Format::PK6: return "PK6";
        case Format::PK7: return "PK7";
        case Format::PB7: return "PB7";
        case Format::PK8: return "PK8";
        case Format::PA8: return "PA8";
        case Format::PB8: return "PB8";
        case Format::PK9: return "PK9";
        default: return "???";
    }
}

const char* formatExt(Format f) {
    switch (f) {
        case Format::PK3: return "pk3";
        case Format::PK4: return "pk4";
        case Format::PK5: return "pk5";
        case Format::PK6: return "pk6";
        case Format::PK7: return "pk7";
        case Format::PB7: return "pb7";
        case Format::PK8: return "pk8";
        case Format::PA8: return "pa8";
        case Format::PB8: return "pb8";
        case Format::PK9: return "pk9";
        default: return "bin";
    }
}

size_t formatStoredSize(Format f) {
    switch (f) {
        case Format::PK3: return 80;
        case Format::PK4: case Format::PK5: return 136;
        case Format::PK6: case Format::PK7: return 232;
        case Format::PB7: return 232;  // stored size; files may carry 260
        case Format::PK8: return 328;
        case Format::PA8: return 360;
        case Format::PB8: return 328;
        case Format::PK9: return 344;
        default: return 0;
    }
}

Format detect(const std::string& ext, size_t size) {
    struct Row { const char* ext; Format f; size_t minSize; };
    static const Row rows[] = {
        {"pk3", Format::PK3, 80}, {"pk4", Format::PK4, 136}, {"pk5", Format::PK5, 136},
        {"pk6", Format::PK6, 232}, {"pk7", Format::PK7, 232}, {"pb7", Format::PB7, 232},
        {"pk8", Format::PK8, 328}, {"pa8", Format::PA8, 360}, {"pb8", Format::PB8, 328},
        {"pk9", Format::PK9, 344},
    };
    for (const auto& r : rows)
        if (ext == r.ext) return size >= r.minSize ? r.f : Format::Unknown;
    return Format::Unknown;
}

static std::u16string readStr(const uint8_t* data, size_t size, size_t off, size_t chars) {
    std::u16string s;
    if (off + chars * 2 > size) return s;
    for (size_t i = 0; i < chars; i++) {
        uint16_t c = rd16(data + off + i * 2);
        if (c == 0) break;
        s.push_back((char16_t)c);
    }
    return s;
}

// Offsets follow the PKHeX-documented layouts for decrypted stored data.
// Gen 3 stored data is substructure-shuffled and Gen 4/5 blocks are shuffled
// when encrypted; .pk4/.pk5 files exported by tools are already decrypted/unshuffled.
Info parse(Format f, const uint8_t* d, size_t size) {
    Info info;
    info.format = f;
    size_t need = formatStoredSize(f);
    if (f == Format::Unknown || size < need) return info;
    info.valid = true;

    switch (f) {
        case Format::PK3:
            // Encrypted & shuffled; only the format is reported until a decryptor lands.
            break;
        case Format::PK4:
        case Format::PK5:
            info.species = rd16(d + 0x08);
            info.nickname = readStr(d, size, 0x48, 11);
            info.otName = readStr(d, size, 0x68, 8);
            break;
        case Format::PK6:
        case Format::PK7:
        case Format::PB7:
            info.species = rd16(d + 0x08);
            info.nickname = readStr(d, size, 0x40, 12);
            info.otName = readStr(d, size, 0xB0, 12);
            break;
        case Format::PK8:
        case Format::PA8:
        case Format::PB8:
        case Format::PK9:
            info.species = rd16(d + 0x08);
            info.nickname = readStr(d, size, 0x58, 12);
            info.otName = readStr(d, size, 0xF8, 12);
            break;
        default: break;
    }
    // Reject obviously bogus species (blank slot or unreadable).
    if (info.species > 1100) { info.species = 0; info.valid = false; }
    return info;
}

std::string utf16ToUtf8(const std::u16string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        uint32_t c = s[i];
        if (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.size()) {
            uint32_t lo = s[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) { c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00); i++; }
        }
        if (c < 0x80) out.push_back((char)c);
        else if (c < 0x800) { out.push_back((char)(0xC0 | (c >> 6))); out.push_back((char)(0x80 | (c & 0x3F))); }
        else if (c < 0x10000) { out.push_back((char)(0xE0 | (c >> 12))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
        else { out.push_back((char)(0xF0 | (c >> 18))); out.push_back((char)(0x80 | ((c >> 12) & 0x3F))); out.push_back((char)(0x80 | ((c >> 6) & 0x3F))); out.push_back((char)(0x80 | (c & 0x3F))); }
    }
    return out;
}

}  // namespace pkm
