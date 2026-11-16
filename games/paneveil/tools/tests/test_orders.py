#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the window order tools: python3 -m unittest discover -s tools/tests   (from the paneveil folder)"""

import glob
import os
import struct
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import window_check  # noqa: E402
from pvorder import model, pack, solve  # noqa: E402


class Model(unittest.TestCase):
    def test_parse_and_defaults(self):
        (w,) = model.parse('TEST: side=BACK petals=6\n', 'ch1.txt')
        self.assertEqual(w.name, 'TEST')
        self.assertEqual(w.target[model.FIELD_INDEX['side']], 1)
        self.assertEqual(w.target[model.FIELD_INDEX['petals']], 2)
        self.assertEqual(w.target[model.FIELD_INDEX['tint']], 0)

    def test_bad_input(self):
        for text in ('X: nosuch=1', 'X: side=SIDEWAYS', 'X: side=BACK side=BOTH', 'no colon', 'X: side'):
            with self.assertRaises(model.OrderError, msg=text):
                model.parse(text, 'ch1.txt')
        with self.assertRaises(model.OrderError):
            model.parse('X: side=BACK', 'notes.txt')

    def test_cyclic_distance(self):
        self.assertEqual(model.distance(model.FIELD_INDEX['petals'], 0, 4), 1)   # 5 values: 4 steps one way, 1 the other
        self.assertEqual(model.distance(model.FIELD_INDEX['flip'], 0, 2), 2)


class Solve(unittest.TestCase):
    def test_par_equals_search_and_moves(self):
        for f, n in enumerate(model.COUNTS):
            for v in range(n):
                t = [0] * model.NFIELDS
                t[f] = v
                self.assertEqual(solve.par(t), solve.bfs_par(t, [f]) or 0)
                self.assertEqual(len(solve.moves(t)), solve.par(t))

    def test_every_shipped_window_passes_the_checker(self):
        files = sorted(glob.glob(os.path.join(os.path.dirname(HERE), 'orders', 'ch*.txt')))
        rows, ok = window_check.check(window_check.load(files))
        self.assertTrue(ok, [r for r in rows if r['problems']])


class Pack(unittest.TestCase):
    def test_layout(self):
        ws = model.parse('ONE: side=BOTH\nTWO: tint=TINT\n', 'ch3.txt')
        blob = pack.pack(ws)
        magic, count = struct.unpack_from('<II', blob)
        self.assertEqual((magic, count, len(blob)), (pack.MAGIC, 2, 8 + 2 * pack.ENTRY))
        ch, idx, par = struct.unpack_from('4B', blob, 8)[:3]
        self.assertEqual((ch, idx, par), (3, 1, 1))
        self.assertEqual(blob[8 + 16:8 + 19], b'ONE')


if __name__ == '__main__':
    unittest.main()
