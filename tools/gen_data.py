#!/usr/bin/env python3
"""
Generate the static data tables used by OpenHome-NX from a checkout of the
OpenHome repository (https://github.com/andrewbenington/OpenHome, GPL-3.0).

    python3 tools/gen_data.py /path/to/OpenHome

Writes:
    source/pkm/data_tables.cpp

The generator sanity-checks everything it parses (counts, spot values) and
aborts instead of silently emitting a bad table.
"""
import json
import re
import sys
from pathlib import Path

GROWTH_IDS = {
    "Erratic": 0,
    "Fast": 1,
    "MediumFast": 2,
    "MediumSlow": 3,
    "Slow": 4,
    "Fluctuating": 5,
}


def die(msg):
    sys.exit(f"gen_data: {msg}")


def cstr(s):
    """C string literal body: raw UTF-8, escaping what C needs."""
    return (
        s.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("?", "\\?")
        .replace("\n", "")
    )


def emit_str_array(out, name, items):
    out.append(f"const char* const {name}[] = {{")
    for it in items:
        out.append(f'    "{cstr(it)}",')
    out.append("};")
    out.append(f"const unsigned {name}Count = {len(items)};")
    out.append("")


def parse_species_names(root):
    p = root / "pkm_rs_resources/src/pkhex_text/species/text_Species_en.txt"
    names = p.read_text(encoding="utf-8").split("\n")
    if names and names[-1] == "":
        names.pop()
    if len(names) != 1026:
        die(f"expected 1026 species name lines, got {len(names)}")
    if names[0] != "Egg" or names[1] != "Bulbasaur" or names[1025] != "Pecharunt":
        die("species name file layout unexpected")
    return names


def parse_growth(root):
    # NationalDex variants in numeric order (sequential from Bulbasaur = 1)
    src = (root / "pkm_rs_types/src/national_dex.rs").read_text(encoding="utf-8")
    body = src.split("pub enum NationalDex {", 1)[1].split("\n}", 1)[0]
    variants = []
    for line in body.split("\n"):
        m = re.match(r"^\s{4}([A-Za-z0-9_]+)(?:\s*=\s*(\d+))?,", line)
        if m:
            variants.append(m.group(1))
    if variants[0] != "Bulbasaur" or variants[-1] != "Pecharunt" or len(variants) != 1025:
        die(f"NationalDex parse unexpected ({len(variants)} variants)")

    meta = (root / "pkm_rs_resources/src/species/metadata.rs").read_text(encoding="utf-8")
    growth_by_name = {}
    # species-level entries are indented exactly 8 spaces; forms are deeper
    pat = re.compile(
        r"^ {8}national_dex: NationalDex::(\w+),\n {8}experience_growth: ExperienceGrowth::(\w+),",
        re.M,
    )
    for m in pat.finditer(meta):
        growth_by_name[m.group(1)] = GROWTH_IDS[m.group(2)]
    missing = [v for v in variants if v not in growth_by_name]
    if missing:
        die(f"no growth rate for {missing[:5]} ({len(missing)} total)")

    table = [0] + [growth_by_name[v] for v in variants]
    spot = {
        1: "MediumSlow",  # Bulbasaur
        25: "MediumFast",  # Pikachu
        113: "Fast",  # Chansey
        149: "Slow",  # Dragonite
        290: "Erratic",  # Nincada
        321: "Fluctuating",  # Wailord
        384: "Slow",  # Rayquaza
    }
    for dex, nm in spot.items():
        if table[dex] != GROWTH_IDS[nm]:
            die(f"growth spot check failed for #{dex}: got {table[dex]}, want {nm}")
    return table


def parse_moves(root):
    d = json.loads((root / "src/core/resources/moves/moves.json").read_text(encoding="utf-8"))
    mx = max(int(k) for k in d)
    names = ["(none)"] + [""] * mx
    for k, v in d.items():
        names[int(k)] = v["name"]
    for i, n in enumerate(names):
        if not n:
            names[i] = f"Move #{i}"
    if names[1] != "Pound" or names[2] != "Karate Chop":
        die("move table unexpected")
    return names


def parse_id_name_table(path, expected_count, first_name):
    src = path.read_text(encoding="utf-8")
    pairs = re.findall(r"id:\s*(\d+),\s*name:\s*\"((?:[^\"\\]|\\.)*)\",?", src)
    if len(pairs) != expected_count:
        die(f"{path.name}: expected {expected_count} entries, got {len(pairs)}")
    mx = max(int(i) for i, _ in pairs)
    names = ["(none)"] + [""] * mx
    for i, n in pairs:
        if "\\" in n:
            n = n.replace('\\"', '"').replace("\\'", "'").replace("\\\\", "\\")
            if "\\" in n:
                die(f"{path.name}: unhandled escape in name {n!r}")
        names[int(i)] = n
    for i, n in enumerate(names):
        if not n:
            names[i] = f"#{i}"
    if names[1] != first_name:
        die(f"{path.name}: first entry is {names[1]!r}, want {first_name!r}")
    return names


def parse_gen3_index(root):
    src = (root / "pkm_rs/src/gen3/pokemon_index.rs").read_text(encoding="utf-8")
    fn = src.split("const fn gen3_to_national_dex", 1)[1].split("\n}\n", 1)[0]
    table = [0] * 412
    for k in range(1, 252):
        table[k] = k
    for m in re.finditer(r"^\s+(\d+) => Some\((\d+)\),", fn, re.M):
        table[int(m.group(1))] = int(m.group(2))
    checks = {277: 252, 301: 290, 304: 276, 308: 327, 411: 358, 410: 386}
    for k, v in checks.items():
        if table[k] != v:
            die(f"gen3 index spot check failed: {k} -> {table[k]}, want {v}")
    if table[260] != 0:
        die("gen3 filler slots should map to 0")
    return table


def parse_sv_index(root):
    src = (root / "src/core/pkm/conversion/SVPokemonIndex.ts").read_text(encoding="utf-8")
    block = src.split("const fromSVPokemonIndexMap", 1)[1].split("\n}", 1)[0]
    table = list(range(1026))
    n = 0
    for m in re.finditer(r"^\s+(\d+):\s*(\d+),", block, re.M):
        table[int(m.group(1))] = int(m.group(2))
        n += 1
    if n < 100 or table[917] != 982 or table[1025] != 1013:
        die(f"SV index map unexpected ({n} entries)")
    return table


def parse_gen3_chars(root, which):
    src = (root / "pkm_rs/src/strings/gen3.rs").read_text(encoding="utf-8")
    block = src.split(f"const ENCODING_{which}: [char; 256] = [", 1)[1].split("];", 1)[0]
    block = re.sub(r"//[^\n]*", "", block)
    toks = re.findall(r"'(\\'|\\\\|[^'\\])'|(INVALID)", block)
    out = []
    for ch, inv in toks:
        if inv:
            out.append(0xFFFF)
        else:
            ch = "'" if ch == "\\'" else ("\\" if ch == "\\\\" else ch)
            cp = ord(ch)
            if cp > 0xFFFF:
                die("non-BMP char in gen3 table")
            out.append(cp)
    if len(out) != 256:
        die(f"gen3 {which} table has {len(out)} entries, want 256")
    return out


def parse_gen4_chars(root):
    src = (root / "src/core/util/gen4ToUTFMap.ts").read_text(encoding="utf-8")
    pairs = [
        (int(a, 16), int(b, 16))
        for a, b in re.findall(r"^\s+0x([0-9a-fA-F]+):\s*0x([0-9a-fA-F]+),", src, re.M)
    ]
    if len(pairs) < 2000:
        die(f"gen4 char map too small ({len(pairs)})")
    pairs.sort()
    return pairs


def main():
    if len(sys.argv) != 2:
        die("usage: gen_data.py /path/to/OpenHome")
    root = Path(sys.argv[1])
    if not (root / "pkm_rs").is_dir():
        die(f"{root} does not look like an OpenHome checkout")

    species = parse_species_names(root)
    growth = parse_growth(root)
    moves = parse_moves(root)
    abilities = parse_id_name_table(
        root / "pkm_rs_resources/src/abilities.rs", 319, "Stench"
    )
    items = parse_id_name_table(
        root / "pkm_rs_resources/src/items/modern.rs", 2684, "Master Ball"
    )
    gen3_idx = parse_gen3_index(root)
    sv_idx = parse_sv_index(root)
    g3_int = parse_gen3_chars(root, "INT")
    g3_jpn = parse_gen3_chars(root, "JPN")
    g4 = parse_gen4_chars(root)

    out = []
    out.append("// GENERATED by tools/gen_data.py from the OpenHome repository (GPL-3.0).")
    out.append("// Do not edit by hand; re-run the generator instead.")
    out.append('#include "data.h"')
    out.append("")
    out.append("namespace ohnx {")
    out.append("")
    emit_str_array(out, "kSpeciesNames", species)
    emit_str_array(out, "kMoveNames", moves)
    emit_str_array(out, "kAbilityNames", abilities)
    emit_str_array(out, "kItemNames", items)

    def emit_num_array(ctype, name, arr, per_line=16, hexfmt=False):
        out.append(f"const {ctype} {name}[] = {{")
        for i in range(0, len(arr), per_line):
            chunk = arr[i : i + per_line]
            out.append("    " + ", ".join((f"0x{v:04X}" if hexfmt else str(v)) for v in chunk) + ",")
        out.append("};")
        out.append(f"const unsigned {name}Count = {len(arr)};")
        out.append("")

    emit_num_array("unsigned char", "kGrowth", growth, 32)
    emit_num_array("unsigned short", "kGen3ToNational", gen3_idx)
    emit_num_array("unsigned short", "kSvToNational", sv_idx)
    emit_num_array("char16_t", "kGen3IntChars", g3_int, 16, True)
    emit_num_array("char16_t", "kGen3JpnChars", g3_jpn, 16, True)

    out.append("const Gen4Pair kGen4Chars[] = {")
    for i in range(0, len(g4), 6):
        out.append("    " + " ".join(f"{{0x{a:04X},0x{b:04X}}}," for a, b in g4[i : i + 6]))
    out.append("};")
    out.append(f"const unsigned kGen4CharsCount = {len(g4)};")
    out.append("")
    out.append("} // namespace ohnx")
    out.append("")

    dest = Path(__file__).resolve().parent.parent / "source/pkm/data_tables.cpp"
    dest.write_text("\n".join(out), encoding="utf-8")
    print(
        f"wrote {dest} ({dest.stat().st_size // 1024} KiB): "
        f"{len(species)} species, {len(moves)} moves, {len(abilities)} abilities, "
        f"{len(items)} items, {len(g4)} gen4 chars"
    )


if __name__ == "__main__":
    main()
