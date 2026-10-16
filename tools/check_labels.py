#!/usr/bin/env python3
"""Find generated functions that `goto` a label they do not define.

That happens when a [functions] entry starts a new function in the middle of another one (typically a
switch case block that looks like a thunk): the codegen does not warn, the C++ compile fails later with
"use of undeclared label". Run after tools/codegen.ps1; exit status 1 if any is found.

    python -I tools/check_labels.py
"""
import glob
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
bad = []
for path in sorted(glob.glob(str(ROOT / "generated" / "*_recomp.*.cpp"))):
    fn, gotos, labels = None, set(), set()
    for line in open(path, encoding="utf-8", errors="replace"):
        m = re.match(r"PPC_FUNC_IMPL\(__imp__sub_([0-9A-F]{8})\)", line)
        if m:
            if fn:
                bad += [(fn, g) for g in sorted(gotos - labels)]
            fn, gotos, labels = m[1], set(), set()
            continue
        gotos.update(re.findall(r"goto loc_([0-9A-F]{8});", line))
        m = re.match(r"loc_([0-9A-F]{8}):", line)
        if m:
            labels.add(m[1])
    if fn:
        bad += [(fn, g) for g in sorted(gotos - labels)]

for fn, label in bad:
    print(f"sub_{fn}: goto loc_{label} outside the function (a [functions] entry at 0x{label} splits it?)")
print(f"{len(bad)} missing label(s)")
sys.exit(1 if bad else 0)
