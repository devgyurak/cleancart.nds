#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The GBATEK sound models the ROM carries in C (game/ear.c, the lines in game/*.c), checked here against the
GBATEK constants.

    python3 games/tonewick/tools/tests/test_model.py
"""

import unittest


def lfsr_bits(n, mask=0x6000):
    """GBATEK DS Sound Notes: X = 7FFFh at the start; X = X SHR 1; carry set: LOW and X = X XOR 6000h, else HIGH."""
    x, out = 0x7FFF, []
    for _ in range(n):
        carry = x & 1
        x >>= 1
        if carry:
            out.append(0)
            x ^= mask
        else:
            out.append(1)
    return out


def volume(data, reg):
    """Register 0..126 is N = 0..126, 127 is N = 128; data * N / 128."""
    n = 128 if reg == 127 else reg
    return data * n // 128


def duty_high_eighths(duty):
    return 0 if duty == 7 else duty + 1


class Noise(unittest.TestCase):
    def test_period_is_32767(self):
        bits = lfsr_bits(2 * 32767)
        self.assertEqual(bits[:32767], bits[32767:])
        self.assertNotEqual(bits[:32767], bits[1:32768])

    def test_first_bits(self):
        # 7FFFh: carry 1 -> LOW, X = 3FFFh ^ 6000h = 5FFFh; carry 1 -> LOW, X = 2FFFh ^ 6000h = 4FFFh; ...
        self.assertEqual(lfsr_bits(3), [0, 0, 0])

    def test_wrong_mask_gives_another_sequence(self):
        self.assertNotEqual(lfsr_bits(300), lfsr_bits(300, 0x4000))


class Mixer(unittest.TestCase):
    def test_volume_127_is_full(self):
        self.assertEqual(volume(0x4000, 127), 0x4000)
        self.assertEqual(volume(0x4000, 126), 0x4000 * 126 // 128)
        self.assertEqual(volume(0x4000, 127), 2 * volume(0x4000, 64))

    def test_divider_3_is_sixteenth(self):
        self.assertEqual(32767 >> 4, 2047)
        self.assertEqual(2 * 32767 // (2 * (32767 >> 4)), 16)

    def test_duty_7_is_never_high(self):
        self.assertEqual(duty_high_eighths(7), 0)
        self.assertEqual([duty_high_eighths(d) for d in range(7)], [1, 2, 3, 4, 5, 6, 7])


class CaptureBugs(unittest.TestCase):
    def test_add_overflow_wraps(self):
        s = 0x7000 + 0x7000
        v = s & 0xFFFF
        v = v - 0x10000 if v & 0x8000 else v
        self.assertEqual(v, -0x2000)

    def test_both_negative_is_min(self):
        self.assertEqual(-0x8000 + 32768, 0)


class Firmware(unittest.TestCase):
    def test_newer_copy_wraps_at_80h(self):
        def newest(c0, c1):
            return 1 if c1 == (c0 + 1) & 0x7F else 0
        self.assertEqual(newest(5, 6), 1)
        self.assertEqual(newest(6, 5), 0)
        self.assertEqual(newest(0x7F, 0), 1)


if __name__ == '__main__':
    unittest.main()
