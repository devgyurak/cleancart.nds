# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The ROM blob `cv_windows`.

    u32  magic 'PVWD' (0x44575650)
    u32  count
    count x 32 bytes: u8 chapter, u8 index, u8 par, u8 reserved,
                      u8 target[12], char name[16] (NUL padded)
Entries are in chapter, index order.
"""

import struct

from . import model, solve

MAGIC = 0x44575650
ENTRY = 32


def pack(windows):
    out = bytearray(struct.pack('<II', MAGIC, len(windows)))
    for w in windows:
        name = w.name.encode('ascii')
        assert len(name) < 16
        out += struct.pack('<4B12B16s', w.chapter, w.index, solve.par(w.target), 0,
                           *w.target, name)
    return bytes(out)
