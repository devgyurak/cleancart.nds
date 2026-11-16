# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Python mirror of the ROM's GBATEK rule models (arm9/source/game/press.c and rules.c).

Every function returns what the GBATEK text says the hardware does, so tests/test_model.py
can pin the rules to the constants of the expected.json items without an emulator. The C
model in the ROM is the same logic; keep both in step.
"""

import math

M32 = 0xFFFFFFFF
M64 = 0xFFFFFFFFFFFFFFFF


def s32(v):
    v &= M32
    return v - (1 << 32) if v & 0x80000000 else v


def s64(v):
    v &= M64
    return v - (1 << 64) if v & (1 << 63) else v


def _tdiv(n, d):
    """C-style division that truncates toward zero."""
    q = abs(n) // abs(d)
    return q if (n < 0) == (d < 0) else -q


def div(mode, n, d):
    """(result u64, remainder u64, div0 flag) of the DS divider. Mode 3 is mode 1."""
    if mode == 3:
        mode = 1
    if mode == 0:
        n = s32(n)
    else:
        n = s64(n)
    dd = s64(d) if mode == 2 else s32(d)
    zero = (d & M64) == 0
    if dd == 0:
        q = 1 if n < 0 else -1
        if mode == 0:
            res = ((~(q >> 63) & M32) << 32) | (q & M32)
        else:
            res = q & M64
        return res, n & M64, zero
    lo = -(1 << 31) if mode == 0 else -(1 << 63)
    if n == lo and dd == -1:
        return (0x80000000 if mode == 0 else 1 << 63), 0, zero
    q = _tdiv(n, dd)
    r = n - q * dd
    return q & M64, r & M64, zero


def sqrt(mode, param):
    v = param & M32 if mode == 0 else param & M64
    return math.isqrt(v) & M32
