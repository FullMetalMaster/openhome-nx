// Console UI. Deliberately dependency-free (libnx only) so it builds out of the box.
// The UI is isolated here: swap in borealis/SDL2 later without touching box/ pkm/ save/.
#include "ui/app.hpp"

#include <switch.h>

#include <cstdio>
#include <string>
#include <vector>

#include "box/boxstore.hpp"
#include "pkm/species.hpp"
#include "save/gamedb.hpp"
#include "save/savemount.hpp"

namespace ui {

static const char* kRoot = "sdmc:/switch/openhome-nx";

enum class Screen { Menu, Boxes, Games };

static void clear() { printf("\x1b[2J\x1b[1;1H"); }

static std::string slotLabel(const box::Slot& s) {
    std::string nick = pkm::utf16ToUtf8(s.info.nickname);
    char buf[160];
    snprintf(buf, sizeof(buf), "[%s] #%u %s%s%s", pkm::formatName(s.info.format), s.info.species,
             species::name(s.info.species), nick.empty() ? "" : " - ", nick.c_str());
    return buf;
}

int run() {
    PadState pad;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    box::Store store(kRoot);
    bool storeOk = store.init();

    Screen screen = Screen::Menu;
    int menuSel = 0, boxIdx = 0, slotSel = 0, gameSel = 0;
    std::vector<box::Slot> slots;
    std::string status = storeOk ? "Ready." : "Could not create folders on the SD card!";
    bool dirty = true;
    const auto& games = save::games();

    while (appletMainLoop()) {
        padUpdate(&pad);
        u64 down = padGetButtonsDown(&pad);
        if (screen == Screen::Menu && (down & HidNpadButton_Plus)) break;

        if (screen == Screen::Menu) {
            if (down & HidNpadButton_Up) { menuSel = (menuSel + 2) % 3; dirty = true; }
            if (down & HidNpadButton_Down) { menuSel = (menuSel + 1) % 3; dirty = true; }
            if (down & HidNpadButton_A) {
                if (menuSel == 0) { screen = Screen::Boxes; slots = store.load(boxIdx); slotSel = 0; }
                else if (menuSel == 1) { screen = Screen::Games; }
                else break;
                dirty = true;
            }
        } else if (screen == Screen::Boxes) {
            if (down & HidNpadButton_B) { screen = Screen::Menu; dirty = true; }
            if (down & HidNpadButton_L) { boxIdx = (boxIdx + store.boxCount() - 1) % store.boxCount(); slots = store.load(boxIdx); slotSel = 0; dirty = true; }
            if (down & HidNpadButton_R) { boxIdx = (boxIdx + 1) % store.boxCount(); slots = store.load(boxIdx); slotSel = 0; dirty = true; }
            if (!slots.empty()) {
                int n = (int)slots.size();
                if (down & HidNpadButton_Up) { slotSel = (slotSel + n - 1) % n; dirty = true; }
                if (down & HidNpadButton_Down) { slotSel = (slotSel + 1) % n; dirty = true; }
                if (down & HidNpadButton_X) {  // move to next box
                    int dest = (boxIdx + 1) % store.boxCount();
                    status = store.move(slots[slotSel], dest) ? "Moved to " + store.boxName(dest) : "Move failed (box full?)";
                    slots = store.load(boxIdx); if (slotSel >= (int)slots.size()) slotSel = 0; dirty = true;
                }
            }
        } else if (screen == Screen::Games) {
            int n = (int)games.size();
            if (down & HidNpadButton_B) { screen = Screen::Menu; dirty = true; }
            if (down & HidNpadButton_Up) { gameSel = (gameSel + n - 1) % n; dirty = true; }
            if (down & HidNpadButton_Down) { gameSel = (gameSel + 1) % n; dirty = true; }
            if (down & HidNpadButton_Y) {  // pick user
                status = save::selectUser() ? "User selected." : "User selection cancelled.";
                dirty = true;
            }
            if (down & HidNpadButton_A) {  // mount + backup (read-only probe for now)
                const auto& g = games[gameSel];
                auto r = save::mount(g);
                if (!r.ok) status = r.error;
                else {
                    std::string b = save::backup(g, kRoot);
                    status = b.empty() ? "Mounted, but BACKUP FAILED - not touching save." : "Backup written: " + b;
                    // TODO: parse g.mainFile (see save/README in docs) and list/transfer Pokemon.
                    save::unmount();
                }
                dirty = true;
            }
        }

        if (dirty) {
            clear();
            printf("== OpenHome NX ==\n\n");
            if (screen == Screen::Menu) {
                const char* items[] = {"Boxes (SD card)", "Game saves", "Quit"};
                for (int i = 0; i < 3; i++) printf("%s %s\n", i == menuSel ? ">" : " ", items[i]);
            } else if (screen == Screen::Boxes) {
                printf("%s  (L/R: change box)  %d Pokemon\n\n", store.boxName(boxIdx).c_str(), (int)slots.size());
                if (slots.empty()) printf("  Empty. Put .pk3-.pk9 files in\n  %s/boxes/%s/\n", kRoot, store.boxName(boxIdx).c_str());
                for (int i = 0; i < (int)slots.size(); i++)
                    printf("%s %s\n", i == slotSel ? ">" : " ", slotLabel(slots[i]).c_str());
                printf("\nX: move to next box   B: back\n");
            } else {
                printf("Y: choose user   A: mount + backup   B: back\n\n");
                for (int i = 0; i < (int)games.size(); i++)
                    printf("%s %s\n", i == gameSel ? ">" : " ", games[i].name);
            }
            printf("\n%s\n", status.c_str());
            consoleUpdate(nullptr);
            dirty = false;
        }
    }
    return 0;
}

}  // namespace ui
