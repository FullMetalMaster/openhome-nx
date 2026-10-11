// Mounting a game's save data from the Switch (like JKSV does) + safe backups.
// Requires custom firmware homebrew with full filesystem permissions, and the app
// must be launched via title takeover or an environment that grants save access.
#pragma once
#include <string>

#include "gamedb.hpp"

namespace save {

struct MountResult {
    bool ok = false;
    std::string device;   // e.g. "ohsave" -> files at "ohsave:/main"
    std::string error;    // human-readable
};

// Shows the system user picker and remembers the selected user. Returns false if cancelled.
bool selectUser();

// Mounts the selected user's save for `game` under device name "ohsave".
MountResult mount(const Game& game);
void unmount();

// Copies everything in the mounted save to <sdRoot>/backups/<game>/<timestamp>/.
// ALWAYS call this before any write. Returns the backup dir or "" on failure.
std::string backup(const Game& game, const std::string& sdRoot);

// Commits writes to the mounted save (required on Switch after modifying files).
bool commit();

}  // namespace save
