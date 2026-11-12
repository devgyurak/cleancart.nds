# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Puzzle data -> ROM blob.

Layout (little endian, all offsets in bytes from the start of the blob):

  u32 magic 'EVPZ' (0x5A505645)
  u16 version (1)  u16 puzzle count
  count x { u32 offset }                      one per puzzle, in file order
  puzzle:
    u8 chapter  u8 index in chapter (1-based)  u8 mode (0 stack, 1 layers)
    u8 parts    u8 flags (bit 0: par is the maker's count, not proven minimal)
    u8 name length  name bytes (ASCII, upper case)  pad to 4 bytes
    parts x part:
      u8 w  u8 d  u8 h  u8 par blocks (this part)
      w bytes  front view (bit z of byte x)
      d bytes  plan view  (bit x of byte y)
      a minimal solution (the maker's when par is not proven): u8 count, then
      count x { u8 x, u8 y, u8 z }
      pad to 4 bytes

The solution ships so the game can show a hint and the QA run can play every puzzle
without a search.
"""

import struct

MAGIC = 0x5A505645


def _pad(b):
    while len(b) % 4:
        b.append(0)


def pack(entries):
    """entries: list of (puzzle, [(order, result, maker_structure), ...]) in file order.
    Returns bytes."""
    blobs = []
    per_chapter = {}
    for puzzle, parts in entries:
        idx = per_chapter.get(puzzle.chapter, 0) + 1
        per_chapter[puzzle.chapter] = idx
        name = puzzle.name.upper().encode('ascii')
        if len(name) > 24:
            raise ValueError('puzzle name too long: %s' % puzzle.name)
        flags = 0 if all(r.exact for (_, r, _) in parts) else 1
        out = bytearray([puzzle.chapter, idx, 0 if puzzle.mode == 'stack' else 1,
                         len(parts), flags, len(name)])
        out += name
        _pad(out)
        for order, res, _maker in parts:
            out += bytes([order.w, order.d, order.h, res.count])
            out += bytes(order.front) + bytes(order.plan)
            cells = sorted(res.cells, key=lambda c: (c[2], c[1], c[0]))
            out += bytes([len(cells)])
            for (x, y, z) in cells:
                out += bytes([x, y, z])
            _pad(out)
        blobs.append(bytes(out))
    head = 8 + 4 * len(blobs)
    out = bytearray(struct.pack('<IHH', MAGIC, 1, len(blobs)))
    off = head
    for b in blobs:
        out += struct.pack('<I', off)
        off += len(b)
    for b in blobs:
        out += b
    _pad(out)
    return bytes(out)
