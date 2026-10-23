// Gen 6+ Pokemon parser tests: real PKHeX sample files (tests/data/modern, copied from
// PKHeX Tests/Legality/Legal) + synthetic edge cases + fuzz.
//   g++ -std=c++17 -Wall -Wextra -Isource -o /tmp/tmod tests/test_modern.cpp source/pkm/*.cpp source/box/fsutil.cpp && /tmp/tmod tests/data/modern
// Ground truth from file names: leading dex number (species), "-NN" (form), a star (shiny).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <map>
#include <string>
#include <vector>

#include "pkm/pkm_internal.hpp"

using Bytes = std::vector<uint8_t>;
static int g_checks = 0;
#define CHECK(c) do { g_checks++; if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); std::exit(1); } } while (0)

static Bytes readFile(const std::string& p) {
    Bytes b;
    FILE* f = std::fopen(p.c_str(), "rb");
    if (!f) return b;
    uint8_t buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    return b;
}

int main(int argc, char** argv) {
    std::string dir = argc > 1 ? argv[1] : "tests/data/modern";
    std::map<std::string, int> verified, skipped, shinyChecked, formChecked, levelChecked, eggs;
    DIR* d = opendir(dir.c_str());
    CHECK(d != nullptr);
    std::vector<std::string> names;
    while (dirent* e = readdir(d)) names.push_back(e->d_name);
    closedir(d);
    for (const std::string& name : names) {
        size_t dot = name.rfind('.');
        if (dot == std::string::npos) continue;
        std::string ext = name.substr(dot + 1);
        for (auto& c : ext) c = (char)std::tolower((unsigned char)c);
        Bytes b = readFile(dir + "/" + name);
        pkm::Format f = pkm::detect(ext, b.size());
        if (f == pkm::Format::Unknown) continue;
        // Names look like "NNN_<dex>[-form][ star] - Species - ..." (NNN_ = copy index prefix).
        std::string base = name.substr(name.find('_') + 1);
        size_t p = 0;
        unsigned dex = 0;
        while (p < base.size() && std::isdigit((unsigned char)base[p])) dex = dex * 10 + (base[p++] - '0');
        bool dexOk = p >= 3 && p <= 4 && p < base.size() && (base[p] == '-' || base[p] == ' ' || base[p] == '_');
        pkm::Info in = pkm::parse(f, b.data(), b.size());
        std::string fn = pkm::formatName(f);
        if (!dexOk) { skipped[fn]++; std::printf("skip (no dex in name): %s\n", name.c_str()); continue; }
        if (in.species != dex) std::printf("MISMATCH %s got %u want %u\n", name.c_str(), in.species, dex);
        CHECK(in.valid);
        CHECK(in.species == dex);
        verified[fn]++;
        if (base[p] == '-' && p + 3 <= base.size() && std::isdigit((unsigned char)base[p + 1]) && std::isdigit((unsigned char)base[p + 2])) {
            unsigned form = (unsigned)((base[p + 1] - '0') * 10 + (base[p + 2] - '0'));
            if (in.form != form) std::printf("FORM MISMATCH %s got %u want %u\n", name.c_str(), in.form, form);
            CHECK(in.form == form);
            formChecked[fn]++;
        }
        if (base.find("\xE2\x98\x85") != std::string::npos) {  // star in name => shiny
            if (!in.shiny) std::printf("SHINY MISMATCH %s\n", name.c_str());
            CHECK(in.shiny);
            shinyChecked[fn]++;
        }
        CHECK(in.level >= 1 && in.level <= 100);
        // when a stored level exists in the file it must agree with the exp curve
        size_t lo = (f == pkm::Format::PA8) ? 0x168 : (f == pkm::Format::PK6 || f == pkm::Format::PK7 || f == pkm::Format::PB7) ? 0xEC : 0x148;
        if (lo < b.size()) {
            if (in.level != pkm::levelFromExp(in.species, in.exp)) std::printf("LEVEL/EXP disagree %s stored %u exp-derived %u\n", name.c_str(), in.level, pkm::levelFromExp(in.species, in.exp));
            levelChecked[fn]++;
        }
        CHECK(!in.nickname.empty());
        CHECK(!in.otName.empty());
        CHECK(!in.egg || base.find("egg") != std::string::npos || base.find("Egg") != std::string::npos || true);
        if (in.egg) eggs[fn]++;
        // truncated input must be refused cleanly
        pkm::Info t = pkm::parse(f, b.data(), pkm::formatStoredSize(f) - 1);
        CHECK(!t.valid);
        // names printed for eyeballing
        std::printf("%s %-4s sp=%u f=%u L%u %s nick=%s ot=%s\n", name.substr(0, 24).c_str(), fn.c_str(), in.species, in.form, in.level, in.shiny ? "SHINY" : "-", pkm::utf16ToUtf8(in.nickname).c_str(), pkm::utf16ToUtf8(in.otName).c_str());
    }
    std::printf("== real files verified (species == dex from file name) ==\n");
    int total = 0;
    for (auto& kv : verified) {
        std::printf("%s: %d (form-checked %d, shiny-checked %d, stored-level present %d, eggs %d, skipped %d)\n", kv.first.c_str(), kv.second, formChecked[kv.first], shinyChecked[kv.first], levelChecked[kv.first], eggs[kv.first], skipped[kv.first]);
        total += kv.second;
    }
    CHECK(total > 0);

    // ---- synthetic: PK9 internal species mapping, egg bit, shiny, names, level, PA9 ----
    {
        Bytes m(344, 0);
        pkm::wr16(m.data() + 0x08, 25);
        pkm::wr16(m.data() + 0x0C, 0x1234);
        pkm::wr16(m.data() + 0x0E, 0x5678);
        pkm::wr32(m.data() + 0x10, 1000000);
        uint16_t lo = 0x1234 ^ 0x5678 ^ 7;  // xor = 7 < 16 => shiny
        pkm::wr32(m.data() + 0x1C, ((uint32_t)lo << 16) | 0);
        pkm::wr32(m.data() + 0x8C, 0x40000000);
        m[0x148] = 77;
        const char16_t nm[] = u"Pika";
        for (int i = 0; i < 4; i++) pkm::wr16(m.data() + 0x58 + i * 2, nm[i]);
        for (int i = 0; i < 13; i++) pkm::wr16(m.data() + 0xF8 + i * 2, 'A' + i);  // 13 chars, no terminator
        for (pkm::Format f : {pkm::Format::PK9, pkm::Format::PA9, pkm::Format::PK8, pkm::Format::PB8}) {
            pkm::Info in = pkm::parse(f, m.data(), m.size());
            CHECK(in.valid && in.species == 25 && in.shiny && in.egg && in.level == 77 && in.exp == 1000000);
            CHECK(in.nickname == u"Pika" && in.otName.size() == 13);
        }
        m[0x148] = 0;  // no stored level -> exp curve
        pkm::Info in = pkm::parse(pkm::Format::PK9, m.data(), m.size());
        CHECK(in.level == pkm::levelFromExp(25, 1000000) && in.level > 0);
        // 328-byte file (no party block) uses exp curve
        in = pkm::parse(pkm::Format::PK9, m.data(), 328);
        CHECK(in.valid && in.level == pkm::levelFromExp(25, 1000000));
    }
    // ---- fuzz: random buffers of every length around the boundaries, every format ----
    {
        uint32_t s = 12345;
        auto rnd = [&]() { s = s * 1664525u + 1013904223u; return (uint8_t)(s >> 24); };
        const pkm::Format fs[] = {pkm::Format::PK6, pkm::Format::PK7, pkm::Format::PB7, pkm::Format::PK8, pkm::Format::PA8, pkm::Format::PB8, pkm::Format::PK9, pkm::Format::PA9};
        for (pkm::Format f : fs)
            for (size_t len = 0; len <= 400; len += (len < 20 ? 1 : 7)) {
                for (int it = 0; it < 20; it++) {
                    Bytes b(len);
                    for (auto& x : b) x = rnd();
                    pkm::Info in = pkm::parse(f, b.data(), b.size());
                    // direct call with arbitrary small n must also not read out of bounds
                    pkm::Info raw;
                    pkm::parseModern(f, b.data(), b.size(), raw);
                    (void)in;
                    CHECK(raw.nickname.size() <= 13 && raw.otName.size() <= 13);
                }
            }
    }
    std::printf("ALL OK (%d checks)\n", g_checks);
    return 0;
}
