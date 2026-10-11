// Host-side tests (no Switch toolchain needed):  make -C tests   (or see README)
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>

#include "box/boxstore.hpp"
#include "pkm/pkm.hpp"
#include "pkm/species.hpp"
#include "save/gamedb.hpp"

namespace fs = std::filesystem;

static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) { b[off] = v & 0xFF; b[off + 1] = v >> 8; }
static void putStr(std::vector<uint8_t>& b, size_t off, const char* s) {
    for (size_t i = 0; s[i]; i++) put16(b, off + i * 2, (uint8_t)s[i]);
}

int main() {
    // detect()
    assert(pkm::detect("pk9", 344) == pkm::Format::PK9);
    assert(pkm::detect("pk9", 100) == pkm::Format::Unknown);   // too small
    assert(pkm::detect("txt", 344) == pkm::Format::Unknown);
    assert(pkm::detect("pa8", 360) == pkm::Format::PA8);

    // parse() PK9: species at 0x08, nickname 0x58, OT 0xF8
    std::vector<uint8_t> pk9(344, 0);
    put16(pk9, 0x08, 3);
    putStr(pk9, 0x58, "Venu");
    putStr(pk9, 0xF8, "Ash");
    auto i9 = pkm::parse(pkm::Format::PK9, pk9.data(), pk9.size());
    assert(i9.valid && i9.species == 3);
    assert(pkm::utf16ToUtf8(i9.nickname) == "Venu");
    assert(pkm::utf16ToUtf8(i9.otName) == "Ash");
    assert(std::strcmp(species::name(3), "Venusaur") == 0);
    assert(std::strcmp(species::name(1025), "Pecharunt") == 0);
    assert(std::strcmp(species::name(5000), "???") == 0);
    assert(std::strcmp(species::moveName(1), "Pound") == 0);
    assert(std::strcmp(species::abilityName(1), "Stench") == 0);
    assert(std::strcmp(species::itemName(1), "Master Ball") == 0);
    assert(std::strcmp(species::itemName(4), "Pok\xC3\xA9 Ball") == 0);   // UTF-8 preserved

    // parse() PK7: nickname at 0x40, OT at 0xB0
    std::vector<uint8_t> pk7(232, 0);
    put16(pk7, 0x08, 6);
    putStr(pk7, 0x40, "Zard");
    putStr(pk7, 0xB0, "Red");
    auto i7 = pkm::parse(pkm::Format::PK7, pk7.data(), pk7.size());
    assert(i7.valid && i7.species == 6 && pkm::utf16ToUtf8(i7.nickname) == "Zard");

    // truncated buffers never crash / read OOB
    auto bad = pkm::parse(pkm::Format::PK9, pk9.data(), 10);
    assert(!bad.valid);

    // UTF-8 conversion of non-ASCII
    assert(pkm::utf16ToUtf8(u"é") == "\xC3\xA9");

    // box store round trip
    std::string root = (fs::temp_directory_path() / "oh_nx_test").string();
    fs::remove_all(root);
    box::Store st(root);
    assert(st.init());
    std::string p = st.add(0, "../evil/mon.pk9", pk9.data(), pk9.size());   // traversal stripped
    assert(!p.empty() && p.find("..") == std::string::npos);
    std::string p2 = st.add(0, "mon.pk9", pk9.data(), pk9.size());          // no overwrite
    assert(!p2.empty() && p2 != p);
    auto slots = st.load(0);
    assert(slots.size() == 2 && slots[0].info.species == 3);
    assert(st.move(slots[0], 1));
    assert(st.load(0).size() == 1 && st.load(1).size() == 1);
    assert(st.remove(st.load(1)[0]));
    fs::remove_all(root);

    // box capacity
    assert(st.init());
    for (int n = 0; n < box::kSlotsPerBox; n++) st.add(2, "x" + std::to_string(n) + ".pk9", pk9.data(), pk9.size());
    assert(st.add(2, "overflow.pk9", pk9.data(), pk9.size()).empty());
    fs::remove_all(root);

    // game db sanity
    assert(save::games().size() == 9);
    for (const auto& g : save::games()) assert(g.titleId != 0 && g.format != pkm::Format::Unknown);

    std::puts("all tests passed");
    return 0;
}
