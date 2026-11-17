#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Writes the hardware-support copy of a ROM: the same ROM with the header game code "####".

Wood-style flash card kernels (R4 and its clones) treat a ROM whose game code is not "####" as a
retail game: they emulate a save chip for it and do not DLDI-patch it, so the ROM sees no SD card
(no QA log, no autoqa marker, no result files). The release ROM keeps its own code because melonDS
attaches no save chip to "####" (see build-and-verify.md). The copy changes only the four game code
bytes (header 0x00C) and the header CRC-16 (0x15E, over 0x000-0x15D); nothing in the game reads its
own code. On such a card the chip save is the kernel's business: without it the game runs with
defaults, as with any missing save chip.

usage: hwsupport.py IN.nds OUT.nds
"""

import struct
import sys

HOMEBREW_CODE = b'####'
CODE_AT = 0x0C
CRC_AT = 0x15E


def crc16(data):
    """CRC-16/MODBUS (poly 0xA001 reflected, init 0xFFFF), the DS header checksum."""
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def hw_support(rom):
    """Returns the hardware-support copy of the ROM bytes; raises ValueError on a bad header."""
    rom = bytearray(rom)
    if len(rom) < 0x200:
        raise ValueError('too short for a DS header')
    if crc16(rom[:CRC_AT]) != struct.unpack_from('<H', rom, CRC_AT)[0]:
        raise ValueError('header CRC does not match: not a DS ROM, or already damaged')
    rom[CODE_AT:CODE_AT + 4] = HOMEBREW_CODE
    struct.pack_into('<H', rom, CRC_AT, crc16(rom[:CRC_AT]))
    return bytes(rom)


def main(argv):
    if len(argv) != 2:
        print(__doc__.strip().splitlines()[-1], file=sys.stderr)
        return 2
    src, dst = argv
    with open(src, 'rb') as f:
        data = f.read()
    try:
        out = hw_support(data)
    except ValueError as e:
        print('hwsupport: %s: %s' % (src, e), file=sys.stderr)
        return 1
    with open(dst, 'wb') as f:
        f.write(out)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
