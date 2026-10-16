#!/usr/bin/env python3
"""WWE 2K14 Paint Tool save (00PaintTool.pt\\SaveData.Dat): unpack, checksum, repack.

The save is 4 "ZLIB" chunks (16-byte header: magic, version 0x3012, compressed size, uncompressed size, all
little-endian) whose data, concatenated, is one 0x64582C-byte big-endian buffer:

    0x000000  u32 magic 0x02A78E9A, u32 7
    0x000008  20 logo slots, 0x50468 bytes each:
                +0x00000  slot header (words 0..0x2C, 8 signed bytes at 0x30, words 0x38..0x44)
                +0x00048  256x256 image, 4 bytes per pixel
                +0x40048  info block (bytes / halfwords, 0x14 bytes) + 0x400 bytes
                +0x40460  -- (part of the 0x400 block ends at +0x4045C; see SlotSum)
                +0x4045C  128x128 thumbnail, 4 bytes per pixel (0x10000 bytes)
                +0x5045C  date: u16 year, u8 month, day, hour, minute, second, u8
                +0x50464  u32 slot checksum
    0x645828  u32 total checksum = word 0 + word 1 + the 20 slot checksums

Checksums are plain 32-bit sums, from the game's own routine sub_8341A528 (called by the Paint Tool loader
sub_82903850, which reports "Custom Logo data is damaged" when the total differs).

    python -I tools/paint_checksum.py check SaveData.Dat
"""
from __future__ import annotations

import struct
import sys
import zlib

SLOT = 0x50468
SLOTS = 20
BASE = 8
TOTAL = 0x645828
SIZE = 0x64582C
CHUNKS = 4


def unpack(data: bytes) -> bytearray:
    out = bytearray()
    o = 0
    while o + 16 <= len(data) and data[o:o + 4] == b'ZLIB':
        _ver, csize, _usize = struct.unpack_from('<III', data, o + 4)
        out += zlib.decompress(data[o + 16:o + 16 + csize])
        o += 16 + csize
    if len(out) != SIZE:
        raise ValueError(f'unexpected Paint Tool buffer size {len(out):#x}')
    return out


def pack(buf: bytes, level: int = 9) -> bytes:
    part = (len(buf) + CHUNKS - 1) // CHUNKS
    out = bytearray()
    for i in range(CHUNKS):
        piece = buf[i * part:(i + 1) * part]
        comp = zlib.compress(piece, level)
        out += b'ZLIB' + struct.pack('<III', 0x3012, len(comp), len(piece)) + comp
    return bytes(out)


def _words(buf, start, end):
    n = (end - start) // 4
    return sum(struct.unpack_from(f'>{n}I', buf, start))


def slot_sum(buf, i: int) -> int:
    s = BASE + i * SLOT
    total = 0
    # Header: words 0x00..0x2C and 0x38..0x44, signed bytes 0x30..0x37.
    total += _words(buf, s, s + 0x30) + _words(buf, s + 0x38, s + 0x48)
    total += sum(b - 256 if b >= 128 else b for b in buf[s + 0x30:s + 0x38])
    # 256x256 image as words.
    total += _words(buf, s + 0x48, s + 0x40048)
    # Info block: bytes 0,1,2,7,16,17; halfwords 3,5,8,10,12,14,18 (unaligned ones included).
    b = s + 0x40048
    total += sum(buf[b + k] for k in (0, 1, 2, 7, 16, 17))
    total += sum(struct.unpack_from('>H', buf, b + k)[0] for k in (3, 5, 8, 10, 12, 14, 18))
    # 0x400 bytes of words, from +20.
    total += _words(buf, b + 20, b + 20 + 0x400)
    # Thumbnail bytes (0x10000), from +0x414.
    total += sum(buf[b + 0x414:b + 0x414 + 0x10000])
    # Date: u16 + 6 bytes.
    e = s + 0x5045C
    total += struct.unpack_from('>H', buf, e)[0] + sum(buf[e + 2:e + 8])
    return total & 0xFFFFFFFF


def fix(buf: bytearray) -> int:
    total = struct.unpack_from('>I', buf, 0)[0] + struct.unpack_from('>I', buf, 4)[0]
    for i in range(SLOTS):
        v = slot_sum(buf, i)
        struct.pack_into('>I', buf, BASE + i * SLOT + 0x50464, v)
        total += v
    total &= 0xFFFFFFFF
    struct.pack_into('>I', buf, TOTAL, total)
    return total


def check(path: str) -> bool:
    buf = unpack(open(path, 'rb').read())
    ok = True
    for i in range(SLOTS):
        stored = struct.unpack_from('>I', buf, BASE + i * SLOT + 0x50464)[0]
        if stored != slot_sum(buf, i):
            print(f'slot {i}: stored {stored:#010x}, computed {slot_sum(buf, i):#010x}')
            ok = False
    stored_total = struct.unpack_from('>I', buf, TOTAL)[0]
    computed = fix(bytearray(buf))
    print(f'total: stored {stored_total:#010x}, computed {computed:#010x}', 'OK' if stored_total == computed else 'MISMATCH')
    return ok and stored_total == computed


if __name__ == '__main__':
    if len(sys.argv) == 3 and sys.argv[1] == 'check':
        sys.exit(0 if check(sys.argv[2]) else 1)
    print(__doc__)
