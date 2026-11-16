# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The ROM blob `cv_orders`.
    u32  magic 'HMOR' (0x524F4D48)
    u32  count
    count x 40 bytes: u8 chapter, u8 index, u8 budget (lines), u8 passes, u8 slack, u8 reserved, u16 par,
                      u8 colour[4], u8 weave[4], u8 dye[4], u8 reserved[4], char name[16] (NUL padded)
Entries are in chapter, index order.
"""
import struct

from . import model

MAGIC = 0x524F4D48
ENTRY = 40


def pack(orders):
    out = bytearray(struct.pack('<II', MAGIC, len(orders)))
    for o in orders:
        name = o.name.encode('ascii')
        assert len(name) < 16
        out += struct.pack('<6BH12B4x16s', o.chapter, o.index, o.budget, o.passes, o.slack, 0, model.par(o),
                           *[r[0] for r in o.rows], *[r[1] for r in o.rows], *[r[2] for r in o.rows], name)
    return bytes(out)
