# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s tools/tests   (from the corvane folder)"""

import io
import json
import os
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import judge  # noqa: E402

EXP = os.path.join(os.path.dirname(HERE), '..', 'expected.json')


def load():
    with open(EXP) as f:
        return json.load(f)


def line(exp, name, values):
    words = [exp['block']['magic'], exp['demos'][name], 120] + values + [0] * (13 - len(values))
    return '%s RESULT %s' % (name, ' '.join('%08x' % w for w in words))


def run_judge(text, *extra):
    with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
        f.write(text)
    out, old = io.StringIO(), sys.stdout
    sys.stdout = out
    try:
        rc = judge.main(['--expected', EXP, '--json', '--partial'] + list(extra) + [f.name])
    finally:
        sys.stdout = old
        os.unlink(f.name)
    return rc, (json.loads(out.getvalue()) if out.getvalue() else None)


class Judge(unittest.TestCase):
    def test_every_item_has_a_level_and_a_known_demo(self):
        exp = load()
        judge.check_levels(exp['items'])
        for it in exp['items']:
            self.assertIn(it['demo'], exp['demos'], it['id'])
            self.assertIn(it['level_basis'].split(':')[0], ('gbatek', 'report', 'hw', 'none'), it['id'])
        self.assertEqual(len({i['id'] for i in exp['items']}), len(exp['items']))

    def test_bad_level_is_an_input_error(self):
        for bad in ({'id': 'x'}, {'id': 'x', 'level': 0}, {'id': 'x', 'level': 5}, {'id': 'x', 'level': True}):
            with self.assertRaises(judge.Fatal):
                judge.check_levels([bad])

    def test_checks(self):
        v = [5, 7, 0, 0]
        self.assertEqual(judge.grade({'check': 'eq', 'values': [0], 'expect': 5}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'eq', 'values': [0], 'expect': 6}, v), 'NG')
        self.assertEqual(judge.grade({'check': 'range', 'values': [1], 'min': 6, 'max': 8}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'range', 'values': [1], 'min': 8, 'max': 9}, v), 'NG')
        self.assertEqual(judge.grade({'check': 'mask', 'values': [1], 'mask': 3, 'expect': 3}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'same', 'values': [0, 1]}, v), 'NG')
        self.assertEqual(judge.grade({'check': 'eq', 'values': [2], 'expect': 9, 'na_when': [2, 0]}, v), 'NA')

    def test_good_and_bad_block(self):
        exp = load()
        rc, r = run_judge(line(exp, 'sys-math', [0, 1000000, 0x5000, 4096]))
        self.assertEqual((rc, r['gating_failing']), (0, []))
        rc, r = run_judge(line(exp, 'sys-math', [1, 1000000, 0x5000, 4096]))
        self.assertEqual((rc, r['gating_failing']), (1, ['sys-math-div']))

    def test_unconfirmed_never_fails(self):
        exp = load()
        rc, r = run_judge(line(exp, 'sys-ipc', [0xA, 9, 120, 0x7F]))
        self.assertEqual(rc, 0)
        self.assertEqual(r['failing'], ['sys-ipc-ping'])

    def test_missing_demo_is_an_input_error_without_partial(self):
        exp = load()
        with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
            f.write(line(exp, 'sys-math', [0, 1000000, 0x5000, 4096]))
        try:
            err, old = io.StringIO(), sys.stderr
            sys.stderr = err
            try:
                rc = judge.main(['--expected', EXP, f.name])
            finally:
                sys.stderr = old
        finally:
            os.unlink(f.name)
        self.assertEqual(rc, 2)

    def test_short_block_is_an_input_error(self):
        exp = load()
        words = [exp['block']['magic'], exp['demos']['sys-math'], 120, 0, 0]
        rc, _ = run_judge('RESULT ' + ' '.join('%08x' % w for w in words) + '\n')
        self.assertEqual(rc, 2)

    def test_informational_change_does_not_break_a_profile(self):
        exp = load()
        # sys-ipc-ping is unconfirmed: a different value is reported but not a mismatch
        rc, r = run_judge(line(exp, 'sys-ipc', [0xA, 9, 120, 0x7F]), '--profile', 'melonds-1.1-software')
        self.assertEqual(rc, 0)

    def test_gating_change_breaks_a_profile(self):
        exp = load()
        rc, _ = run_judge(line(exp, 'sys-math', [1, 1000000, 0x5000, 4096]), '--profile', 'melonds-1.1-software')
        self.assertEqual(rc, 1)

    def test_not_value_and_new_checks(self):
        v = [0xFFFF, 0xFFFF, 0xFFFF, 0, 0x10]
        self.assertEqual(judge.grade({'check': 'same', 'values': [0, 1, 2], 'not_value': 0xFFFF}, v), 'NG')
        self.assertEqual(judge.grade({'check': 'same', 'values': [0, 1, 2]}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'ne', 'values': [3, 4]}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'nonzero', 'values': [4], 'mask': 0x10}, v), 'OK')
        self.assertEqual(judge.grade({'check': 'nonzero', 'values': [3], 'mask': 0x10}, v), 'NG')

    def test_malformed_item_is_an_input_error(self):
        with self.assertRaises(judge.Fatal):
            judge.grade({'id': 'x', 'check': 'eq', 'values': [0]}, [1])    # no "expect"

    def test_profiles_name_known_items(self):
        exp = load()
        ids = {i['id'] for i in exp['items']}
        for name, p in exp['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


if __name__ == '__main__':
    unittest.main()
