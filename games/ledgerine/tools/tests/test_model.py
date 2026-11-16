# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The GBATEK rule model against the constants of the Ledgerine expected.json items.

    python3 -m unittest discover -s games/ledgerine/tools/tests
"""

import os
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))

import ledger_model as m  # noqa: E402

MIN32 = -(1 << 31)
MIN64 = -(1 << 63)


class Div0(unittest.TestCase):
    def test_mode0_positive(self):
        self.assertEqual(m.div(0, 7, 0), (0x00000000FFFFFFFF, 7, True))

    def test_mode0_negative(self):
        res, rem, zero = m.div(0, -7, 0)
        self.assertEqual(res, 0xFFFFFFFF00000001)
        self.assertEqual(m.s32(rem), -7)
        self.assertTrue(zero)

    def test_mode2(self):
        self.assertEqual(m.div(2, 7, 0), (m.M64, 7, True))
        self.assertEqual(m.div(2, -7, 0)[0], 1)

    def test_flag_follows_the_whole_denominator(self):
        self.assertFalse(m.div(0, 7, 1 << 32)[2])
        self.assertEqual(m.div(0, 7, 1 << 32)[1], 7)   # still a divide by zero below

    def test_overflow(self):
        self.assertEqual(m.div(0, MIN32, -1)[0], 0x80000000)
        self.assertEqual(m.div(2, MIN64, -1)[0], 1 << 63)
        self.assertFalse(m.div(2, MIN64, -1)[2])

    def test_mode3_is_mode1(self):
        self.assertEqual(m.div(3, 0x100000007, 7), m.div(1, 0x100000007, 7))
        self.assertEqual(m.div(1, 0x100000007, 7)[0], 613566757)

    def test_plain(self):
        self.assertEqual(m.div(0, 1000, 7)[:2], (142, 6))
        self.assertEqual(m.s64(m.div(0, -1234, 10)[0]), -123)


class Sqrt(unittest.TestCase):
    def test_max(self):
        self.assertEqual(m.sqrt(1, (1 << 64) - 1), 0xFFFFFFFF)

    def test_mode0_ignores_upper_half(self):
        self.assertEqual(m.sqrt(0, 0xFFFFFFFF00000010), 4)

    def test_small(self):
        self.assertEqual(m.sqrt(1, 10 ** 12), 10 ** 6)


if __name__ == '__main__':
    unittest.main()
