#include "boxstore.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace box {

static std::string lowerExt(const fs::path& p) {
    std::string e = p.extension().string();
    if (!e.empty() && e[0] == '.') e.erase(0, 1);
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return e;
}

Store::Store(std::string root) : root_(std::move(root)) {
    for (int i = 1; i <= kDefaultBoxCount; i++) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "Box %02d", i);
        boxNames_.push_back(buf);
    }
}

std::string Store::boxDir(int i) const { return root_ + "/boxes/" + boxNames_[i]; }

bool Store::init() {
    std::error_code ec;
    for (int i = 0; i < boxCount(); i++) {
        fs::create_directories(boxDir(i), ec);
        if (ec) return false;
    }
    return true;
}

bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n < 0 || n > (1 << 20)) { std::fclose(f); return false; }  // sanity cap 1 MiB
    out.resize((size_t)n);
    size_t got = n ? std::fread(out.data(), 1, (size_t)n, f) : 0;
    std::fclose(f);
    return got == (size_t)n;
}

bool writeFile(const std::string& path, const uint8_t* data, size_t size) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t put = std::fwrite(data, 1, size, f);
    std::fclose(f);
    return put == size;
}

std::vector<Slot> Store::load(int boxIndex) const {
    std::vector<Slot> slots;
    if (boxIndex < 0 || boxIndex >= boxCount()) return slots;
    std::error_code ec;
    std::vector<fs::path> files;
    for (const auto& e : fs::directory_iterator(boxDir(boxIndex), ec)) {
        if (e.is_regular_file(ec)) files.push_back(e.path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& p : files) {
        std::vector<uint8_t> buf;
        if (!readFile(p.string(), buf)) continue;
        pkm::Format fmt = pkm::detect(lowerExt(p), buf.size());
        if (fmt == pkm::Format::Unknown) continue;
        Slot s;
        s.path = p.string();
        s.fileName = p.filename().string();
        s.info = pkm::parse(fmt, buf.data(), buf.size());
        slots.push_back(std::move(s));
        if ((int)slots.size() >= kSlotsPerBox) break;
    }
    return slots;
}

std::string Store::add(int boxIndex, const std::string& fileName, const uint8_t* data, size_t size) {
    if (boxIndex < 0 || boxIndex >= boxCount()) return "";
    if (load(boxIndex).size() >= (size_t)kSlotsPerBox) return "";
    // Strip any directory components from the supplied name (no path traversal).
    std::string base = fs::path(fileName).filename().string();
    if (base.empty()) return "";
    fs::path dest = fs::path(boxDir(boxIndex)) / base;
    std::error_code ec;
    for (int n = 1; fs::exists(dest, ec); n++) {  // never overwrite an existing Pokemon
        dest = fs::path(boxDir(boxIndex)) /
               (fs::path(base).stem().string() + "_" + std::to_string(n) + fs::path(base).extension().string());
    }
    return writeFile(dest.string(), data, size) ? dest.string() : "";
}

bool Store::move(const Slot& slot, int destBox) {
    if (destBox < 0 || destBox >= boxCount()) return false;
    if (load(destBox).size() >= (size_t)kSlotsPerBox) return false;
    fs::path dest = fs::path(boxDir(destBox)) / slot.fileName;
    std::error_code ec;
    if (fs::exists(dest, ec)) return false;
    fs::rename(slot.path, dest, ec);
    return !ec;
}

bool Store::remove(const Slot& slot) {
    std::error_code ec;
    return fs::remove(slot.path, ec);
}

}  // namespace box
