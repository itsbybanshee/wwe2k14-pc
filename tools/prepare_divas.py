#!/usr/bin/env python3
"""Prepare the Divas Evolution mod's default.xex for the codegen of wwe2k14_divas.exe.

    python -I tools/prepare_divas.py "<mod folder>/default.xex" game_divas/default.xex

Divas Evolution (by @stqned, the default.xex of Nov 2026) is the retail game (default.xex 0.0.0.2) with its code
patched: ~75 changed instructions in .text, ~5 KB of new code in the gap after RADCODE (0x835C3500-0x835C4904, in no PE
section) and a zlib-compressed default save split over the unused gaps between sections (table at 0x835C4904). Data is
read from the mod's own default.xex at run time, so the data side needs nothing. The code has to be recompiled, and three things in it are outside what the codegen handles:

  * the new code lies outside every executable section, so the codegen never decodes it;
  * trampolines: a .text instruction becomes `b <new code>`, which ends with `b` back into the middle of the original
    function. The codegen compiles functions one at a time, and a branch into the middle of another function can't
    be expressed (the SDK's [functions] `parent` chunks are registered as separate functions, not merged);
  * one function (0x82C384C0) is hooked at its first instruction, and its wrapper calls the displaced original.

Only the generated code matters at run time (guest code never executes), so the codegen gets an equivalent copy of
the XEX instead: unencrypted, uncompressed, RADCODE grown over the new code, and each function with trampolines
rebuilt as one contiguous copy in the free space after the new code (original body + its trampoline pieces, branches
and switch tables relocated), its old entry turned into a jump to the copy. The [functions] entries for all of it go
to config_divas/functions_divas.toml.

Refuses any other default.xex: every address below is specific to this release.
"""
from __future__ import annotations

import hashlib
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOML_OUT = ROOT / "config_divas" / "functions_divas.toml"

DIVAS_SHA256 = "22ADF8FAE6381F4B529E97C43E013E94B161D3E0C409979963B59F60B41417F6"
BASE = 0x82000000
RADCODE_VA = 0x835C2A00
NEXT_SECTION_VA = 0x835D0000  # BINKBSS: the rebuilt code must end before it
COPIES_BASE = 0x835C8000      # free space after the mod's code and save fragments

NOP = 0x60000000
SAVE_TABLE = 0x835C4904       # (address, word count) pairs, up to a zero address: the default save's fragments

# New functions of the mod in the RADCODE gap: address -> (size, what it does as far as the code shows).
MOD_FUNCTIONS = {
    0x835C3500: (0xB4, "both objects pass the same vtable+0x80 check (0x825A0B50)"),
    0x835C3900: (0xF0, "scans the 9 slots at 0x836F38D0 for a match against r3"),
    0x835C39F0: (0x60, "0..99 value from the byte table at 0x820796D8, two alternating counters"),
    0x835C3A50: (0x88, "adjusts r4 (1..99) before tail-calling 0x82464880"),
    0x835C3AD8: (0x74, "sets +0x959 on a wrestler after a roll"),
    0x835C3B4C: (0x68, "sets +0x959 on a wrestler after a roll"),
    0x835C3C00: (0x2C, "runs 0x835C3C2C on the table at *0x837733C0 + 0x112824"),
    0x835C3C2C: (0x44, "sets the top bit of two table entries (ids 0x7D8, 0x9D0)"),
    0x835C3D00: (0x9C, "flags (+0x10 = 1) 16 match/move ids"),
    0x835C3D9C: (0x58, "runs 0x835C3D00 over the 400-entry table at *0x837733C0"),
    0x835C3DF4: (0x58, "wraps 0x827D8EF0, then flags entry r3 of that table"),
    0x835C3E4C: (0x2C, "wraps 0x827D8E30, then flags the whole table"),
    0x835C3F00: (0x5C, "after 0x835C4300: registers entry 0x12A when a file exists"),
    0x835C3F5C: (0xAC, "finds or adds entry 0x12A in a 200-slot list (+0x44C0 count)"),
    0x835C4008: (0x3C, "id 0x1766 lookup with a fallback record (0x835C4080)"),
    0x835C4044: (0x3C, "wrapper around 0x82C384C0 for id 0x1766 (0xFF -> 0)"),
    0x835C4300: (0x10C, "loads extra 'COSX' records into the 200-slot list"),
    0x835C4500: (0x3C, "calls 0x822C1CF0 unless 0x822C0CA8 says so"),
    0x835C4600: (0xC, "returns the 2-entry table at 0x835C4610 in r5"),
    0x835C48B4: (0x28, "picks r8 = 0x22 once after the 0x5EED marker"),
    0x835C48DC: (0x28, "picks r11 = 0x22 once after the 0x5EED marker"),
}

# Functions whose trampolines go back into their middle: parent start -> list of (site, piece start, piece end).
# A piece reached by `bl` (instead of `b`) returns with blr to site + 4 on some paths.
PARENTS = {
    0x822BCFB8: [(0x822BD028, 0x835C3E78, 0x835C3EAC)],
    0x824192C8: [(0x82419380, 0x835C4200, 0x835C4220), (0x824193F8, 0x835C4220, 0x835C4268)],
    0x8261EB60: [(0x8261EE9C, 0x835C4700, 0x835C475C)],
    0x82818688: [(0x828186DC, 0x835C440C, 0x835C4428)],
    0x8281F150: [(0x8281F370, 0x835C4100, 0x835C411C)],
    0x82820090: [(0x82820128, 0x835C411C, 0x835C413C)],
    0x82918E10: [(0x82919924, 0x835C4800, 0x835C48B4)],
}

# The hooked function: its first instruction became `b 0x835C4044`; the displaced instruction sits at 0x835C404C,
# followed by `b 0x82C384C4` back into the body, and the wrapper calls it with `bl 0x835C404C`.
HOOKED = 0x82C384C0
HOOK_WRAPPER = 0x835C4044
HOOK_DISPLACED = 0x835C404C
HOOK_CALL = 0x835C4060

# Small functions the codegen should know by address: the thunk after 0x82C38588 (`b 0x82C38588` in the retail
# game, `b 0x835C4008` in the mod) has no .pdata entry.
EXTRA_FUNCTIONS = {0x82C38648: (0x4, "thunk, now b 0x835C4008")}


class Image:
    def __init__(self, data: bytearray):
        self.data = data

    def word(self, addr: int) -> int:
        return struct.unpack_from(">I", self.data, addr - BASE)[0]

    def put(self, addr: int, value: int) -> None:
        struct.pack_into(">I", self.data, addr - BASE, value & 0xFFFFFFFF)


def be32(b: bytes, o: int) -> int:
    return struct.unpack_from(">I", b, o)[0]


def load_xex(raw: bytes) -> tuple[bytes, int, bytearray]:
    """Header bytes, file format info offset, memory image (base 0x82000000)."""
    if raw[:4] != b"XEX2":
        sys.exit("not an XEX2 file")
    pe_off, sec, count = be32(raw, 0x8), be32(raw, 0x10), be32(raw, 0x14)
    ffi = next((be32(raw, 0x18 + i * 8 + 4) for i in range(count) if be32(raw, 0x18 + i * 8) == 0x3FF), None)
    if ffi is None:
        sys.exit("no file format info in the XEX header")
    info_size = be32(raw, ffi)
    enc, comp = struct.unpack_from(">HH", raw, ffi + 4)
    if enc != 0 or comp != 1:
        sys.exit(f"expected an unencrypted, basic-compressed XEX (got encryption {enc}, compression {comp})")
    image = bytearray(be32(raw, sec + 4))
    src, dst = pe_off, 0
    for p in range(ffi + 8, ffi + info_size, 8):
        data_size, zero_size = be32(raw, p), be32(raw, p + 4)
        image[dst:dst + data_size] = raw[src:src + data_size]
        src += data_size
        dst += data_size + zero_size
    if image[:2] != b"MZ":
        sys.exit("the decoded image has no MZ header")
    return raw[:pe_off], ffi, image


# --- PowerPC branch encoding -----------------------------------------------------------------------------------------

def branch_target(word: int, addr: int) -> int | None:
    """Target of a relative I-form (b/bl) or B-form (bc/bcl) branch."""
    op = word >> 26
    if op == 18 and not word & 2:
        li = word & 0x03FFFFFC
        return (addr + (li - 0x04000000 if li & 0x02000000 else li)) & 0xFFFFFFFF
    if op == 16 and not word & 2:
        bd = word & 0xFFFC
        return (addr + (bd - 0x10000 if bd & 0x8000 else bd)) & 0xFFFFFFFF
    return None


def with_target(word: int, addr: int, target: int) -> int:
    disp = target - addr
    if word >> 26 == 18:
        if not -0x02000000 <= disp < 0x02000000:
            sys.exit(f"branch at {addr:08X} can't reach {target:08X}")
        return (word & ~0x03FFFFFC) | (disp & 0x03FFFFFC)
    if not -0x8000 <= disp < 0x8000:
        sys.exit(f"conditional branch at {addr:08X} can't reach {target:08X}")
    return (word & ~0xFFFC) | (disp & 0xFFFC)


def b(addr: int, target: int) -> int:
    return with_target(18 << 26, addr, target)


def is_bclr(word: int) -> bool:
    return word >> 26 == 19 and (word >> 1) & 0x3FF == 16 and not word & 1


def bclr_to_branch(word: int, addr: int, target: int) -> int:
    """blr / bclr BO,BI -> b / bc BO,BI to target (the return address of the replaced bl)."""
    bo, bi = (word >> 21) & 31, (word >> 16) & 31
    if bo & 0x14 == 0x14:
        return b(addr, target)
    return with_target((16 << 26) | (bo << 21) | (bi << 16), addr, target)


# --- Rebuilt copies ---------------------------------------------------------------------------------------------------

def pdata_size(img: Image, start: int) -> int:
    pe = struct.unpack_from("<I", img.data, 0x3C)[0]
    rva, size = struct.unpack_from("<II", img.data, pe + 24 + 96 + 3 * 8)
    for i in range(size // 8):
        begin, data = struct.unpack_from(">II", img.data, rva + i * 8)
        if begin == start:
            return ((data >> 8) & 0x3FFFFF) * 4
    sys.exit(f"no .pdata entry for {start:08X}")


def switch_tables(img: Image, start: int, end: int) -> list[tuple[int, int, int]]:
    """(lis address, table address, entry count) of each `lis r12; slwi r0,r11,2; addi r12,r12; lwzx; mtctr; bctr`
    switch in [start, end), its table of absolute addresses right after the bctr, sized by the cmplwi r11 before."""
    tables = []
    for a in range(start, end, 4):
        if img.word(a) != 0x4E800420:
            continue
        lis, slwi, addi, lwzx, mtctr = (img.word(a - 0x14 + i * 4) for i in range(5))
        cmp = img.word(a - 0x1C)
        if (lis >> 16 != 0x3D80 or slwi != 0x5560103A or addi >> 16 != 0x398C or lwzx != 0x7C0C002E
                or mtctr != 0x7C0903A6 or cmp >> 16 != 0x2B0B):
            sys.exit(f"unrecognized bctr at {a:08X}")
        lo = addi & 0xFFFF
        table = ((lis & 0xFFFF) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
        if table != a + 4:
            sys.exit(f"switch table of the bctr at {a:08X} is not inline")
        tables.append((a - 0x14, table, (cmp & 0xFFFF) + 1))
    return tables


def check_code_constants(img: Image, start: int, end: int, known_lis: set[int]) -> None:
    """Refuse lis/addi pairs that build an address inside the function (other than the switch tables)."""
    for a in range(start, end, 4):
        w = img.word(a)
        if w >> 26 != 15 or (w >> 16) & 31 != 0 or a in known_lis:
            continue
        reg = (w >> 21) & 31
        for n in range(a + 4, min(a + 0x20, end), 4):
            x = img.word(n)
            if x >> 26 == 14 and (x >> 16) & 31 == reg:
                lo = x & 0xFFFF
                value = ((w & 0xFFFF) << 16) + (lo - 0x10000 if lo & 0x8000 else lo)
                if start <= value < end:
                    sys.exit(f"{a:08X}: code address constant {value:08X}, can't relocate")
                break


def rebuild(img: Image, start: int, pieces: list[tuple[int, int, int]], dest: int, first_word: int | None) -> int:
    """Copy [start, end) to dest followed by the pieces, relocate it, point the old entry at the copy. Returns the
    copy's size."""
    end = start + pdata_size(img, start)
    tables = switch_tables(img, start, end)
    check_code_constants(img, start, end, {lis for lis, _, _ in tables})

    places = [(start, end, dest)]
    at = dest + (end - start)
    for _, p_start, p_end in pieces:
        check_code_constants(img, p_start, p_end, set())
        places.append((p_start, p_end, at))
        at += p_end - p_start

    def moved(addr: int) -> int | None:
        for s, e, d in places:
            if s <= addr < e:
                return d + addr - s
        return None

    table_words = {t + i * 4 for _, t, n in tables for i in range(n)}
    sites = {site: (p_start, p_end) for site, p_start, p_end in pieces}
    for s, e, d in places:
        for a in range(s, e, 4):
            w = img.word(a)
            new = d + a - s
            if a == start and first_word is not None:
                w = first_word
            if a in table_words:
                img.put(new, moved(w) if moved(w) is not None else w)
                continue
            if a in sites:  # b / bl to the piece -> b to its copy
                w = b(new, moved(sites[a][0]))
            elif (t := branch_target(w, a)) is not None:
                w = with_target(w, new, moved(t) if moved(t) is not None else t)
            img.put(new, w)

    # Pieces reached by bl return to site + 4.
    for site, p_start, p_end in pieces:
        if img.word(site) & 1:
            for a in range(p_start, p_end, 4):
                if is_bclr(img.word(a)):
                    img.put(moved(a), bclr_to_branch(img.word(a), moved(a), moved(site + 4)))

    for lis, table, _ in tables:
        new_table = moved(table)
        img.put(moved(lis), (img.word(lis) & 0xFFFF0000) | ((new_table + 0x8000) >> 16))
        addi = lis + 8
        img.put(moved(addi), (img.word(addi) & 0xFFFF0000) | (new_table & 0xFFFF))

    # The old body is now unreachable: keep it consistent for the codegen (no branches into the pieces), then clear
    # the pieces themselves so nothing decodes them as code.
    img.put(start, b(start, dest))
    for site, p_start, p_end in pieces:
        img.put(site, NOP)
        img.data[p_start - BASE:p_end - BASE] = bytes(p_end - p_start)
    return at - dest


def clear_save_fragments(img: Image) -> None:
    """The fragments inside RADCODE's new range are data: nothing should decode them as code."""
    table = SAVE_TABLE
    while (addr := img.word(table)) != 0:
        count = img.word(table + 4)
        if RADCODE_VA <= addr < NEXT_SECTION_VA:
            img.data[addr - BASE:addr - BASE + count * 4] = bytes(count * 4)
        img.put(table, 0)
        img.put(table + 4, 0)
        table += 8


def set_radcode_size(img: Image, end: int) -> None:
    pe = struct.unpack_from("<I", img.data, 0x3C)[0]
    nsec = struct.unpack_from("<H", img.data, pe + 6)[0]
    opt = struct.unpack_from("<H", img.data, pe + 20)[0]
    for i in range(nsec):
        o = pe + 24 + opt + i * 40
        if img.data[o:o + 8].rstrip(b"\0") == b"RADCODE":
            if BASE + struct.unpack_from("<I", img.data, o + 12)[0] != RADCODE_VA:
                sys.exit("RADCODE is not where this release has it")
            struct.pack_into("<I", img.data, o + 8, end - RADCODE_VA)
            return
    sys.exit("no RADCODE section")


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src, dst = Path(sys.argv[1]), Path(sys.argv[2])
    raw = src.read_bytes()
    digest = hashlib.sha256(raw).hexdigest().upper()
    if digest != DIVAS_SHA256:
        sys.exit(f"{src} is not the supported Divas Evolution default.xex (sha256 {digest})")
    header, ffi, data = load_xex(raw)
    img = Image(data)
    clear_save_fragments(img)

    toml = [
        "# Divas Evolution (wwe2k14_divas.exe): the mod's new code and the rebuilt copies of the functions",
        "# it patches with trampolines. Written by tools/prepare_divas.py, which also writes the matching",
        "# game_divas/default.xex; regenerate both together.",
        "",
        "[functions]",
        "",
        "# New code of the mod, in the gap after RADCODE (prepare_divas.py grows the section over it).",
    ]
    for addr, (size, what) in MOD_FUNCTIONS.items():
        toml.append(f'0x{addr:08X} = {{ name = "divas_{addr:08X}", size = 0x{size:X} }}  # {what}')
    for addr, (size, what) in EXTRA_FUNCTIONS.items():
        toml.append(f"0x{addr:08X} = {{ size = 0x{size:X} }}  # {what}")

    toml += ["", "# Functions with trampolines, rebuilt in one piece (the original entry jumps here)."]
    dest = COPIES_BASE
    for parent, pieces in PARENTS.items():
        size = rebuild(img, parent, pieces, dest, None)
        sites = ", ".join(f"{site:08X}" for site, _, _ in pieces)
        toml.append(f'0x{dest:08X} = {{ name = "divas_{parent:08X}", size = 0x{size:X} }}  # 0x{parent:08X}, '
                    f"trampolines at {sites}")
        dest = (dest + size + 0xF) & ~0xF

    # The hooked function: rebuild the original (displaced first instruction back in place), make the wrapper
    # tail-call it on the plain path and call it on the 0x1766 path.
    displaced = img.word(HOOK_DISPLACED)
    size = rebuild(img, HOOKED, [], dest, displaced)
    img.put(HOOKED, b(HOOKED, HOOK_WRAPPER))
    img.put(HOOK_DISPLACED, b(HOOK_DISPLACED, dest))
    img.put(HOOK_DISPLACED + 4, NOP)
    img.put(HOOK_CALL, b(HOOK_CALL, dest) | 1)
    toml.append(f'0x{dest:08X} = {{ name = "divas_{HOOKED:08X}_original", size = 0x{size:X} }}  # 0x{HOOKED:08X} '
                f"before the mod's hook (wrapper 0x{HOOK_WRAPPER:08X})")
    dest += size

    if dest > NEXT_SECTION_VA:
        sys.exit("the rebuilt code doesn't fit before BINKBSS")
    set_radcode_size(img, dest)

    # Uncompressed and unencrypted: the image follows the header as is.
    out_header = bytearray(header)
    struct.pack_into(">H", out_header, ffi + 6, 0)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(bytes(out_header) + bytes(img.data))
    TOML_OUT.parent.mkdir(parents=True, exist_ok=True)
    TOML_OUT.write_text("\n".join(toml) + "\n", encoding="utf-8", newline="\n")
    print(f"{dst}: codegen input written (RADCODE now ends at 0x{dest:08X})")
    print(f"{TOML_OUT.relative_to(ROOT)}: {len(MOD_FUNCTIONS) + len(EXTRA_FUNCTIONS) + len(PARENTS) + 1} functions")


if __name__ == "__main__":
    main()
