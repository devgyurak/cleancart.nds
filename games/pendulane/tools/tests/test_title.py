#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the title art generator (tools/cvtools/pdart.py): sizes, determinism, palette indices.

    python3 games/pendulane/tools/tests/test_title.py
"""

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
from cvtools import pdart  # noqa: E402


class Lettering(unittest.TestCase):
    def test_sizes(self):
        tiles, entries, pal = pdart.title_lettering()
        self.assertEqual(len(tiles), (pdart.COLS * pdart.ROWS + 2) * 32)   # the lettering, a blank and a panel tile
        self.assertEqual(len(entries), pdart.COLS * pdart.ROWS)
        self.assertEqual(len(pal), 32)
        self.assertLessEqual(pdart.COLS, 32)

    def test_map_points_at_its_palette_line_and_tiles(self):
        _, entries, _ = pdart.title_lettering()
        for i, e in enumerate(entries):
            self.assertEqual(e & 0x3FF, i)
            self.assertEqual(e >> 12, pdart.PAL_LINE)

    def test_same_bytes_every_time(self):
        self.assertEqual(pdart.title_lettering(), pdart.title_lettering())
        self.assertEqual(pdart.sprites(), pdart.sprites())

    def test_every_pixel_uses_a_defined_colour(self):
        tiles, _, _ = pdart.title_lettering()
        used = {b & 15 for b in tiles} | {b >> 4 for b in tiles}
        self.assertLessEqual(max(used), len(pdart.COLOURS) - 1)
        self.assertIn(pdart.OUTLINE, used)
        self.assertIn(pdart.SHINE, used)

    def test_sprites(self):
        bob, dot, pal = pdart.sprites()
        self.assertEqual(len(bob), 32 * 32 // 2)
        self.assertEqual(len(dot), 8 * 8 // 2)
        self.assertEqual(len(pal), 32)
        used = {b & 15 for b in bob} | {b >> 4 for b in bob} | {b & 15 for b in dot} | {b >> 4 for b in dot}
        self.assertLessEqual(max(used), len(pdart.OBJ_COLOURS) - 1)


if __name__ == '__main__':
    unittest.main()
