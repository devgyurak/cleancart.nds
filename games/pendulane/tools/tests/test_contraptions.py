#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the contraption checker (tools/pdorder/check.py) and of the shipped contraption files.

    python3 games/pendulane/tools/tests/test_contraptions.py
"""

import glob
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
from pdorder import check, model  # noqa: E402

FILES = sorted(glob.glob(os.path.join(HERE, '..', 'contraptions', 'ch*.txt')))


def device(chapter, target, *slots):
    return model.Contraption(chapter, 'TEST', target, list(slots), 1)


class Checker(unittest.TestCase):
    def test_every_shipped_contraption_passes(self):
        rows, ok = check.check(check.load(FILES))
        self.assertTrue(ok, [r for r in rows if r['problems']])
        self.assertEqual(len(rows), 20)

    def test_every_chapter_has_four_contraptions(self):
        by = {}
        for c in check.load(FILES):
            by[c.chapter] = by.get(c.chapter, 0) + 1
        self.assertEqual(by, {1: 4, 2: 4, 3: 4, 4: 4, 5: 4})

    def test_an_unsolvable_target_is_an_error(self):
        rows, ok = check.check([device(1, 7, 'DH', 'DR')])
        self.assertFalse(ok)
        self.assertIn('no plan adds up to 7', rows[0]['problems'][0])

    def test_a_starting_plan_that_already_solves_it_is_an_error(self):
        rows, ok = check.check([device(1, 36, 'DH', 'DH')])   # D + D = 36
        self.assertFalse(ok)
        self.assertIn('starting plan', rows[0]['problems'][0])

    def test_unknown_station_and_repeated_station(self):
        rows, ok = check.check([device(1, 36, 'DZ', 'DD')])
        self.assertFalse(ok)
        self.assertTrue(any('no station' in p for p in rows[0]['problems']))
        self.assertTrue(any('repeats' in p for p in rows[0]['problems']))

    def test_par_is_the_fewest_slots_to_change(self):
        rows, ok = check.check([device(1, 52, 'DH', 'DR')])   # D+D = 36 is the start; H+D = 52 changes one slot
        self.assertTrue(ok)
        self.assertEqual(rows[0]['par'], 1)
        rows, ok = check.check([device(1, 47, 'DH', 'DR')])   # H+R = 47 changes both
        self.assertTrue(ok)
        self.assertEqual(rows[0]['par'], 2)

    def test_book_costs_are_the_gbatek_figures(self):
        st = model.CHAPTERS[1][1]
        self.assertEqual((st['D'][1], st['H'][1], st['T'][1], st['R'][1]), (18, 34, 34, 13))
        self.assertEqual(model.CHAPTERS[2][1]['E'][1] - model.CHAPTERS[2][1]['A'][1], 56)   # 448 cycles / 8
        self.assertEqual(model.CHAPTERS[3][1]['L'][1] * 16, 1280)
        self.assertEqual(model.CHAPTERS[3][1]['W'][1] * 16, 2048)


if __name__ == '__main__':
    unittest.main()
