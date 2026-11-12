# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s scripts/tests   (from the repository root)"""

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import mklevels  # noqa: E402


def item(**kw):
    base = {'id': 'a', 'level': 1, 'role': 'target', 'level_basis': 'none', 'source': 'rom'}
    base.update(kw)
    return base


class Roles(unittest.TestCase):
    def test_valid(self):
        mklevels.check_item_roles('g', 2, [item(level=2), item(id='b', level=1, role='prerequisite'),
                                           item(id='c', level=4, role='relocate', to='probes')])

    def test_violations(self):
        for bad in (item(role='x'), item(level=2), item(role='prerequisite', level=3), item(level=3),
                    item(level=3, role='relocate'), item(level=3, role='relocate', to='nowhere')):
            with self.assertRaises(ValueError):
                mklevels.check_item_roles('g', 1, [bad])


class Map(unittest.TestCase):
    def test_games_load_and_map_is_deterministic(self):
        games = mklevels.load_games()
        self.assertTrue(games)
        self.assertEqual(mklevels.section_map(games), mklevels.section_map(mklevels.load_games()))

    def test_sections_are_known_names(self):
        self.assertIn('DS 3D Tests', mklevels.GBATEK_SECTIONS)
        self.assertEqual(len(mklevels.GBATEK_SECTIONS), len(set(mklevels.GBATEK_SECTIONS)))


if __name__ == '__main__':
    unittest.main()
