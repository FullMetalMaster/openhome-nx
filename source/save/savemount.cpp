#include "savemount.hpp"

#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>

#ifdef __SWITCH__
#include <switch.h>
#endif

namespace fs = std::filesystem;

namespace save {

#ifdef __SWITCH__
static AccountUid g_uid{};
static bool g_haveUser = false;
static bool g_mounted = false;
static const char* kDev = "ohsave";

bool selectUser() {
    PselUserSelectionSettings cfg;
    __builtin_memset(&cfg, 0, sizeof(cfg));
    AccountUid uid{};
    Result rc = pselShowUserSelector(&uid, &cfg);
    if (R_FAILED(rc)) return false;
    g_uid = uid;
    g_haveUser = true;
    return true;
}

MountResult mount(const Game& game) {
    MountResult r;
    if (!g_haveUser) { r.error = "No user selected"; return r; }
    unmount();
    Result rc = fsdevMountSaveData(kDev, game.titleId, g_uid);
    if (R_FAILED(rc)) {
        char buf[96];
        snprintf(buf, sizeof(buf), "Mount failed (0x%X). Is the game installed and does this user have a save?", rc);
        r.error = buf;
        return r;
    }
    g_mounted = true;
    r.ok = true;
    r.device = kDev;
    return r;
}

void unmount() {
    if (g_mounted) { fsdevUnmountDevice(kDev); g_mounted = false; }
}

bool commit() { return g_mounted && R_SUCCEEDED(fsdevCommitDevice(kDev)); }

#else  // host stubs so the rest of the code can be compiled/tested on a PC
bool selectUser() { return false; }
MountResult mount(const Game&) { MountResult r; r.error = "Save mounting only works on Switch"; return r; }
void unmount() {}
bool commit() { return false; }
#endif

std::string backup(const Game& game, const std::string& sdRoot) {
    char stamp[32];
    std::time_t t = std::time(nullptr);
    std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", std::localtime(&t));
    std::string name;
    for (const char* p = game.name; *p; ++p)  // keep folder names FAT-safe
        name.push_back((std::isalnum((unsigned char)*p)) ? *p : '_');
    fs::path dest = fs::path(sdRoot) / "backups" / name / stamp;
    std::error_code ec;
    fs::create_directories(dest, ec);
    if (ec) return "";
    fs::copy("ohsave:/", dest, fs::copy_options::recursive, ec);
    return ec ? "" : dest.string();
}

}  // namespace save
