#!/usr/bin/env python3
"""Names the functions in [cp-profile] log lines (src/cp_profiler.cpp) using the linker map.

    python -I tools/cp_profile_report.py logs/run-cp-profile.log out/build/win-amd64/RelWithDebInfo/wwe2k14_tu.map

Prints, for the busiest 10 s window (most samples outside waits), self and total percentages per function.
"""
import bisect
import re
import sys


def short(sym):
    # ?Func@Class@ns@@... -> ns::Class::Func (enough to read a profile)
    if sym.startswith('?') and '@@' in sym:
        parts = sym[1:sym.index('@@')].split('@')
        return '::'.join(reversed(parts))
    return sym


def load_map(path):
    # MSVC-style map (lld-link /map): " 0001:06be7c10       ?Name@Class@@...    0000000146be8c10 f   lib:file.obj"
    syms = []
    for line in open(path, errors='ignore'):
        m = re.match(r'\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s', line)
        if m:
            syms.append((int(m.group(2), 16), short(m.group(1))))
    syms.sort()
    return syms


def main():
    log, mapfile = sys.argv[1], sys.argv[2]
    syms = load_map(mapfile)
    addrs = [a for a, _ in syms]
    image_base = 0x140000000  # lld default; offsets in the log are relative to the loaded image base

    def name(offset):
        if offset in (0, 0xFFFFFFFF):
            return '(outside the exe: system / driver code, mostly waiting)'
        va = image_base + offset
        i = bisect.bisect_right(addrs, va) - 1
        return syms[i][1] if i >= 0 else hex(offset)

    windows = {}
    for line in open(log, errors='ignore'):
        m = re.search(r'\[cp-profile\] samples=(\d+) (self|total):(.*)$', line)
        if not m:
            continue
        ts = line[:23]
        entries = [(int(o, 16), int(c)) for o, c in re.findall(r'([0-9A-F]+):(\d+)', m.group(3))]
        windows.setdefault(ts[:22], {})[m.group(2)] = (int(m.group(1)), entries)

    def busy(w):
        samples, self_entries = w.get('self', (1, []))
        waiting = sum(c for o, c in self_entries if o in (0, 0xFFFFFFFF))
        return 1 - waiting / max(samples, 1)

    ranked = sorted((k for k in windows if 'self' in windows[k] and 'total' in windows[k]), key=lambda k: busy(windows[k]),
                    reverse=True)
    for key in ranked[:2]:
        w = windows[key]
        samples = w['self'][0]
        print(f'== window {key}  samples={samples}  busy={busy(w):.0%}')
        print('-- self (own code)')
        for o, c in w['self'][1][:20]:
            print(f'{c * 100 / samples:6.1f}%  {name(o)}')
        print('-- total (with callees)')
        for o, c in w['total'][1][:40]:
            print(f'{c * 100 / samples:6.1f}%  {name(o)}')


if __name__ == '__main__':
    main()
