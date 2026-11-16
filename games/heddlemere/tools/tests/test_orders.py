# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the loom model and the order files (run: python3 -m unittest discover tools/tests)."""

import glob
import os
import sys
import unittest

TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, TOOLS)

import order_check  # noqa: E402
from hmorder import model, pack  # noqa: E402


def orders():
    return order_check.load(glob.glob(os.path.join(TOOLS, 'orders', 'ch*.txt')))


class ModelTest(unittest.TestCase):
    def test_ticks_are_the_gbatek_sums(self):
        self.assertEqual(model.TICKS, [19, 75, 105, 146])

    def test_valid_needs_weave_and_budget(self):
        self.assertTrue(model.valid(1, 1, 0, 3, 64))
        self.assertFalse(model.valid(1, 0, 0, 3, 64))     # too weak a program
        self.assertFalse(model.valid(3, 3, 0, 4, 64))     # 9344 ticks > 4 lines
        self.assertTrue(model.valid(3, 3, 1, 4, 64))      # the lever adds 10 lines

    def test_all_orders_pass_the_checker(self):
        rows, ok = order_check.check(orders())
        self.assertTrue(ok, [r for r in rows if r['problems']])
        self.assertEqual(len(rows), 40)

    def test_chapters_use_only_their_columns(self):
        for o in orders():
            levers = sum(model.row_plan(o, r)[2] for r in range(model.ROWS))
            if o.chapter < 4:
                self.assertEqual(levers, 0, o.id)
            if o.chapter < 5:
                self.assertFalse(any(r[2] for r in o.rows), o.id)

    def test_pack_layout(self):
        blob = pack.pack(orders())
        self.assertEqual(len(blob), 8 + 40 * pack.ENTRY)

    def test_plan_hash_is_stable(self):
        self.assertEqual(model.plan_hash(orders()), model.plan_hash(orders()))


if __name__ == '__main__':
    unittest.main()
