# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s tools/tests   (from the hushwell folder)"""

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
        rc, r = run_judge(line(exp, 'io-cart', [0, 1, 1]))
        self.assertEqual((rc, r['gating_failing']), (0, []))
        rc, r = run_judge(line(exp, 'io-cart', [0, 0, 1]))
        self.assertEqual((rc, r['gating_failing']), (1, ['io-cart-ok']))

    def test_relocate_items_are_informational(self):
        # no item is waiting to move any more; the rule still holds for one that would be
        self.assertFalse(judge.is_gating({'source': 'GBATEK', 'role': 'relocate'}))
        self.assertTrue(judge.is_gating({'source': 'GBATEK', 'role': 'target'}))
        self.assertFalse(judge.is_gating({'source': 'unconfirmed', 'role': 'prerequisite'}))
        exp = load()
        self.assertEqual([i['id'] for i in exp['items'] if i['role'] == 'relocate'], [])

    def test_roles_are_checked(self):
        exp = load()
        judge.check_roles(exp)
        for mutate in (lambda e: e.pop('target_level'),
                       lambda e: e['items'][0].pop('role'),
                       lambda e: e['items'][0].update(role='target', level=2),
                       lambda e: e['items'][0].update(role='prerequisite', level=3),
                       lambda e: e['items'][0].update(role='target', level=3),
                       lambda e: e['items'][0].update(role='relocate', to=None)):
            bad = json.loads(json.dumps(exp))
            mutate(bad)
            with self.assertRaises(judge.Fatal):
                judge.check_roles(bad)

    def test_unconfirmed_never_fails(self):
        exp = load()
        rc, r = run_judge(line(exp, 'sys-ipc', [0xA, 9, 120, 0x7F]))
        self.assertEqual(rc, 0)
        self.assertEqual(r['failing'], ['sys-ipc-ping'])

    def test_missing_demo_is_an_input_error_without_partial(self):
        exp = load()
        with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
            f.write(line(exp, 'io-cart', [0, 1, 1]))
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
        words = [exp['block']['magic'], exp['demos']['io-cart'], 120, 0, 0]
        rc, _ = run_judge('RESULT ' + ' '.join('%08x' % w for w in words) + '\n')
        self.assertEqual(rc, 2)

    def test_informational_change_does_not_break_a_profile(self):
        exp = load()
        # sys-ipc-ping is unconfirmed: a different value is reported but not a mismatch
        rc, r = run_judge(line(exp, 'sys-ipc', [0xA, 9, 120, 0x7F]), '--profile', 'melonds-1.1-software')
        self.assertEqual(rc, 0)

    def test_gating_change_breaks_a_profile(self):
        exp = load()
        rc, _ = run_judge(line(exp, 'io-cart', [0, 0, 1]), '--profile', 'melonds-1.1-software')
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

    def test_other_games_lines_are_ignored(self):
        exp = load()
        text = line(exp, 'io-cart', [0, 0, 1]).replace('io-cart RESULT', '%s-io-cart RESULT' % judge.OTHER_GAMES[0])
        err, old = io.StringIO(), sys.stderr
        sys.stderr = err
        try:
            self.assertEqual(judge.parse_blocks(text), {})
        finally:
            sys.stderr = old
        self.assertIn('ignored', err.getvalue())
        self.assertEqual(len(judge.parse_blocks(line(exp, 'io-cart', [0, 1, 1]).replace('io-cart RESULT', '%s-io-cart RESULT' % judge.GAME))), 1)

    def test_env_items_are_n_a_on_hardware(self):
        exp = load()
        rc, r = run_judge(line(exp, 'io-cart', [0, 0, 1]), '--hardware')
        self.assertEqual((rc, r['gating_failing']), (0, []))
        self.assertEqual(r['items']['io-cart-ok'], 'NA')

    def test_n_a_without_a_profile_is_not_a_pass(self):
        exp = load()
        na_item = next((i for i in exp['items'] if i.get('na_when') and judge.is_gating(i)), None)
        if na_item is None:
            self.skipTest('no gating item with na_when in this game (the 3D items moved out)')
        words = [0] * 13
        words[na_item['na_when'][0]] = na_item['na_when'][1]
        rc, r = run_judge(line(exp, na_item['demo'], words))
        self.assertEqual(r['items'][na_item['id']], 'NA')
        self.assertEqual(rc, 1)
        rc, r = run_judge(line(exp, na_item['demo'], words), '--allow-na')
        self.assertEqual(r['gating_failing'] == [], rc == 0)

    def test_items_must_be_a_list_of_objects(self):
        for bad in ({}, {'a': 1}, [1], 'x'):
            with self.assertRaises(judge.Fatal):
                judge.check_levels(bad)

    def test_role_summary_counts_informational_apart(self):
        rows = [('target', True, True), ('target', False, True), ('prerequisite', False, False), ('relocate', True, False)]
        self.assertEqual(judge.role_summary(rows),
                         'target 1/2 OK \u00b7 prerequisite informational 1 \u00b7 relocate 1 informational')

    def test_informational_prerequisite_never_fails(self):
        exp = load()
        item = next(i for i in exp['items'] if i['role'] == 'prerequisite' and i['source'] == 'unconfirmed')
        self.assertFalse(judge.is_gating(item))

    def test_missing_item_keys_are_an_input_error(self):
        with self.assertRaises(judge.Fatal):
            judge.check_levels([{'id': 'x', 'level': 1}])


if __name__ == '__main__':
    unittest.main()
