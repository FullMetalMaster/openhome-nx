// Host-side tests (no Switch toolchain needed). Build line is in the README.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "box/boxstore.hpp"
#include "box/fsutil.hpp"
#include "pkm/pkm.hpp"
#include "pkm/species.hpp"
#include "save/gamedb.hpp"
#include "save/sdscan.hpp"

static void put16(std::vector<uint8_t>& b, size_t off, uint16_t v) { b[off] = v & 0xFF; b[off + 1] = v >> 8; }
static void putStr(std::vector<uint8_t>& b, size_t off, const char* s) {
    for (size_t i = 0; s[i]; i++) put16(b, off + i * 2, (uint8_t)s[i]);
}

int main() {
    // ---- pkm detect/parse ----
    assert(pkm::detect("pk9", 344) == pkm::Format::PK9);
    assert(pkm::detect("pk9", 100) == pkm::Format::Unknown);
    assert(pkm::detect("txt", 344) == pkm::Format::Unknown);
    assert(pkm::detect("pa8", 360) == pkm::Format::PA8);

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
    assert(std::strcmp(species::itemName(4), "Pok\xC3\xA9 Ball") == 0);

    std::vector<uint8_t> pk7(232, 0);
    put16(pk7, 0x08, 6);
    putStr(pk7, 0x40, "Zard");
    putStr(pk7, 0xB0, "Red");
    auto i7 = pkm::parse(pkm::Format::PK7, pk7.data(), pk7.size());
    assert(i7.valid && i7.species == 6 && pkm::utf16ToUtf8(i7.nickname) == "Zard");
    assert(!pkm::parse(pkm::Format::PK9, pk9.data(), 10).valid);  // truncated buffer
    assert(pkm::utf16ToUtf8(u"é") == "\xC3\xA9");

    // ---- slot prefix parsing ----
    std::string rest;
    assert(box::parseSlotPrefix("007_Charizard.pk9", &rest) == 7 && rest == "Charizard.pk9");
    assert(box::parseSlotPrefix("120_x.pk9") == -1);   // out of range (0..119)
    assert(box::parseSlotPrefix("Charizard.pk9") == -1);
    assert(box::parseSlotPrefix("12a_x.pk9") == -1);

    // ---- box store ----
    std::string root = "/tmp/oh_nx_test";
    system("rm -rf /tmp/oh_nx_test");
    box::Store st(root);
    assert(st.init());
    assert(st.bankCount() == 1);

    std::string p = st.add(0, 0, "../evil/mon.pk9", pk9.data(), pk9.size());  // traversal stripped
    assert(!p.empty() && p.find("..") == std::string::npos);
    std::string p2 = st.add(0, 0, "mon.pk9", pk9.data(), pk9.size());         // never overwrites
    assert(!p2.empty() && p2 != p);
    auto slots = st.load(0, 0);
    assert(slots.size() == 2 && slots[0].index == 0 && slots[1].index == 1 && slots[0].info.species == 3);

    // fixed positions: move to a specific empty slot, in another box
    assert(st.move(slots[0], 0, 1, 77));
    auto b1 = st.load(0, 1);
    assert(b1.size() == 1 && b1[0].index == 77);
    assert(!st.move(slots[1], 0, 1, 77));                // occupied -> refused
    assert(st.swap(b1[0], st.load(0, 0)[0]));            // swap across boxes
    assert(st.load(0, 1)[0].index == 77 && st.load(0, 0)[0].index == 1);

    // release moves to <root>/released, never deletes
    auto r = st.load(0, 0)[0];
    assert(st.release(r));
    assert(st.load(0, 0).empty());
    assert(fsu::exists(root + "/released"));

    // unprefixed files dropped in by hand get a slot and a rename
    assert(fsu::writeFile(root + "/boxes/Bank 1/Box 03/PIKA.pk9", pk9.data(), pk9.size()));
    auto b3 = st.load(0, 2);
    assert(b3.size() == 1 && b3[0].index == 0 && b3[0].fileName == "000_PIKA.pk9");

    // capacity
    for (int n = 0; n < box::kSlotsPerBox; n++)
        st.add(0, 3, "x" + std::to_string(n) + ".pk9", pk9.data(), pk9.size());
    assert((int)st.load(0, 3).size() == box::kSlotsPerBox);
    assert(st.add(0, 3, "overflow.pk9", pk9.data(), pk9.size()).empty());

    // banks
    assert(st.addBank() && st.bankCount() == 2);

    // ---- game db ----
    assert(save::games().size() == 12);
    for (const auto& g : save::games()) assert((g.titleId != 0 || g.match) && g.format != pkm::Format::Unknown);

    // ---- SD card save classification ----
    assert(!save::classifySave("Red.sav", 32768).empty());
    assert(!save::classifySave("Emerald.sav", 131072).empty());
    assert(!save::classifySave("Black.dsv", 524288).empty());
    assert(save::classifySave("notes.txt", 32768).empty());   // wrong extension
    assert(save::classifySave("odd.sav", 12345).empty());     // unknown size

    system("rm -rf /tmp/oh_nx_test");
    std::puts("all tests passed");
    return 0;
}
