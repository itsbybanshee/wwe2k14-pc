#!/usr/bin/env python3
"""Names image offsets (rva:... in [re-tools] lines) with the linker map.

    python -I tools/rva_names.py out/build/win-amd64/RelWithDebInfo/wwe2k14_tu.map 35023C0 3522A90 ...
"""
import bisect
import re
import sys

syms = []
for line in open(sys.argv[1], errors='ignore'):
    m = re.match(r'\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s', line)
    if m:
        syms.append((int(m.group(2), 16), m.group(1)))
syms.sort()
addrs = [a for a, _ in syms]
for r in sys.argv[2:]:
    a = 0x140000000 + int(r, 16)
    i = bisect.bisect_right(addrs, a) - 1
    print(r, syms[i][1][:70], hex(a - syms[i][0]))
