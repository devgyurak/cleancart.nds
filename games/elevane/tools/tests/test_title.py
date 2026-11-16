# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s tools/tests   (from the elevane folder)"""

import hashlib
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

from cvtools import audio, evart  # noqa: E402
from evpuzzle import model  # noqa: E402


class TitleArt(unittest.TestCase):
    def test_size_and_palette(self):
        pix, pal = evart.title_art()
        self.assertEqual(len(pix), 256 * 192)
        self.assertEqual(len(pal), 512)
        # Entries 0-95 belong to the text layer; the picture never uses them.
        self.assertGreaterEqual(min(pix), evart.BASE)
        self.assertLessEqual(max(pix), evart.LAST)
        self.assertEqual(pal[:evart.BASE * 2], bytes(evart.BASE * 2))

    def test_text_rows_stay_dark(self):
        pix, _ = evart.title_art()
        self.assertTrue(set(pix[evart.TEXT_TOP * 256:]) <= set(range(evart.BG, evart.BG + evart.BG_N)))

    def test_clear_colours_match_play(self):
        _, pal = evart.title_art()

        def rgb(idx):
            v = pal[idx * 2] | pal[idx * 2 + 1] << 8
            return v & 31, v >> 5 & 31, v >> 10 & 31
        self.assertEqual(rgb(evart.FRONT_CLEAR), (3, 4, 10))
        self.assertEqual(rgb(evart.PLAN_CLEAR), (4, 3, 8))

    def test_deterministic(self):
        a = hashlib.sha256(b''.join(evart.title_art())).hexdigest()
        b = hashlib.sha256(b''.join(evart.title_art())).hexdigest()
        self.assertEqual(a, b)

    def test_structure_is_a_valid_order(self):
        s = evart.structure()
        self.assertTrue(model.supported(s.cells))
        self.assertEqual(model.check_structure(model.Order.of(s), s.cells), [])


class Songs(unittest.TestCase):
    def test_only_the_workbench_theme(self):
        self.assertEqual([n for n, _ in audio.SONGS], ['workbench'])

    def test_deterministic(self):
        # mkassets numbers the samples before it composes; do the same here.
        for i, smp in enumerate(audio.make_samples()):
            audio.SAMPLE_IDS[smp.name] = i
        a = audio.make_songs()
        b = audio.make_songs()
        self.assertEqual(a, b)
        name, tracks, tempo = a[0]
        self.assertTrue(any(tracks))
        self.assertGreater(tempo, 0)


if __name__ == '__main__':
    unittest.main()
