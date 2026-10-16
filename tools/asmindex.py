#!/usr/bin/env python3
"""Index the PPC disassembly comments in generated/*.cpp: function address -> instruction list.

    python -I tools/asmindex.py build              # writes work/asm.json
    python -I tools/asmindex.py show 0x82B92348    # print one function
    python -I tools/asmindex.py grep REGEX [--max N]   # functions with a matching instruction

Each function keeps (address, text) pairs, the address taken from `ctx.lr = ...` after calls is not
available for every line, so instructions are numbered from the function start (4 bytes each); good
enough because the generator emits one comment per instruction in order (labels aside).
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "generated"
INDEX = ROOT / "work" / "asm.json"
FUNC_RE = re.compile(r"^PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\)")
LABEL_RE = re.compile(r"^loc_([0-9A-F]{8}):")
ASM_RE = re.compile(r"^\t// (.+)$")


def build() -> None:
    funcs: dict[str, list[list]] = {}
    for path in sorted(GEN.glob("*_recomp.*.cpp")):
        cur = None
        addr = 0
        for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
            m = FUNC_RE.match(line)
            if m:
                cur = funcs.setdefault(m[1], [])
                addr = int(m[1], 16)
                continue
            if cur is None:
                continue
            m = LABEL_RE.match(line)
            if m:
                addr = int(m[1], 16)
                continue
            m = ASM_RE.match(line)
            if m:
                cur.append([addr, m[1].strip()])
                addr += 4
    INDEX.parent.mkdir(exist_ok=True)
    INDEX.write_text(json.dumps(funcs))
    print(f"{len(funcs)} functions indexed -> {INDEX}")


def load() -> dict[str, list[list]]:
    return json.loads(INDEX.read_text())


def show(addr: str) -> None:
    key = f"{int(addr, 16):08X}"
    for a, text in load().get(key, []):
        print(f"{a:08X}  {text}")


def grep(pattern: str, max_hits: int) -> None:
    rx = re.compile(pattern)
    hits = 0
    for key, insns in load().items():
        lines = [f"{a:08X}  {t}" for a, t in insns if rx.search(t)]
        if lines:
            print(f"sub_{key} ({len(insns)} insns): " + " | ".join(lines[:4]))
            hits += 1
            if hits >= max_hits:
                break


if __name__ == "__main__":
    cmd = sys.argv[1]
    if cmd == "build":
        build()
    elif cmd == "show":
        show(sys.argv[2])
    elif cmd == "grep":
        n = int(sys.argv[sys.argv.index("--max") + 1]) if "--max" in sys.argv else 50
        grep(sys.argv[2], n)
