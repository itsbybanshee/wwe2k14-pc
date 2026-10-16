#!/usr/bin/env python3
"""Find WWE 2K14 functions the recompiler's scanner can miss, as [functions] lines for config/*.toml.

    python -I tools/find_functions.py default_dec.xex --dump-image work/image.bin
    python -I tools/find_functions.py work/image.bin --registered generated/wwe2k14_init.cpp
    python -I tools/find_functions.py work/image.bin --codegen-log logs/codegen-<time>.log

Input: a decrypted XEX (uncompressed or "basic" compression; decrypt retail discs first, e.g. with
`xextool -e u -c u -o default_dec.xex default.xex`) or a raw image saved with --dump-image.

Reported:
  * targets of data pointers (vtables, callbacks, thread entries) that look like a function start;
  * addresses built in code (lis rX,hi followed by addi/ori rY,rX,lo) pointing into code;
  * with --codegen-log: only the codegen's UnresolvedCall targets;
  * setjmp / longjmp: the CRT routines that save / restore f14-f31 through a jmp_buf.
With --registered <project>_init.cpp, functions the codegen already generated are left out.
Candidates are not proof: tools/check_labels.py after the next codegen catches entries that split a function.
"""
from __future__ import annotations

import argparse
import re
import struct
import sys
from pathlib import Path

BLR, BCTR = 0x4E800020, 0x4E800420
SCN_EXECUTE = 0x20000000


# ---------------------------------------------------------------------------------------------------------- loading

def load_xex(raw: bytes) -> tuple[bytes, int]:
    """Decrypted XEX2 -> (memory image, load address)."""
    _magic, _flags, pe_offset, _reserved, security_offset, header_count = struct.unpack_from(">4sIIIII", raw, 0)
    headers = dict(struct.unpack_from(">II", raw, 24 + i * 8) for i in range(header_count))
    image_size = struct.unpack_from(">I", raw, security_offset + 4)[0]
    load_address = headers.get(0x00010201, struct.unpack_from(">I", raw, security_offset + 0x110)[0])
    if 0x000003FF not in headers:
        sys.exit("XEX without a file format header")
    fmt_offset = headers[0x000003FF]
    info_size, encryption, compression = struct.unpack_from(">IHH", raw, fmt_offset)
    if encryption != 0:
        sys.exit("encrypted XEX: decrypt it first (see the help text)")
    data = raw[pe_offset:]
    if compression == 0:
        image = data[:image_size]
    elif compression == 1:  # basic: (data size, zero fill) blocks
        parts, pos = [], 0
        for i in range((info_size - 8) // 8):
            size, zeros = struct.unpack_from(">II", raw, fmt_offset + 8 + i * 8)
            parts += [data[pos:pos + size], bytes(zeros)]
            pos += size
        image = b"".join(parts)
    else:
        sys.exit("LZX-compressed XEX: decompress it first (see the help text)")
    if image[:2] != b"MZ":
        sys.exit("the XEX image does not start with MZ")
    return image.ljust(image_size, b"\0"), load_address


class Image:
    def __init__(self, data: bytes, base: int):
        self.data, self.base = data, base
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        sections = struct.unpack_from("<H", data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", data, pe + 20)[0]
        optional = pe + 24
        # Exception directory (data directory 3): the .pdata function table.
        pdata_rva, pdata_size = struct.unpack_from("<II", data, optional + 96 + 3 * 8)
        self.code: list[tuple[int, int]] = []
        table = optional + optional_size
        for i in range(sections):
            _name, vsize, rva = struct.unpack_from("<8sII", data, table + i * 40)
            characteristics = struct.unpack_from("<I", data, table + i * 40 + 36)[0]
            if characteristics & SCN_EXECUTE:
                self.code.append((base + rva, base + rva + vsize))
        self.pdata: dict[int, int] = {}
        for i in range(pdata_size // 8):
            begin, flags = struct.unpack_from(">II", data, pdata_rva + i * 8)
            if begin:
                self.pdata[begin] = ((flags >> 8) & 0x3FFFFF) * 4

    def word(self, address: int) -> int:
        return struct.unpack_from(">I", self.data, address - self.base)[0]

    def in_code(self, address: int) -> bool:
        return address % 4 == 0 and any(lo <= address < hi for lo, hi in self.code)

    def looks_like_start(self, address: int) -> bool:
        """The previous word ends a function: blr, bctr, an unconditional b, or padding."""
        previous = self.word(address - 4)
        return previous in (BLR, BCTR, 0) or (previous >> 26 == 18 and not previous & 1)


# ------------------------------------------------------------------------------------------------------- scanning

def data_pointer_targets(img: Image) -> dict[int, list[int]]:
    refs: dict[int, list[int]] = {}
    for offset in range(0, len(img.data) - 3, 4):
        address = img.base + offset
        if img.in_code(address):
            continue
        value = struct.unpack_from(">I", img.data, offset)[0]
        if img.in_code(value):
            refs.setdefault(value, []).append(address)
    return refs


def built_address_targets(img: Image) -> dict[int, list[int]]:
    refs: dict[int, list[int]] = {}
    for lo, hi in img.code:
        for address in range(lo, hi - 4, 4):
            insn = img.word(address)
            if insn >> 26 != 15 or (insn >> 16) & 31 != 0:   # lis rD,imm  (addis rD,0,imm)
                continue
            reg, high = (insn >> 21) & 31, (insn & 0xFFFF) << 16
            for follow in range(address + 4, min(address + 40, hi), 4):
                nxt = img.word(follow)
                op = nxt >> 26
                if op in (14, 24) and (nxt >> 16) & 31 == reg:  # addi rY,rX,lo / ori rY,rX,lo
                    low = nxt & 0xFFFF
                    target = (high + (low - 0x10000 if op == 14 and low & 0x8000 else low)) & 0xFFFFFFFF
                    if img.in_code(target):
                        refs.setdefault(target, []).append(address)
                    break
    return refs


def jmp_buf_routines(img: Image) -> tuple[int | None, int | None]:
    """setjmp stores f14-f31 through r3; longjmp loads them back and restores CR with mtcrf. Both are often
    frameless leaves without a .pdata entry, so scan the code and walk back to the function's first instruction."""
    def run_start(op: int, base_reg: int | None, address: int) -> bool:
        regs = set()
        for a in range(address, address + 18 * 4, 4):
            insn = img.word(a)
            if insn >> 26 != op or (base_reg is not None and (insn >> 16) & 31 != base_reg):
                return False
            regs.add((insn >> 21) & 31)
        return regs == set(range(14, 32))

    def function_start(address: int) -> int:
        while not img.looks_like_start(address):
            address -= 4
        return address

    setjmp = longjmp = None
    for lo, hi in img.code:
        for address in range(lo, hi - 18 * 4, 4):
            if setjmp is None and run_start(54, 3, address):          # stfd f14..f31, n(r3)
                setjmp = function_start(address)
            elif longjmp is None and run_start(50, None, address):    # lfd f14..f31
                start = function_start(address)
                body = [img.word(a) for a in range(start, min(address + 0x400, hi), 4)]  # past the vector reloads
                if any(w >> 26 == 31 and (w >> 1) & 0x3FF == 144 for w in body):  # mtcrf
                    longjmp = start
            if setjmp and longjmp:
                return setjmp, longjmp
    return setjmp, longjmp


# ------------------------------------------------------------------------------------------------------------ main

def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path, help="decrypted default.xex or a raw image from --dump-image")
    ap.add_argument("--dump-image", type=Path, help="save the memory image (base 0x82000000) and exit")
    ap.add_argument("--registered", type=Path, help="generated/<project>_init.cpp: skip generated functions")
    ap.add_argument("--codegen-log", type=Path, help="only the UnresolvedCall targets of this codegen log")
    ap.add_argument("--known", type=Path, action="append", default=[], help="config TOML whose entries to skip")
    ap.add_argument("-o", "--output", type=Path, help="write the TOML lines here instead of stdout")
    args = ap.parse_args()

    raw = args.input.read_bytes()
    if raw[:4] == b"XEX2":
        data, base = load_xex(raw)
    elif raw[:2] == b"MZ":
        data, base = raw, 0x82000000
    else:
        sys.exit("input is neither a XEX2 nor an MZ image")
    if args.dump_image:
        args.dump_image.write_bytes(data)
        print(f"image saved: {args.dump_image} ({len(data):#x} bytes, base {base:#010x})", file=sys.stderr)
        return
    img = Image(data, base)

    known = set(img.pdata)
    starts = sorted(img.pdata)
    import bisect

    def inside_pdata_function(t: int) -> bool:
        i = bisect.bisect_right(starts, t) - 1
        return i >= 0 and starts[i] < t < starts[i] + img.pdata[starts[i]]
    if args.registered:
        text = args.registered.read_text(encoding="utf-8", errors="replace")
        known |= {int(m, 16) for m in re.findall(r"^\s*\{\s*(0x[0-9A-Fa-f]+)\s*,", text, re.M)}
    for path in args.known:
        known |= {int(m, 16) for m in re.findall(r"^(0x[0-9A-Fa-f]+)\s*=", path.read_text(encoding="utf-8"), re.M)}

    lines = [f"# tools/find_functions.py: {args.input.name}, base {base:#010x}, {len(img.pdata)} .pdata functions"]
    setjmp, longjmp = jmp_buf_routines(img)
    if setjmp:
        lines.append(f"# setjmp_address = 0x{setjmp:08X}")
    if longjmp:
        lines.append(f"# longjmp_address = 0x{longjmp:08X}")

    if args.codegen_log:
        log = args.codegen_log.read_text(encoding="utf-8", errors="replace")
        unresolved = sorted({int(t, 16) for t in re.findall(r"(0x[0-9A-Fa-f]{8}) from 0x[0-9A-Fa-f]{8}: b", log)})
        lines.append(f"\n# --- UnresolvedCall targets ({len(unresolved)})")
        lines += [f"0x{t:08X} = {{}}" for t in unresolved if t not in known]
    else:
        for title, refs in (("data pointers (vtables, callbacks, thread entries)", data_pointer_targets(img)),
                            ("addresses built in code (lis + addi/ori)", built_address_targets(img))):
            found = sorted(t for t in refs
                           if t not in known and not inside_pdata_function(t) and img.looks_like_start(t))
            lines.append(f"\n# --- {title} ({len(found)})")
            lines += [f"0x{t:08X} = {{}}  # {len(refs[t])} ref(s), e.g. 0x{refs[t][0]:08X}" for t in found]

    text = "\n".join(lines) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
