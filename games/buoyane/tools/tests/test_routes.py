#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the route checker, the packer and the almanac.

    python3 games/buoyane/tools/tests/test_routes.py
"""

import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
from byroute import model, routes  # noqa: E402

ROUTES = os.path.join(HERE, '..', 'routes')


class Almanac(unittest.TestCase):
    def test_looks_are_in_range(self):
        for ch in range(1, 6):
            for t in range(model.NTYPES):
                for s in range(model.NVIEWS):
                    self.assertIn(model.look(ch, t, s), (0, 1, 2, 3))

    def test_every_chapter_tells_types_apart(self):
        # A view may tell nothing (chapter 4 under anti-aliasing: both edges turn translucent), but each
        # chapter has at least two views that do.
        for ch in range(1, 6):
            useful = [s for s in range(model.NVIEWS)
                      if len({model.look(ch, t, s) for t in range(model.NTYPES)}) >= 2]
            self.assertGreaterEqual(len(useful), 2, ch)


class Routes(unittest.TestCase):
    def test_shipped_routes_are_solvable(self):
        chapters = routes.load_all(ROUTES)
        self.assertEqual(sum(len(a) for a in chapters.values()), 30)

    def test_pack_is_deterministic_and_sized(self):
        chapters = routes.load_all(ROUTES)
        a, b = routes.pack(chapters), routes.pack(chapters)
        self.assertEqual(a, b)
        self.assertEqual(len(a), 16 + (5 * 6 * 3 * 5 + 3) // 4 * 4)

    def test_an_ambiguous_gate_is_refused(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, 'ch1.txt')
            with open(os.path.join(ROUTES, 'ch1.txt')) as f:
                body = f.read()
            lines = body.split('\n')
            i = next(k for k, l in enumerate(lines) if l.startswith('gate'))
            lines[i] = 'gate 0 0 0 1 2'   # types 0 and 1 both look 1 in view 0 of chapter 1
            with open(path, 'w') as f:
                f.write('\n'.join(lines))
            ch, title, approaches = routes.parse(path)
            with self.assertRaises(routes.RouteError):
                routes.check(ch, title, approaches)

    def test_a_missing_true_option_is_refused(self):
        with self.assertRaises(routes.RouteError):
            routes.check(1, 'RANGE', [('A', [(0, 0, (1, 2, 3))] * 3)] * 6)


if __name__ == '__main__':
    unittest.main()
