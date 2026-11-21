# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""hwsupport.py changes the game code and the header CRC of a ROM and nothing else."""

import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import hwsupport  # noqa: E402


def fake_rom():
    rom = bytearray(0x400)
    rom[0x0C:0x10] = b'PRBS'
    rom[0x200:0x210] = bytes(range(16))
    struct.pack_into('<H', rom, 0x15E, hwsupport.crc16(rom[:0x15E]))
    return bytes(rom)


class HwSupport(unittest.TestCase):
    def test_only_code_and_crc_change(self):
        a = fake_rom()
        b = hwsupport.hw_support(a)
        self.assertEqual(b[0x0C:0x10], b'####')
        diff = [i for i in range(len(a)) if a[i] != b[i]]
        self.assertTrue(all(0x0C <= i < 0x10 or 0x15E <= i < 0x160 for i in diff))
        self.assertEqual(struct.unpack_from('<H', b, 0x15E)[0], hwsupport.crc16(b[:0x15E]))

    def test_bad_crc_is_refused(self):
        rom = bytearray(fake_rom())
        rom[0x20] ^= 1
        with self.assertRaises(ValueError):
            hwsupport.hw_support(bytes(rom))


if __name__ == '__main__':
    unittest.main()
