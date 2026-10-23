// BDSP reader test. REAL: two PKHeX sample .pb8 files (crypto round trip, species). SYNTHETIC: the save is
// built here with PKHeX's SAV8BS offsets and MD5 rule, so it proves agreement with my reading of PKHeX only.
//   g++ -std=c++17 -Isource -Itests -o /tmp/tbd tests/test_bdsp.cpp source/pkm/*.cpp source/box/fsutil.cpp source/save/savefile.cpp source/save/hash.cpp source/save/bdsp.cpp && /tmp/tbd tests/data/bdsp
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "box/fsutil.hpp"
#include "pkm/pkmcrypto.hpp"
#include "save/hash.hpp"
#include "save/savefile.hpp"
#define OH_REAL_BDSP
#include "stub_openers.hpp"

using Bytes = std::vector<uint8_t>;
static int g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data/bdsp";
    Bytes tox, mes;
    CHECK(fsu::readFile(dir + "/toxicroak.pb8", tox) && fsu::readFile(dir + "/mesprit.pb8", mes));
    tox.resize(328); mes.resize(328);
    CHECK(pkm::rd16(tox.data() + 8) == 454 && pkm::rd16(mes.data() + 8) == 481);   // real files: species
    for (Bytes* m : {&tox, &mes}) {                                                 // real files: crypto round trip
        Bytes e = *m;
        CHECK(pkm::crypto::encryptBlocks(e.data(), e.size(), 80));
        CHECK(e != *m);
        CHECK(pkm::crypto::decryptBlocks(e.data(), e.size(), 80));
        CHECK(e == *m);
    }

    Bytes img(0xEF0A4, 0);
    pkm::wr32(img.data(), 0x34);                       // revision 1.3
    const size_t kBox = 0x14EF4, kStride = 344, kStatus = 0x79BB4, kParty = 0x14098, kNames = 0x148AA;
    img[kStatus + 0x2B] = 1;                           // Shining Pearl
    const char* ot = "Lucas";
    for (int i = 0; ot[i]; i++) pkm::wr16(img.data() + kStatus + 2 * i, (uint16_t)ot[i]);
    auto place = [&](size_t off, const Bytes& m) {
        Bytes e = m;
        pkm::crypto::encryptBlocks(e.data(), e.size(), 80);
        std::memcpy(img.data() + off, e.data(), 328);
    };
    place(kBox, tox);                                              // box 0 slot 0
    place(kBox + (39 * 30 + 29) * kStride, mes);                   // last slot of the last box
    place(kParty, mes);
    img[kParty + 6 * kStride] = 1;
    const char* bn = "Fav";
    for (int i = 0; bn[i]; i++) pkm::wr16(img.data() + kNames + 0x22 * 3 + 2 * i, (uint16_t)bn[i]);
    uint8_t h[16];
    save::hash::md5(img.data(), img.size(), h);       // hash bytes are zero at this point
    std::memcpy(img.data() + 0xE9818, h, 16);

    std::string err;
    auto sv = save::open(img, "SaveData.bin", nullptr, err);
    CHECK(sv);
    CHECK(sv->format() == pkm::Format::PB8);
    CHECK(sv->title() == "Shining Pearl");
    CHECK(sv->trainer() == "Lucas");
    CHECK(sv->boxCount() == 40 && sv->boxSlots() == 30);
    CHECK(sv->boxName(3) == "Fav" && sv->boxName(0) == "Box 1");
    CHECK(sv->notes().find("checksum OK") != std::string::npos);
    Bytes out;
    CHECK(sv->readBox(0, 0, out) && out == tox);
    CHECK(sv->readBox(39, 29, out) && out == mes);
    CHECK(!sv->readBox(0, 1, out));
    CHECK(!sv->readBox(40, 0, out) && !sv->readBox(0, 30, out) && !sv->readBox(-1, 0, out));
    CHECK(sv->partyCount() == 1 && sv->readParty(0, out) && out == mes && !sv->readParty(1, out));

    Bytes bad = img;
    bad[1000] ^= 1;                                    // hash no longer matches: still readable, but flagged
    auto sb = save::open(bad, "SaveData.bin", nullptr, err);
    CHECK(sb && sb->notes().find("does not match") != std::string::npos);

    Bytes bd = img;
    bd[kStatus + 0x2B] = 0;
    pkm::wr32(bd.data(), 0x34);
    std::memset(bd.data() + 0xE9818, 0, 16);
    save::hash::md5(bd.data(), bd.size(), h);
    std::memcpy(bd.data() + 0xE9818, h, 16);
    auto sd = save::open(bd, "x", nullptr, err);
    CHECK(sd && sd->title() == "Brilliant Diamond");

    srand(7);                                          // garbage and damaged saves never crash
    for (int n = 0; n < 100; n++) {
        Bytes g((size_t)(rand() % 0x20000));
        for (auto& c : g) c = (uint8_t)rand();
        CHECK(!save::open(g, "junk", nullptr, err));
    }
    Bytes trunc(img.begin(), img.begin() + 0x80000);
    CHECK(!save::open(trunc, "t", nullptr, err));
    for (int n = 0; n < 100; n++) {
        Bytes c = img;
        for (int k = 0; k < 50; k++) c[(size_t)(rand() % c.size())] = (uint8_t)rand();
        auto r = save::open(c, "c", nullptr, err);
        if (r) for (int b = 0; b < 40; b++) for (int s = 0; s < 30; s++) r->readBox(b, s, out);
    }
    std::printf("bdsp tests passed (%d checks, 2 real .pb8 files)\n", g_checks);
    return 0;
}
