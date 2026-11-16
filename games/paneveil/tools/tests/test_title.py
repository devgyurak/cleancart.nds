# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s tools/tests   (from the paneveil folder)"""

import glob
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import window_check  # noqa: E402
from cvtools import audio, pvart  # noqa: E402
from pvorder import model  # noqa: E402


class Lettering(unittest.TestCase):
    def test_sizes(self):
        tiles, entries, pal = pvart.title_lettering()
        self.assertEqual(len(entries), pvart.COLS * pvart.ROWS)
        self.assertEqual(len(tiles), (pvart.COLS * pvart.ROWS + 2) * 32)   # + blank and panel tile
        self.assertEqual(len(pal), 32)
        self.assertLessEqual(pvart.COLS, 32)
        self.assertLessEqual(pvart.COLS * pvart.ROWS + 2, 512)             # fits the 16 KB char block

    def test_palette_line_clear_of_the_text_layer(self):
        self.assertGreaterEqual(pvart.PAL_LINE, 6)                        # text lines are 0-5
        _, entries, _ = pvart.title_lettering()
        self.assertTrue(all(e >> 12 == pvart.PAL_LINE for e in entries))
        self.assertTrue(all((e & 0xFFF) < pvart.COLS * pvart.ROWS for e in entries))

    def test_indices_in_range(self):
        tiles, _, _ = pvart.title_lettering()
        nibbles = [b & 15 for b in tiles] + [b >> 4 for b in tiles]
        self.assertLessEqual(max(nibbles), pvart.PANEL)
        self.assertEqual(tiles[pvart.BLANK_TILE * 32:pvart.PANEL_TILE * 32], bytes(32))   # blank stays blank

    def test_deterministic(self):
        self.assertEqual(pvart.title_lettering(), pvart.title_lettering())


class Preset(unittest.TestCase):
    def test_values_inside_the_field_counts(self):
        self.assertEqual(len(pvart.TITLE_PRESET), model.NFIELDS)
        for v, n in zip(pvart.TITLE_PRESET, model.COUNTS):
            self.assertTrue(0 <= v < n)

    def test_no_order_asks_for_it(self):
        here = os.path.dirname(HERE)
        windows = window_check.load(sorted(glob.glob(os.path.join(here, 'orders', 'ch*.txt'))))
        for w in windows:
            self.assertNotEqual(tuple(w.target), pvart.TITLE_PRESET, w.name)

    def test_rose_is_dense_and_plain(self):
        idx = model.FIELD_INDEX
        self.assertEqual(pvart.TITLE_PRESET[idx['tint']], 0)       # clear glass, no wireframe
        self.assertEqual(pvart.TITLE_PRESET[idx['cutout']], 0)
        self.assertGreaterEqual(pvart.TITLE_PRESET[idx['rings']], 1)   # 2 or 3 rings


class Songs(unittest.TestCase):
    def test_only_the_clerestory_theme(self):
        self.assertEqual([n for n, _ in audio.SONGS], ['clerestory'])

    def test_deterministic(self):
        for i, smp in enumerate(audio.make_samples()):
            audio.SAMPLE_IDS[smp.name] = i
        a = audio.make_songs()
        self.assertEqual(a, audio.make_songs())
        self.assertTrue(any(a[0][1]))


if __name__ == '__main__':
    unittest.main()
