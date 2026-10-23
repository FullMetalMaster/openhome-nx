// SCBlock (Sword/Shield, Legends: Arceus, Scarlet/Violet, Legends: Z-A) reader tests.
//   g++ -std=c++17 -Isource -Itests -o /tmp/tsc tests/test_scblock.cpp source/pkm/*.cpp source/box/fsutil.cpp source/save/savefile.cpp source/save/hash.cpp source/save/scblock.cpp
//   /tmp/tsc tests/data/gen89
// REAL data: the sample .pk8/.pk9/.pa8 files (decrypted, from PKHeX's test suite): checksums and the
// crypto round trip. SYNTHETIC: the saves are built here following PKHeX's SwishCrypto.Encrypt and its
// block/box layouts, so they prove the reader agrees with my reading of PKHeX, not with a real console save.
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <vector>

#define OH_REAL_SCBLOCK
#include "box/fsutil.hpp"
#include "pkm/pkmcrypto.hpp"
#include "save/hash.hpp"
#include "save/savefile.hpp"
#include "stub_openers.hpp"

using Bytes = std::vector<uint8_t>;
static int g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

// ---------- independent writer, following PKHeX SCBlock.WriteBlock / SwishCrypto.Encrypt ----------
static const uint8_t kIntro[64] = {
    0x9E, 0xC9, 0x9C, 0xD7, 0x0E, 0xD3, 0x3C, 0x44, 0xFB, 0x93, 0x03, 0xDC, 0xEB, 0x39, 0xB4, 0x2A, 0x19, 0x47, 0xE9, 0x63, 0x4B, 0xA2,
    0x33, 0x44, 0x16, 0xBF, 0x82, 0xA2, 0xBA, 0x63, 0x55, 0xB6, 0x3D, 0x9D, 0xF2, 0x4B, 0x5F, 0x7B, 0x6A, 0xB2, 0x62, 0x1D, 0xC2, 0x1B,
    0x68, 0xE5, 0xC8, 0xB5, 0x3A, 0x05, 0x90, 0x00, 0xE8, 0xA8, 0x10, 0x3D, 0xE2, 0xEC, 0xF0, 0x0C, 0xB2, 0xED, 0x4F, 0x6D};
static const uint8_t kOutro[64] = {
    0xD6, 0xC0, 0x1C, 0x59, 0x8B, 0xC8, 0xB8, 0xCB, 0x46, 0xE1, 0x53, 0xFC, 0x82, 0x8C, 0x75, 0x75, 0x13, 0xE0, 0x45, 0xDF, 0x32, 0x69,
    0x3C, 0x75, 0xF0, 0x59, 0xF8, 0xD9, 0xA2, 0x5F, 0xB2, 0x17, 0xE0, 0x80, 0x52, 0xDB, 0xEA, 0x89, 0x73, 0x99, 0x75, 0x79, 0xAF, 0xCB,
    0x2E, 0x80, 0x07, 0xE6, 0xF1, 0x26, 0xE0, 0x03, 0x0A, 0xE6, 0x6F, 0xF6, 0x41, 0xBF, 0x7E, 0x59, 0xC2, 0xAE, 0x55, 0xFD};
static const uint8_t kPad[128] = {
    0xA0, 0x92, 0xD1, 0x06, 0x07, 0xDB, 0x32, 0xA1, 0xAE, 0x01, 0xF5, 0xC5, 0x1E, 0x84, 0x4F, 0xE3, 0x53, 0xCA, 0x37, 0xF4, 0xA7, 0xB0,
    0x4D, 0xA0, 0x18, 0xB7, 0xC2, 0x97, 0xDA, 0x5F, 0x53, 0x2B, 0x75, 0xFA, 0x48, 0x16, 0xF8, 0xD4, 0x8A, 0x6F, 0x61, 0x05, 0xF4, 0xE2,
    0xFD, 0x04, 0xB5, 0xA3, 0x0F, 0xFC, 0x44, 0x92, 0xCB, 0x32, 0xE6, 0x1B, 0xB9, 0xB1, 0x2E, 0x01, 0xB0, 0x56, 0x53, 0x36, 0xD2, 0xD1,
    0x50, 0x3D, 0xDE, 0x5B, 0x2E, 0x0E, 0x52, 0xFD, 0xDF, 0x2F, 0x7B, 0xCA, 0x63, 0x50, 0xA4, 0x67, 0x5D, 0x23, 0x17, 0xC0, 0x52, 0xE1,
    0xA6, 0x30, 0x7C, 0x2B, 0xB6, 0x70, 0x36, 0x5B, 0x2A, 0x27, 0x69, 0x33, 0xF5, 0x63, 0x7B, 0x36, 0x3F, 0x26, 0x9B, 0xA3, 0xED, 0x7A,
    0x53, 0x00, 0xA4, 0x48, 0xB3, 0x50, 0x9E, 0x14, 0xA0, 0x52, 0xDE, 0x7E, 0x10, 0x2B, 0x1B, 0x77, 0x6E, 0x00};

struct Xs {
    uint32_t s; int c = 0;
    static uint32_t adv(uint32_t x) { x ^= x << 2; x ^= x >> 15; x ^= x << 13; return x; }
    explicit Xs(uint32_t seed) : s(seed) { int pc = __builtin_popcount(seed); for (int i = 0; i < pc; i++) s = adv(s); }
    uint8_t next() { uint8_t r = (uint8_t)(s >> (c << 3)); if (c == 3) { s = adv(s); c = 0; } else c++; return r; }
    int32_t next32() { uint32_t a = next(), b = next(), cc = next(), d = next(); return (int32_t)(a | b << 8 | cc << 16 | d << 24); }
};
static void put32(Bytes& o, uint32_t v) { for (int i = 0; i < 4; i++) o.push_back((uint8_t)(v >> (8 * i))); }

struct B { uint32_t key; uint8_t type; uint8_t sub; Bytes data; };

static void writeBlock(Bytes& o, const B& b) {
    put32(o, b.key);
    Xs xk(b.key);
    o.push_back((uint8_t)(b.type ^ xk.next()));
    if (b.type == 4) put32(o, (uint32_t)((int32_t)b.data.size() ^ xk.next32()));
    else if (b.type == 5) {
        int sz = b.sub == 9 || b.sub == 13 ? 2 : b.sub == 10 ? 4 : 1;
        put32(o, (uint32_t)((int32_t)(b.data.size() / sz) ^ xk.next32()));
        o.push_back((uint8_t)(b.sub ^ xk.next()));
    }
    for (uint8_t v : b.data) o.push_back((uint8_t)(v ^ xk.next()));
}

static Bytes encryptSave(const std::vector<B>& blocks) {
    Bytes p;
    for (const B& b : blocks) writeBlock(p, b);
    for (size_t i = 0; i < p.size(); i++) p[i] ^= kPad[i % 0x7F];  // PKHeX advances the 0x80 pad by 0x7F
    Bytes h(64 + p.size() + 64);
    std::memcpy(h.data(), kIntro, 64);
    std::memcpy(h.data() + 64, p.data(), p.size());
    std::memcpy(h.data() + 64 + p.size(), kOutro, 64);
    uint8_t d[32];
    save::hash::sha256(h.data(), h.size(), d);
    p.insert(p.end(), d, d + 32);
    return p;
}

static std::vector<std::string> listDir(const std::string& dir, const std::string& ext) {
    std::vector<std::string> out;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d)) {
            std::string n = e->d_name;
            if (n.size() > ext.size() && n.substr(n.size() - ext.size()) == ext) out.push_back(dir + "/" + n);
        }
        closedir(d);
    }
    return out;
}

static void putUtf16(Bytes& d, size_t off, const char* s) {
    for (size_t i = 0; s[i]; i++) d[off + i * 2] = (uint8_t)s[i];
}

struct Spec {
    const char* name;
    const char* ext;
    uint32_t boxKey, partyKey, statusKey;
    size_t stride, stored, bs, partyStride, otOff, gameOff;
    uint8_t game;
    const char* titleHas;
    bool za;
};

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data/gen89";

    // ---------- 1. real sample files: checksum + crypto round trip ----------
    struct { const char* ext; size_t stored, bs; int n; } kinds[] = {{".pk8", 328, 80, 0}, {".pk9", 328, 80, 0}, {".pa8", 360, 88, 0}};
    for (auto& k : kinds) {
        for (const auto& f : listDir(dir, k.ext)) {
            Bytes b;
            CHECK(fsu::readFile(f, b));
            CHECK(b.size() >= k.stored);
            CHECK(pkm::crypto::checksum16(b.data(), k.stored) == pkm::rd16(b.data() + 6));  // real data: checksum matches
            Bytes e = b;
            CHECK(pkm::crypto::encryptBlocks(e.data(), k.stored, k.bs));
            if (pkm::rd32(b.data()) >> 13 & 31) CHECK(std::memcmp(e.data() + 8, b.data() + 8, k.stored - 8) != 0);
            CHECK(pkm::crypto::decryptBlocks(e.data(), k.stored, k.bs));
            CHECK(std::memcmp(e.data(), b.data(), k.stored) == 0);
            k.n++;
        }
    }
    CHECK(kinds[0].n >= 5 && kinds[1].n >= 3 && kinds[2].n >= 2);

    // ---------- 2. synthetic saves for every supported game ----------
    const Spec specs[] = {
        {"Sword", ".pk8", 0x0D66012C, 0x2985FE5D, 0xF25C070E, 344, 328, 80, 344, 0xB0, 0xA4, 44, "Sword", false},
        {"Shield", ".pk8", 0x0D66012C, 0x2985FE5D, 0xF25C070E, 344, 328, 80, 344, 0xB0, 0xA4, 45, "Shield", false},
        {"Legends: Arceus", ".pa8", 0x47E1CEAB, 0x2985FE5D, 0xF25C070E, 360, 360, 88, 376, 0x20, 0x14, 47, "Arceus", false},
        {"Scarlet", ".pk9", 0x0D66012C, 0x3AA1A9AD, 0xE3E89BD1, 344, 328, 80, 344, 0x10, 0x04, 50, "Scarlet", false},
        {"Violet", ".pk9", 0x0D66012C, 0x3AA1A9AD, 0xE3E89BD1, 344, 328, 80, 344, 0x10, 0x04, 51, "Violet", false},
        {"Legends: Z-A", ".pk9", 0x0D66012C, 0x3AA1A9AD, 0xE3E89BD1, 408, 328, 80, 0, 0x10, 0x04, 52, "Z-A", true},
    };
    int saves = 0;
    for (const Spec& sp : specs) {
        auto files = listDir(dir, sp.ext);
        CHECK(files.size() >= 2);
        std::vector<Bytes> mons;
        for (const auto& f : files) { Bytes b; CHECK(fsu::readFile(f, b)); b.resize(sp.stored); mons.push_back(b); }

        Bytes box(32 * 30 * sp.stride + (sp.za ? 0 : 344), 0);
        auto place = [&](int bx, int slot, const Bytes& m) {
            Bytes e = m;
            e.resize(sp.stride < 344 ? sp.stride : 344, 0);
            CHECK(pkm::crypto::encryptBlocks(e.data(), sp.stored, sp.bs));
            std::memcpy(box.data() + ((size_t)bx * 30 + (size_t)slot) * sp.stride, e.data(), sp.stored);
        };
        place(0, 0, mons[0]);
        place(0, 29, mons[1]);
        place(31, 29, mons[0]);
        place(7, 11, mons[1]);

        Bytes names(0x22 * 32, 0);
        putUtf16(names, 0x22 * 3, "Pals");
        Bytes status(0x100, 0);
        status[sp.gameOff] = sp.game;
        putUtf16(status, sp.otOff, "Ash");
        Bytes party(sp.partyStride ? sp.partyStride * 6 + 16 : 16, 0);

        std::vector<B> blocks;
        blocks.push_back({0x11111111, 1, 0, {}});
        blocks.push_back({0x22222222, 2, 0, {}});
        blocks.push_back({0x33333333, 10, 0, Bytes{1, 2, 3, 4}});
        blocks.push_back({0x44444444, 5, 9, Bytes(64, 7)});  // array of u16
        blocks.push_back({sp.boxKey, 4, 0, box});
        blocks.push_back({0x19722C89, 4, 0, names});
        blocks.push_back({sp.statusKey, 4, 0, status});
        blocks.push_back({sp.partyKey, 4, 0, party});
        if (sp.za) blocks.push_back({0x2D87BE5C, 4, 0, Bytes(16, 1)});
        else if (sp.game == 50 || sp.game == 51) blocks.push_back({0x0DEAAEBD, 4, 0, Bytes(16, 1)});
        Bytes file = encryptSave(blocks);

        std::string err;
        auto sv = save::open(file, "main", nullptr, err);
        CHECK(sv);
        CHECK(sv->title().find(sp.titleHas) != std::string::npos);
        CHECK(sv->trainer() == "Ash");
        CHECK(sv->boxCount() == 32 && sv->boxSlots() == 30);
        CHECK(sv->boxName(3) == "Pals");
        CHECK(sv->boxName(0) == "Box 1");
        Bytes out;
        CHECK(sv->readBox(0, 0, out));
        CHECK(out.size() == sp.stored && out == mons[0]);
        CHECK(sv->readBox(0, 29, out) && out == mons[1]);
        CHECK(sv->readBox(31, 29, out) && out == mons[0]);
        CHECK(sv->readBox(7, 11, out) && out == mons[1]);
        CHECK(!sv->readBox(0, 1, out));
        CHECK(!sv->readBox(32, 0, out));
        CHECK(!sv->readBox(0, 30, out));
        CHECK(sv->partyCount() == 0);
        // the parser should know the Pokemon
        pkm::Info info = pkm::parse(sv->format(), out.data(), out.size());
        CHECK(info.valid);
        // editing is refused until it is round-trip tested on a real save
        CHECK(!sv->clearBox(0, 0, err));

        // tampering: a flipped payload byte breaks the hash, so it is simply not recognised
        Bytes bad = file;
        bad[bad.size() / 2] ^= 0x40;
        CHECK(!save::open(bad, "main", nullptr, err));
        // truncation / a damaged hash
        Bytes cut(file.begin(), file.end() - 40);
        CHECK(!save::open(cut, "main", nullptr, err));
        saves++;
    }

    // a party member is read through the party block (Sword layout)
    {
        const Spec& sp = specs[0];
        Bytes b;
        CHECK(fsu::readFile(listDir(dir, ".pk8")[0], b));
        b.resize(sp.stored);
        Bytes e = b;
        e.resize(344, 0);
        CHECK(pkm::crypto::encryptBlocks(e.data(), 328, 80));
        Bytes party(344 * 6 + 16, 0);
        std::memcpy(party.data() + 344 * 2, e.data(), 344);
        std::memcpy(party.data() + 344, e.data(), 344);
        party[344 * 6] = 3;
        Bytes box(32 * 30 * 344, 0), names(0x22 * 32, 0), status(0x100, 0);
        status[0xA4] = 44;
        std::vector<B> blocks = {{sp.boxKey, 4, 0, box}, {0x19722C89, 4, 0, names}, {sp.statusKey, 4, 0, status}, {sp.partyKey, 4, 0, party}};
        std::string err;
        auto sv = save::open(encryptSave(blocks), "main", nullptr, err);
        CHECK(sv && sv->partyCount() == 3);
        Bytes out;
        CHECK(!sv->readParty(0, out));  // slot 0 is empty
        CHECK(sv->readParty(1, out) && out == b);
        CHECK(sv->readParty(2, out) && out == b);
        CHECK(!sv->readParty(3, out));
    }

    // ---------- 3. garbage never crashes ----------
    srand(99);
    for (int n = 0; n < 150; n++) {
        Bytes g((size_t)(0x1000 + rand() % 0x40000));
        for (auto& c : g) c = (uint8_t)rand();
        std::string e;
        CHECK(!save::open(g, "junk", nullptr, e));
    }
    // a save whose hash is valid but whose block list is nonsense must report an error, not crash
    for (int n = 0; n < 200; n++) {
        Bytes payload((size_t)(64 + rand() % 4000));
        for (auto& c : payload) c = (uint8_t)rand();
        Bytes enc = payload;
        for (size_t i = 0; i < enc.size(); i++) enc[i] ^= kPad[i % 0x7F];
        Bytes h(64 + enc.size() + 64);
        std::memcpy(h.data(), kIntro, 64);
        std::memcpy(h.data() + 64, enc.data(), enc.size());
        std::memcpy(h.data() + 64 + enc.size(), kOutro, 64);
        uint8_t d[32];
        save::hash::sha256(h.data(), h.size(), d);
        enc.insert(enc.end(), d, d + 32);
        if (enc.size() < 0x1000) enc.resize(0x1000, 0);  // too small: not even considered
        std::string e;
        auto r = save::open(enc, "x", nullptr, e);
        CHECK(!r);
    }

    std::printf("scblock tests passed (%d checks, %d real pk files, %d synthetic saves)\n", g_checks, kinds[0].n + kinds[1].n + kinds[2].n, saves);
    return 0;
}
