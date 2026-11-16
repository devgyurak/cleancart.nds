#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py: the checks ported for the level 2 demo boots, n/a and profiles.

    python3 games/lightenette/tools/tests/test_judge.py
"""

import io
import json
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
import judge  # noqa: E402

EXPECTED = os.path.join(HERE, '..', '..', 'expected.json')


def block_line(name, item_id, values):
    words = [judge_magic(), item_id, 121] + list(values) + [0] * (61 - len(values))
    return '%s RESULT %s' % (name, ' '.join('%08x' % w for w in words))


def judge_magic():
    with open(EXPECTED) as f:
        return json.load(f)['block']['magic']


def boot_id(boot):
    with open(EXPECTED) as f:
        return json.load(f)['boots'][boot]['item_id']


def run_judge(args, log_text):
    with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
        f.write(log_text)
        path = f.name
    try:
        out = io.StringIO()
        with redirect_stdout(out):
            rc = judge.main(args + [path])
        return rc, out.getvalue()
    finally:
        os.unlink(path)


class GradeChecks(unittest.TestCase):
    vals = [0b1010, 7, 7, 7, 0, 5]

    def g(self, **item):
        item.setdefault('id', 't')
        return judge.grade(item, self.vals)[0]

    def test_mask(self):
        self.assertTrue(self.g(values=[0], check='mask', mask=2, expect=2))
        self.assertFalse(self.g(values=[0], check='mask', mask=4, expect=4))

    def test_nonzero(self):
        self.assertTrue(self.g(values=[0], check='nonzero', mask=8))
        self.assertFalse(self.g(values=[0], check='nonzero', mask=5))

    def test_same_and_ne(self):
        self.assertTrue(self.g(values=[1, 2, 3], check='same'))
        self.assertFalse(self.g(values=[0, 1], check='same'))
        self.assertTrue(self.g(values=[0, 1], check='ne'))
        self.assertFalse(self.g(values=[1, 2], check='ne'))

    def test_not_value(self):
        self.assertFalse(self.g(values=[1], check='eq', expect=7, not_value=7))

    def test_na_when(self):
        self.assertTrue(judge.is_na({'na_when': [4, 0]}, self.vals))
        self.assertFalse(judge.is_na({'na_when': [5, 0]}, self.vals))
        self.assertFalse(judge.is_na({}, self.vals))

    def test_range(self):
        self.assertTrue(self.g(values=[1], check='range', min=7, max=7))
        self.assertFalse(self.g(values=[1], check='range', min=8, max=9))

    def test_unknown_check(self):
        with self.assertRaises(judge.Fatal):
            self.g(values=[0], check='bogus')


class Level2Boots(unittest.TestCase):
    def test_boot_ids_are_fnv1a_of_the_prefixed_name(self):
        with open(EXPECTED) as f:
            boots = json.load(f)['boots']
        for name, b in boots.items():
            if not name.startswith('l2-'):
                continue
            h = 2166136261
            for c in name.encode():
                h = ((h ^ c) * 16777619) & 0xffffffff
            self.assertEqual(b['item_id'], h, name)

    def test_sys_irq_gate(self):
        # values[1] is the HBlank count: the loose range 31297..31560
        for hb, want in ((31557, 0), (31296, 1), (31561, 1)):
            log = block_line('lightenette-l2-sys-irq', boot_id('l2-sys-irq'), [120, hb, 19, 120])
            rc, _ = run_judge([], log)
            self.assertEqual(rc, want, hb)

    def test_acc_dual_masks(self):
        log = block_line('x', boot_id('l2-acc-dual'), [1, 0, 0, 0x1F, 0x1000, 0x2000])
        self.assertEqual(run_judge([], log)[0], 0)
        log = block_line('x', boot_id('l2-acc-dual'), [1, 0, 0, 0x19, 0x1000, 0x2000])   # no "one pass overflows"
        rc, out = run_judge([], log)
        self.assertEqual(rc, 1)
        self.assertIn('acc-dual-over', out)

    def test_pixel_item_needs_the_capture(self):
        # no bad pixels but the capture did not finish: NG, not a pass
        log = block_line('x', boot_id('l2-2d-mosaic'), [0, 0, 0, 0, 49152, 0, 0, 0])
        self.assertEqual(run_judge([], log)[0], 1)
        log = block_line('x', boot_id('l2-2d-mosaic'), [0, 0, 0, 0, 49152, 0, 1, 0])
        self.assertEqual(run_judge([], log)[0], 0)

    def test_na_without_a_profile_is_not_graded(self):
        log = block_line('x', boot_id('l2-3d-limits'), [0, 0, 0, 0])   # no polygon counters (DeSmuME)
        rc, out = run_judge([], log)
        self.assertEqual(rc, 1)
        self.assertIn('NOT GRADED', out)
        self.assertEqual(run_judge(['--allow-na'], log)[0], 0)
        self.assertEqual(run_judge(['--profile', 'desmume-pydesmume-0.0.9'], log)[0], 0)
        # the same block on an emulator that must measure it
        self.assertEqual(run_judge(['--profile', 'melonds-1.1-software'], log)[0], 1)

    def test_snd_wave_pan_exact(self):
        # values[4] swing at pan 126, values[5] the pan 0 reference, values[6] bit 0 the guard
        for right, left, want in ((2, 199, 0), (1, 199, 0), (3, 199, 1), (0, 0, 1)):
            log = block_line('x', boot_id('l2-snd-wave'), [left, right, 0, 0, right, left, int(left > 16)])
            self.assertEqual(run_judge([], log)[0], want, (right, left))
        # a silent record (swing 0 at pan 126 too) must not pass: guard bit clear
        log = block_line('x', boot_id('l2-snd-wave'), [0, 0, 0, 0, 0, 0, 0])
        rc, out = run_judge([], log)
        self.assertEqual(rc, 1)
        self.assertIn('snd-wave-pan-right-exact', out)
        # melonDS reads 3 on 1.1: a known NG there, not on DeSmuME
        log = block_line('x', boot_id('l2-snd-wave'), [199, 3, 0, 0, 3, 199, 1])
        self.assertEqual(run_judge(['--profile', 'melonds-1.1-software'], log)[0], 0)
        self.assertEqual(run_judge(['--profile', 'desmume-pydesmume-0.0.9'], log)[0], 1)

    def test_profile_ignores_other_boots(self):
        log = block_line('x', boot_id('l2-sys-math'), [0, 1000000, 0x5000, 0x1000])
        self.assertEqual(run_judge(['--profile', 'melonds-1.1-gl-classic'], log)[0], 0)


class Batch(unittest.TestCase):
    def test_unfinished_block_never_replaces_a_finished_one(self):
        good = 'a-rom RESULT ' + ' '.join('%08x' % w for w in [judge_magic(), 1, 5] + [0] * 8) + '\n'
        zero = 'b-rom RESULT ' + ' '.join(['00000000'] * 11) + '\n'
        self.assertEqual(judge.parse_words(good + zero)[0], judge_magic())
        self.assertEqual(judge.parse_words(zero + good)[0], judge_magic())
        self.assertFalse(any(judge.parse_words(zero)))


if __name__ == '__main__':
    unittest.main()
