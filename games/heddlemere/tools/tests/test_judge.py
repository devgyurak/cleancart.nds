# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of tools/judge.py against expected.json (run: python3 -m unittest discover tools/tests)."""

import json
import os
import sys
import unittest

TOOLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
sys.path.insert(0, TOOLS)

import judge  # noqa: E402

EXPECTED = os.path.join(TOOLS, '..', 'expected.json')


def items():
    with open(EXPECTED) as f:
        return {i['id']: i for i in json.load(f)['items']}


class GradeTest(unittest.TestCase):
    def test_signed_negative_difference(self):
        it = items()['gx-scale-mode2-extra']
        vals = [1] + [0] * 63
        vals[13] = 0xFFFFFFFF   # -1 as a 32-bit word
        self.assertTrue(judge.grade(it, vals)[0])
        vals[13] = 0xFFFFFFE0   # -32
        self.assertFalse(judge.grade(it, vals)[0])

    def test_two_value_eq(self):
        it = items()['swap-ramcount-line']
        vals = [1] + [0] * 63
        vals[18], vals[19] = 64, 0
        self.assertTrue(judge.grade(it, vals)[0])
        vals[19] = 64
        self.assertFalse(judge.grade(it, vals)[0])

    def test_range_edges(self):
        it = items()['line-cycles']
        vals = [1] + [0] * 63
        vals[1] = it['min']
        self.assertTrue(judge.grade(it, vals)[0])
        vals[1] = it['min'] - 1
        self.assertFalse(judge.grade(it, vals)[0])

    def test_done_gate(self):
        it = items()['frame-cycles']
        vals = [0] * 64
        vals[2] = 560190
        self.assertFalse(judge.grade(it, vals)[0])   # flags bit 0 clear: the measurement did not finish
        vals[0] = 1
        self.assertTrue(judge.grade(it, vals)[0])

    def test_expected_json_levels_and_roles(self):
        with open(EXPECTED) as f:
            exp = json.load(f)
        judge.check_levels(exp['items'])
        judge.check_roles(exp)


if __name__ == '__main__':
    unittest.main()
