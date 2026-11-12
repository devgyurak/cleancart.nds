# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Run: python3 -m unittest discover -s tools/tests   (from the elevane folder)"""

import glob
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import puzzle_check  # noqa: E402
from evpuzzle import model, pack, solve  # noqa: E402


def one(text):
    (p,) = model.parse(text)
    return p, p.parts[0], model.Order.of(p.parts[0])


class Views(unittest.TestCase):
    def test_views_and_support(self):
        _, s, o = one('[t] chapter=4 mode=layers grid=3x2x2\nz0 = #.#/...\nz1 = ###/...\n')
        self.assertEqual(o.front, (3, 2, 3))
        self.assertEqual(o.plan, (7, 0))
        self.assertTrue(model.supported(s.cells))
        self.assertFalse(model.supported({(0, 0, 0), (2, 0, 1)}))

    def test_floating_block_rejected(self):
        _, s, o = one('[t] chapter=4 mode=layers grid=2x1x2\nz0 = #.\nz1 = .#\n')
        self.assertTrue(any('floating' in e for e in model.check_structure(o, s.cells)))

    def test_bad_files(self):
        for bad in ('[t] chapter=1 grid=9x1x1\nh = 1\n',
                    '[t] chapter=1 mode=stack grid=2x1x2\nh = 1\n',
                    '[t] chapter=1 mode=layers grid=2x1x2\nz0 = #x\n',
                    '[t] chapter=1 grid=1x1x1\nh = 1\n[t] chapter=1 grid=1x1x1\nh = 1\n'):
            with self.assertRaises(model.PuzzleError):
                model.parse(bad)


class Minimum(unittest.TestCase):
    def test_stack_closed_form(self):
        p, s, o = one('[t] chapter=1 mode=stack grid=2x2x2\nh = 22/22\n')
        r = solve.solve(o, p.mode, s)
        self.assertEqual((r.count, r.exact), (6, True))   # 2 towers of 2, 2 singles
        self.assertEqual(solve.validate(o, p.mode, r.cells), [])

    def test_stack_gap_is_infeasible(self):
        o = model.Order(1, 1, 3, (0b101,), (1,))
        with self.assertRaises(solve.Infeasible):
            solve.solve(o, 'stack')

    def test_arch(self):
        p, s, o = one('[t] chapter=4 mode=layers grid=3x1x2\nz0 = #.#\nz1 = ###\n')
        r = solve.solve(o, p.mode, s)
        self.assertEqual((r.count, r.exact), (5, True))
        self.assertEqual(solve.validate(o, p.mode, r.cells), [])

    def test_support_costs_a_block(self):
        # The maker fills all 8 cells. The views alone need 4 (two per x), but any 4
        # leave a top block floating, so the minimum is 5; the maker's 8 is beaten.
        p, s, o = one('[t] chapter=4 mode=layers grid=2x2x2\nz0 = ##/##\nz1 = ##/##\n')
        r = solve.solve(o, p.mode, s)
        self.assertEqual((r.count, r.exact), (5, True))
        self.assertEqual(solve.validate(o, p.mode, r.cells), [])

    def test_layers_infeasible(self):
        o = model.Order(3, 1, 2, (0b01, 0b10, 0b01), (0b111,))
        with self.assertRaises(solve.Infeasible):
            solve.solve(o, 'layers')

    def test_node_limit_marks_reference(self):
        p, s, o = one('[t] chapter=4 mode=layers grid=3x3x3\n'
                      'z0 = ###/###/###\nz1 = ###/###/###\nz2 = ###/###/###\n')
        r = solve.solve(o, p.mode, s, node_limit=3)
        self.assertFalse(r.exact)
        self.assertLess(r.count, 27)   # pruned, not the maker's 27
        self.assertEqual(solve.validate(o, p.mode, r.cells), [])


class Data(unittest.TestCase):
    def files(self):
        return sorted(glob.glob(os.path.join(os.path.dirname(HERE), 'puzzles', 'ch*.txt')))

    def test_every_shipped_puzzle_passes(self):
        rows, _, ok = puzzle_check.check(puzzle_check.load(self.files()))
        self.assertTrue(ok, [r for r in rows if r['problems']])

    def test_output_is_deterministic(self):
        blobs = []
        for _ in range(2):
            _, entries, _ = puzzle_check.check(puzzle_check.load(self.files()))
            blobs.append(pack.pack(entries))
        self.assertEqual(blobs[0], blobs[1])

    def test_chapter1_has_eight(self):
        ps = [p for p in puzzle_check.load(self.files()) if p.chapter == 1]
        self.assertEqual(len(ps), 8)


if __name__ == '__main__':
    unittest.main()
