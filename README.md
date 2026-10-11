# OpenHome NX

A Nintendo Switch homebrew (`.nro`) Pokémon box manager inspired by
[OpenHome](https://github.com/andrewbenington/OpenHome). OpenHome itself is a Tauri/TypeScript desktop
app, so this is a native C++/libnx rewrite that stays **file-compatible** with it: boxes are folders of
raw `.pk3 … .pk9` files on your SD card.

> **Status: scaffold (v0.1).** Box storage, PKM detection/parsing, game database, save mounting and
> backups are in place. Reading/writing the Pokémon inside each game's save is **not implemented yet**
> (see Roadmap). Nothing here ever writes to a game save.

## Build (Ubuntu)

```bash
# 1. Install devkitPro pacman, then:
sudo dkp-pacman -S switch-dev
export DEVKITPRO=/opt/devkitpro
export PATH=$DEVKITPRO/devkitA64/bin:$PATH

# 2. Build
make            # -> openhome-nx.nro
```

Copy `openhome-nx.nro` to `sd:/switch/` and launch from the Homebrew Menu.
Needs custom firmware (e.g. Atmosphere). Save mounting needs full filesystem permissions, so launch via
title takeover (hold R on a game, as with JKSV) rather than the album applet if mounting fails.

## Host tests (no Switch toolchain)

```bash
g++ -std=c++17 -Wall -Isource -o tests/test_pkm tests/test_pkm.cpp \
  source/pkm/pkm.cpp source/pkm/species_data.cpp source/pkm/data_tables.cpp source/box/boxstore.cpp \
  source/save/gamedb.cpp source/save/savemount.cpp && ./tests/test_pkm
```

## Layout

| Path | Purpose |
|------|---------|
| `source/pkm/` | PKM format detection + core field parsing, species names |
| `source/box/` | SD-card box store (`sdmc:/switch/openhome-nx/boxes/Box NN/*.pkN`) |
| `source/save/` | Game DB (title IDs), user select, save mount, **backup** |
| `source/ui/` | Console UI (swap for borealis/SDL2 later) |
| `source/pkm/data_tables.cpp` | Generated name/data tables (species, moves, abilities, items, Gen 3/4 charsets) |
| `tools/gen_data.py` | Regenerates the tables: `python3 tools/gen_data.py /path/to/OpenHome` (GPL-3.0 data) |

## Supported games (database)

Let's Go Pikachu/Eevee (PB7), Sword/Shield (PK8), Brilliant Diamond/Shining Pearl (PB8),
Legends: Arceus (PA8), Scarlet/Violet (PK9). Legends: Z-A is not included yet: add it to
`source/save/gamedb.cpp` once its title ID and save layout are confirmed.

## Roadmap (what's left, in order)

1. **Verify on hardware**: title IDs, user picker, `fsdevMountSaveData`, and the backup copy.
2. **Let's Go (PB7) save reader**: plain binary, easiest first target.
3. **BDSP (`SaveData.bin`)** reader.
4. **SCBlock + swish decryption** for Sword/Shield, Legends: Arceus, Scarlet/Violet. Port from
   [PKHeX](https://github.com/kwsch/PKHeX) (`SwishCrypto`, `SCBlock`). Mind its GPL-3.0 license.
5. **Box ↔ save transfer** with backup-first, `fsdevCommitDevice`, checksum/hash fixups.
6. **Cross-generation conversion** (PK8→PK9 etc.) per OpenHome's rules. Home-compat checks are the hard part.
7. Gen 3–5 encrypted/shuffled stored-data decryption (`.pk3` currently reports format only).
8. A graphical UI with touch support.

## Caveats

- Struct offsets in `pkm.cpp` follow PKHeX's decrypted stored layouts, tested only against synthetic
  buffers. Validate against real exports; the `PB8` size in particular should be checked against PKHeX.
- Modifying saves can corrupt them or trigger bans in online play. Always keep the backups this app makes.
- Not affiliated with OpenHome, Nintendo, or The Pokémon Company.
