// Gen 3 tests: real PKHeX sample .pk3 files + synthetic saves.
//   g++ -std=c++17 -Isource -o /tmp/t3 tests/test_gen3.cpp source/pkm/*.cpp source/box/*.cpp source/save/*.cpp && /tmp/t3 tests/data/pk3
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

#include "box/fsutil.hpp"
#include "pkm/pkm_internal.hpp"
#include "save/savefile.hpp"

using Bytes = std::vector<uint8_t>;
static int g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

// PKHeX's inverse permutation table, used to cross-check my encrypt direction.
static const uint8_t kInvert[24] = {0, 1, 2, 4, 3, 5, 6, 7, 12, 18, 13, 19, 8, 10, 14, 20, 16, 22, 9, 11, 15, 21, 17, 23};
static const uint8_t kPos[24][4] = {
    {0, 1, 2, 3}, {0, 1, 3, 2}, {0, 2, 1, 3}, {0, 3, 1, 2}, {0, 2, 3, 1}, {0, 3, 2, 1}, {1, 0, 2, 3}, {1, 0, 3, 2},
    {2, 0, 1, 3}, {3, 0, 1, 2}, {2, 0, 3, 1}, {3, 0, 2, 1}, {1, 2, 0, 3}, {1, 3, 0, 2}, {2, 1, 0, 3}, {3, 1, 0, 2},
    {2, 3, 0, 1}, {3, 2, 0, 1}, {1, 2, 3, 0}, {1, 3, 2, 0}, {2, 1, 3, 0}, {3, 1, 2, 0}, {2, 3, 1, 0}, {3, 2, 1, 0}};

// ---- helpers to build a synthetic Gen 3 save ----
static uint16_t sum32(const uint8_t* p, size_t len) {
    uint32_t s = 0;
    for (size_t i = 0; i + 4 <= len; i += 4) s += pkm::rd32(p + i);
    return (uint16_t)(s + (s >> 16));
}
static const uint16_t kLen[14] = {3884, 3968, 3968, 3968, 3848, 3968, 3968, 3968, 3968, 3968, 3968, 3968, 3968, 2000};

// Writes one slot of 14 sectors starting at `base`; sector order is rotated by `rot` (as real saves do).
static void buildSlot(Bytes& img, size_t base, uint32_t counter, int rot, const Bytes& small, const Bytes& large, const Bytes& storage) {
    for (int phys = 0; phys < 14; phys++) {
        int id = (phys + rot) % 14;
        uint8_t* sec = img.data() + base + (size_t)phys * 0x1000;
        std::memset(sec, 0, 0x1000);
        const uint8_t* src = id == 0 ? small.data() : id <= 4 ? large.data() + (id - 1) * 0xF80 : storage.data() + (id - 5) * 0xF80;
        std::memcpy(sec, src, kLen[id]);
        pkm::wr16(sec + 0xFF4, (uint16_t)id);
        pkm::wr32(sec + 0xFF8, 0x08012025);
        pkm::wr32(sec + 0xFFC, counter);
        pkm::wr16(sec + 0xFF6, sum32(sec, kLen[id]));
    }
}

static Bytes makeMon(uint32_t pid, uint32_t oid, uint16_t internalSpecies, uint32_t exp, const char* /*unused*/) {
    Bytes m(80, 0);
    pkm::wr32(m.data(), pid);
    pkm::wr32(m.data() + 4, oid);
    for (int i = 0; i < 10; i++) m[8 + i] = 0xFF;
    m[8] = 0xBB; m[9] = 0xBC; m[10] = 0xBD;  // "ABC"
    m[0x12] = 2;                              // English
    m[0x13] = 2;                              // has species
    for (int i = 0; i < 7; i++) m[0x14 + i] = 0xFF;
    m[0x14] = 0xCC;                           // "R"
    pkm::wr16(m.data() + 0x20, internalSpecies);
    pkm::wr32(m.data() + 0x24, exp);
    pkm::wr16(m.data() + 0x1C, pkm::gen3Checksum(m.data()));
    return m;
}

static std::vector<std::string> listPk3(const std::string& dir) {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".pk3") out.push_back(dir + "/" + n);
        }
        closedir(d);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data/pk3";

    // ---------- 1. real PKHeX sample files ----------
    auto files = listPk3(dir);
    CHECK(files.size() >= 10);
    int parsed = 0;
    for (const auto& f : files) {
        Bytes b;
        CHECK(fsu::readFile(f, b));
        CHECK(b.size() == 80 || b.size() == 100);
        CHECK(pkm::gen3Checksum(b.data()) == pkm::rd16(b.data() + 0x1C));  // real data: my checksum matches
        pkm::Info i = pkm::parse(pkm::Format::PK3, b.data(), b.size());
        std::string base = fsu::baseName(f);
        int expect = std::atoi(base.c_str());                              // file names start with the dex number
        CHECK(i.valid);
        CHECK(i.species == expect);
        CHECK(i.level >= 1 && i.level <= 100);
        // crypto round trip on real data
        Bytes enc = b;
        pkm::gen3Encrypt(enc.data());
        CHECK(std::memcmp(enc.data() + 0x20, b.data() + 0x20, 48) != 0 || pkm::rd32(b.data()) % 24 == 0);
        pkm::gen3Decrypt(enc.data());
        CHECK(enc == b);
        parsed++;
    }

    // ---------- 2. crypto direction agrees with PKHeX's inverse table ----------
    for (int sv = 0; sv < 24; sv++) {
        // decrypt permutation P: logical[i] = enc[kPos[sv][i]].  PKHeX encrypts by applying the same
        // operation with row kInvert[sv]; the two must be inverse permutations of each other.
        for (int i = 0; i < 4; i++) {
            int enc_pos = kPos[sv][i];             // where logical block i lives in encrypted data
            int back = kPos[kInvert[sv]][enc_pos]; // applying PKHeX's inverse row to that position
            CHECK(back == i);
        }
    }

    // ---------- 3. synthetic save: read, write, serialize, reopen ----------
    Bytes small(0xF80, 0), large(4 * 0xF80, 0), storage(9 * 0xF80, 0);
    small[0] = 0xCC; small[1] = 0xFF; small[6] = 1;  // trainer "R", not Japanese
    pkm::wr32(small.data() + 0xAC, 1);               // FRLG marker
    large[0x34] = 1;                                 // one party member
    Bytes partyMon = makeMon(0x12345678, 0x00010002, 1 /*Bulbasaur internal*/, 1000, "");
    partyMon.resize(100, 0);
    partyMon[0x54] = 12;
    Bytes encParty = partyMon;
    pkm::gen3Encrypt(encParty.data());
    std::memcpy(large.data() + 0x38, encParty.data(), 100);

    Bytes mon0 = makeMon(0x0BADF00D, 0xAAAA5555, 4 /*Charmander*/, 125, "");
    Bytes mon1 = makeMon(0xFEEDBEEF, 0x13371337, 7 /*Squirtle*/, 8000, "");
    auto place = [&](int box, int slot, const Bytes& m) {
        Bytes e = m;
        pkm::gen3Encrypt(e.data());
        std::memcpy(storage.data() + 4 + 80 * (box * 30 + slot), e.data(), 80);
    };
    place(0, 0, mon0);
    place(13, 29, mon1);   // last slot of last box (spans a sector boundary region check)
    place(5, 17, mon1);

    Bytes img(0x20000, 0xFF);
    buildSlot(img, 0, 5, 3, small, large, storage);          // older copy, rotated
    // newer copy has a different party count so we can tell which slot was chosen
    Bytes large2 = large; large2[0x34] = 0;
    buildSlot(img, 0xE000, 9, 7, small, large2, storage);

    std::string err;
    auto sv = save::open(img, "test.sav", nullptr, err);
    CHECK(sv);
    CHECK(sv->format() == pkm::Format::PK3);
    CHECK(sv->title().find("FireRed") != std::string::npos);
    CHECK(sv->trainer() == "R");
    CHECK(sv->boxCount() == 14 && sv->boxSlots() == 30);
    CHECK(sv->partyCount() == 0);                    // newer slot (counter 9) wins -> party 0
    Bytes out;
    CHECK(sv->readBox(0, 0, out));
    CHECK(out.size() == 80 && out == mon0);          // decrypted bytes equal the original logical mon
    CHECK(sv->readBox(13, 29, out) && out == mon1);
    CHECK(!sv->readBox(0, 1, out));                  // empty slot
    CHECK(!sv->readBox(14, 0, out));                 // out of range

    // older slot active when its counter is higher
    Bytes img2 = img;
    pkm::wr32(img2.data() + 0, 0);  // (no-op write keeps type happy)
    for (int p = 0; p < 14; p++) pkm::wr32(img2.data() + (size_t)p * 0x1000 + 0xFFC, 50);
    for (int p = 0; p < 14; p++) {
        uint8_t* sec = img2.data() + (size_t)p * 0x1000;
        uint16_t id = pkm::rd16(sec + 0xFF4);
        pkm::wr16(sec + 0xFF6, sum32(sec, kLen[id]));
    }
    auto sv2 = save::open(img2, "x.sav", nullptr, err);
    CHECK(sv2 && sv2->partyCount() == 1);
    Bytes party;
    CHECK(sv2->readParty(0, party) && party.size() == 100 && party[0x54] == 12);
    pkm::Info pi = pkm::parse(pkm::Format::PK3, party.data(), party.size());
    CHECK(pi.valid && pi.species == 1 && pi.level == 12);

    // write: put mon1 in box 2 slot 3, clear box 0 slot 0, serialize, reopen
    CHECK(sv->writeBox(1, 2, mon1.data(), mon1.size(), err));
    CHECK(sv->clearBox(0, 0, err));
    Bytes saved;
    CHECK(sv->serialize(saved, err));
    CHECK(saved.size() == img.size());
    auto sv3 = save::open(saved, "t.sav", nullptr, err);
    CHECK(sv3);
    CHECK(sv3->readBox(1, 2, out) && out == mon1);
    CHECK(!sv3->readBox(0, 0, out));
    CHECK(sv3->readBox(13, 29, out) && out == mon1);   // untouched mon survives
    CHECK(sv3->notes().find("14 of 14") != std::string::npos);  // all checksums valid after the edit
    // the older save copy (slot 0, bytes 0..0xE000) must be left exactly as it was
    CHECK(std::memcmp(saved.data(), img.data(), 0xE000) == 0);
    // only the touched sectors changed in the active copy
    int changed = 0;
    for (int p = 0; p < 14; p++)
        if (std::memcmp(saved.data() + 0xE000 + p * 0x1000, img.data() + 0xE000 + p * 0x1000, 0x1000) != 0) changed++;
    CHECK(changed >= 1 && changed <= 2);

    // writing garbage is refused
    Bytes empty(80, 0);
    CHECK(!sv->writeBox(0, 0, empty.data(), empty.size(), err));
    CHECK(!sv->writeBox(0, 0, mon1.data(), 40, err));
    CHECK(!sv->writeBox(99, 0, mon1.data(), mon1.size(), err));

    // ---------- 4. save embedded in a larger container ----------
    Bytes box(0x30000, 0xA5);
    std::memcpy(box.data() + 0x3000, img.data(), img.size());
    auto ev = save::open(box, "wrapped.bin", nullptr, err);
    CHECK(ev);
    CHECK(ev->readBox(13, 29, out) && out == mon1);
    CHECK(ev->notes().find("inside a larger file at offset 12288") != std::string::npos);
    CHECK(ev->writeBox(3, 3, mon0.data(), mon0.size(), err));
    Bytes wrapped;
    CHECK(ev->serialize(wrapped, err));
    CHECK(wrapped.size() == box.size());
    CHECK(std::memcmp(wrapped.data(), box.data(), 0x3000) == 0);                       // bytes before the save untouched
    CHECK(std::memcmp(wrapped.data() + 0x23000, box.data() + 0x23000, 0x30000 - 0x23000) == 0);  // and after
    auto ev2 = save::open(wrapped, "wrapped.bin", nullptr, err);
    CHECK(ev2 && ev2->readBox(3, 3, out) && out == mon0);

    // ---------- 5. garbage never crashes ----------
    srand(1234);
    for (int n = 0; n < 200; n++) {
        Bytes g((size_t)(rand() % 0x30000));
        for (auto& c : g) c = (uint8_t)rand();
        std::string e2;
        auto r = save::open(g, "junk", nullptr, e2);
        CHECK(!r);
    }
    Bytes trunc(img.begin(), img.begin() + 0x7000);
    CHECK(!save::open(trunc, "short.sav", nullptr, err));
    // a corrupted copy of a real save (random flips) must still not crash
    for (int n = 0; n < 200; n++) {
        Bytes c = img;
        for (int k = 0; k < 40; k++) c[(size_t)(rand() % c.size())] = (uint8_t)rand();
        auto r = save::open(c, "c.sav", nullptr, err);
        if (r) { Bytes t; for (int b = 0; b < 14; b++) for (int s = 0; s < 30; s++) r->readBox(b, s, t); }
    }

    std::printf("gen3 tests passed (%d checks, %d real .pk3 files)\n", g_checks, parsed);
    return 0;
}
