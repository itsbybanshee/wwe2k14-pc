#!/usr/bin/env python3
"""Move the vanilla function lists (config/*.toml) to their addresses in the title-update code (config_tu/).

The title update relinks the whole executable, so nearly every function moves. Functions are matched through
the two images' .pdata tables: each function becomes a fingerprint of its instructions with addresses masked
out (branch targets, lis/addi/ori pairs, non-stack load/store offsets), and the two fingerprint sequences are
aligned like a text diff. Addresses inside or between .pdata functions move with the function before them and
are checked against the instructions found there.

    python -I tools/port_functions.py work/image.bin work/image_tu.bin            # writes config_tu/*.toml
    python -I tools/port_functions.py work/image.bin work/image_tu.bin 0x82BB6D50  # just map addresses

Images: raw images as dumped by REX_DUMP_PATCHED_IMAGE (title update) / find_functions.py --dump-image.
"""
from __future__ import annotations

import bisect
import difflib
import hashlib
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASE = 0x82000000


def sections(img: bytes) -> dict[str, tuple[int, int]]:
    pe = struct.unpack_from('<I', img, 0x3C)[0]
    count = struct.unpack_from('<H', img, pe + 6)[0]
    opt = struct.unpack_from('<H', img, pe + 20)[0]
    out = {}
    for i in range(count):
        o = pe + 24 + opt + 40 * i
        name = img[o:o + 8].rstrip(b'\0').decode()
        size, va = struct.unpack_from('<II', img, o + 8)
        out[name] = (va, size)
    return out


def norm(word: int) -> int:
    op = word >> 26
    if op == 18:                               # b / bl
        return word & 0xFC000003
    if op == 16:                               # bc
        return word & 0xFFFF0003
    if op in (14, 15, 24, 25):                 # addi / lis / ori / oris: address halves
        return word & 0xFFFF0000
    if 32 <= op <= 55 or op in (58, 62):       # D-form loads / stores: keep stack offsets (r1)
        if (word >> 16) & 31 != 1:
            return word & 0xFFFF0000
    return word


class Image:
    def __init__(self, path: str):
        self.data = Path(path).read_bytes()
        va, size = sections(self.data)['.pdata']
        funcs = []
        for o in range(va, va + size, 8):
            start, packed = struct.unpack_from('>II', self.data, o)
            length = ((packed >> 8) & 0x3FFFFF) * 4
            if start and length:
                funcs.append((start, length))
        funcs.sort()
        self.starts = [f[0] for f in funcs]
        self.lengths = [f[1] for f in funcs]
        self.prints = [self.fingerprint(s, n) for s, n in funcs]

    def word(self, addr: int) -> int:
        return struct.unpack_from('>I', self.data, addr - BASE)[0]

    def normalized(self, addr: int, count: int) -> list[int]:
        return [norm(self.word(addr + 4 * i)) for i in range(count)]

    def fingerprint(self, start: int, length: int) -> bytes:
        words = self.normalized(start, length // 4)
        return hashlib.md5(struct.pack(f'>{len(words)}I', *words)).digest()[:8] + struct.pack('>I', length)

    def containing(self, addr: int) -> int:
        """Index of the last .pdata function starting at or before addr."""
        return bisect.bisect_right(self.starts, addr) - 1


class Mapper:
    def __init__(self, old: Image, new: Image):
        self.old, self.new = old, new
        self.func = {}       # old function index -> new function index (identical code)
        self.changed = {}    # old -> new for functions the update modified (same position in a changed run)
        sm = difflib.SequenceMatcher(None, old.prints, new.prints, autojunk=False)
        for tag, i1, i2, j1, j2 in sm.get_opcodes():
            if tag == 'equal':
                for k in range(i2 - i1):
                    self.func[i1 + k] = j1 + k
            elif tag == 'replace' and i2 - i1 == j2 - j1:
                for k in range(i2 - i1):
                    self.changed[i1 + k] = j1 + k

    def check(self, a: int, b: int, words: int) -> bool:
        return self.old.normalized(a, words) == self.new.normalized(b, words)

    def map(self, addr: int, words: int = 4) -> tuple[int | None, str]:
        """New address of addr and how it was found ('' if identical code, else a note)."""
        i = self.old.containing(addr)
        if i < 0:
            return None, 'before the first function'
        offset = addr - self.old.starts[i]
        for table, note in ((self.func, ''), (self.changed, 'function changed by the update')):
            j = table.get(i)
            if j is None:
                continue
            cand = self.new.starts[j] + offset
            if self.check(addr, cand, words):
                return cand, note
            # Between two functions (frameless code with no .pdata entry): gaps can change size; search near.
            for delta in range(4, 0x400, 4):
                for c in (cand + delta, cand - delta):
                    if self.check(addr, c, words):
                        return c, f'{note or "moved"} (searched, {delta:+#x})'
        return None, 'no match'


def port_file(mapper: Mapper, src: Path, dst: Path) -> tuple[int, int]:
    mapped = lost = 0
    out = []
    used = set()
    for line in src.read_text().splitlines():
        m = re.match(r'^(0x[0-9A-Fa-f]{8})(\s*=\s*)(.*)$', line)
        if not m:
            out.append(line)
            continue
        old = int(m.group(1), 16)
        size = re.search(r'size\s*=\s*(0x[0-9A-Fa-f]+|\d+)', m.group(3))
        words = max(1, min(8, int(size.group(1), 0) // 4)) if size else 4
        new, note = mapper.map(old, words)
        if new is not None and new in used:
            new, note = None, f'0x{new:08X} is already listed: two look-alike functions, ambiguous'
        if new is None:
            out.append(f'# {line}  # not found in the title update ({note})')
            lost += 1
            continue
        used.add(new)
        rest = m.group(3).replace(f'{old:08X}', f'{new:08X}')
        out.append(f'0x{new:08X}{m.group(2)}{rest}  # vanilla 0x{old:08X}{"; " + note if note else ""}')
        mapped += 1
    dst.write_text('\n'.join(out) + '\n')
    return mapped, lost


def main() -> int:
    old, new = Image(sys.argv[1]), Image(sys.argv[2])
    mapper = Mapper(old, new)
    print(f'functions: vanilla {len(old.starts)}, update {len(new.starts)}, identical {len(mapper.func)}, '
          f'changed {len(mapper.changed)}')
    if len(sys.argv) > 3:
        for a in sys.argv[3:]:
            addr = int(a, 16)
            new_addr, note = mapper.map(addr)
            print(f'0x{addr:08X} -> {"0x%08X" % new_addr if new_addr else "?"} {note}')
        return 0
    (ROOT / 'config_tu').mkdir(exist_ok=True)
    for src in sorted((ROOT / 'config').glob('*.toml')):
        mapped, lost = port_file(mapper, src, ROOT / 'config_tu' / src.name)
        print(f'{src.name}: {mapped} mapped, {lost} not found')
    return 0


if __name__ == '__main__':
    sys.exit(main())
