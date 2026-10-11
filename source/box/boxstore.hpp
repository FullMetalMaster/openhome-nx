// SD-card box storage. Layout (compatible with dropping files in from desktop OpenHome/PKHeX):
//   <root>/boxes/Box 01/<anything>.pk9
//   <root>/boxes/Box 02/...
// A "box" is just a folder; a slot is just a file. Moving a Pokemon = moving a file.
#pragma once
#include <string>
#include <vector>

#include "pkm/pkm.hpp"

namespace box {

constexpr int kSlotsPerBox = 30;
constexpr int kDefaultBoxCount = 36;

struct Slot {
    std::string path;      // full path to the file
    std::string fileName;  // basename
    pkm::Info info;
};

class Store {
public:
    explicit Store(std::string root);

    // Creates <root>/boxes and the default box folders if missing.
    bool init();

    int boxCount() const { return (int)boxNames_.size(); }
    const std::string& boxName(int i) const { return boxNames_[i]; }

    // Re-scan one box from disk (max kSlotsPerBox slots shown; extras are kept on disk).
    std::vector<Slot> load(int boxIndex) const;

    // Import a raw pkm buffer into a box. Returns the new path or "" on failure/full box.
    std::string add(int boxIndex, const std::string& fileName, const uint8_t* data, size_t size);

    // Move a slot's file to another box.
    bool move(const Slot& slot, int destBox);

    bool remove(const Slot& slot);

    const std::string& root() const { return root_; }

private:
    std::string boxDir(int i) const;
    std::string root_;
    std::vector<std::string> boxNames_;
};

bool readFile(const std::string& path, std::vector<uint8_t>& out);
bool writeFile(const std::string& path, const uint8_t* data, size_t size);

}  // namespace box
