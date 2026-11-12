# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Run: python3 -m unittest discover -s tools/tests   (from the probes folder)"""

import copy
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
    out, err, old, olderr = io.StringIO(), io.StringIO(), sys.stdout, sys.stderr
    sys.stdout, sys.stderr = out, err
    try:
        rc = judge.main(['--expected', EXP, '--json', '--partial'] + list(extra) + [f.name])
    finally:
        sys.stdout, sys.stderr = old, olderr
        os.unlink(f.name)
    return rc, (json.loads(out.getvalue()) if out.getvalue() else None)


class Rules(unittest.TestCase):
    def test_shipped_expected_json_is_valid(self):
        exp = load()
        judge.check_exp(exp)
        self.assertEqual(exp['target_level'], 4)
        self.assertEqual(exp['area'], 'research probes')
        for it in exp['items']:
            self.assertEqual((it['level'], it['role'], it['source']), (4, 'target', 'unconfirmed'), it['id'])
        self.assertEqual(len(exp['known']), 7)
        for name, p in exp['known'].items():
            self.assertEqual(set(p['observed']), {i['id'] for i in exp['items']}, name)

    def test_no_relocation_leftovers_and_master_profiles(self):
        exp = load()
        for it in exp['items']:
            self.assertNotIn('to', it, it['id'])    # role target: nothing moves out of the probes
        for name in ('melonds-4ce2188-software', 'melonds-4ce2188-gl-classic', 'melonds-4ce2188-gl-compute'):
            self.assertIn(name, exp['known'])
        # melonDS#2618 is recorded on master GL/Compute and not on software
        self.assertEqual(exp['known']['melonds-4ce2188-software']['observed']['acc-split-hdma-holes'], [0])
        self.assertGreater(exp['known']['melonds-4ce2188-gl-compute']['observed']['acc-split-hdma-holes'][0], 0)
        ids = {i['id'] for i in exp['items']}
        self.assertTrue({'rear-plane', 'rear-plane-full'} <= ids)

    def test_role_and_level_rules_are_input_errors(self):
        for field, value in (('role', 'relocate'), ('role', 'prerequisite'), ('level', 3), ('level', True),
                             ('level', 0), ('source', 'GBATEK'), ('demo', 'nope')):
            exp = load()
            exp['items'][0][field] = value
            with self.assertRaises(judge.Fatal, msg='%s=%r' % (field, value)):
                judge.check_exp(exp)
        exp = load()
        exp['target_level'] = 3
        with self.assertRaises(judge.Fatal):
            judge.check_exp(exp)
        exp = load()
        exp['items'].append(copy.deepcopy(exp['items'][0]))
        with self.assertRaises(judge.Fatal):
            judge.check_exp(exp)


class Grading(unittest.TestCase):
    def test_verdicts(self):
        v = [5, 7, 0, 0]
        self.assertEqual(judge.verdict({'check': 'eq', 'values': [0], 'expect': 5}, v), 'SAME')
        self.assertEqual(judge.verdict({'check': 'eq', 'values': [0], 'expect': 6}, v), 'DIFFERS')
        self.assertEqual(judge.verdict({'check': 'range', 'values': [0], 'min': 1, 'max': 5}, v), 'SAME')
        self.assertEqual(judge.verdict({'check': 'mask', 'values': [1], 'mask': 3, 'expect': 3}, v), 'SAME')
        self.assertEqual(judge.verdict({'check': 'nonzero', 'values': [2], 'mask': 255}, v), 'DIFFERS')
        self.assertEqual(judge.verdict({'check': 'ne', 'values': [0, 1]}, v), 'SAME')
        self.assertEqual(judge.verdict({'check': 'same', 'values': [2, 3]}, v), 'SAME')
        self.assertEqual(judge.verdict({'check': 'eq', 'values': [2], 'expect': 1, 'na_when': [3, 0]}, v), 'NA')
        self.assertEqual(judge.verdict({'check': 'ne', 'values': [0, 1], 'not_value': 5}, v), 'DIFFERS')
        with self.assertRaises(judge.Fatal):
            judge.verdict({'check': 'bogus', 'values': [0]}, v)
        with self.assertRaises(judge.Fatal):
            judge.verdict({'check': 'eq', 'values': [9], 'expect': 1}, v)

    def test_observe_applies_the_mask(self):
        self.assertEqual(judge.observe({'values': [0, 1], 'mask': 0xFF0000}, [0x120005, 0x340000]),
                         [0x120000, 0x340000])
        self.assertEqual(judge.observe({'values': [1]}, [1, 2]), [2])


class Run(unittest.TestCase):
    def log_for(self, profile):
        """A log whose observed values equal the profile's record."""
        exp = load()
        blocks = {}
        for it in exp['items']:
            vals = blocks.setdefault(it['demo'], [0] * 13)
            for idx, o in zip(it['values'], profile['observed'][it['id']]):
                vals[idx] = o
        return '\n'.join(line(exp, d, v) for d, v in sorted(blocks.items()))

    def test_profile_match_and_change(self):
        exp = load()
        name = sorted(exp['known'])[0]
        text = self.log_for(exp['known'][name])
        rc, out = run_judge(text, '--profile', name)
        self.assertEqual((rc, out['changed']), (0, []))
        # one value moves: exit 1 and the item is named
        item = exp['items'][0]
        lines = text.split('\n')
        for i, row in enumerate(lines):
            if row.startswith(item['demo'] + ' '):
                words = row.split()
                k = 5 + item['values'][0]     # name RESULT magic id frame values...
                words[k] = '%08x' % (int(words[k], 16) ^ 1)
                lines[i] = ' '.join(words)
        rc, out = run_judge('\n'.join(lines), '--profile', name)
        self.assertEqual(rc, 1)
        self.assertIn(item['id'], out['changed'])

    def test_without_profile_is_informational(self):
        exp = load()
        name = sorted(exp['known'])[0]
        rc, out = run_judge(self.log_for(exp['known'][name]))
        self.assertEqual(rc, 0)
        self.assertEqual(set(out['verdicts'].values()) - {'SAME', 'DIFFERS', 'NA'}, set())

    def test_every_value_wrong_still_exits_0_without_profile(self):
        exp = load()
        rc, out = run_judge('\n'.join(line(exp, d, [0xABCDEF] * 13) for d in exp['demos']))
        self.assertEqual(rc, 0)

    def test_missing_probe_needs_partial(self):
        exp = load()
        with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
            f.write(line(exp, 'l4-3d-fifo', [0] * 13))
        old = sys.stderr
        sys.stderr = io.StringIO()
        try:
            rc = judge.main(['--expected', EXP, f.name])
        finally:
            sys.stderr = old
            os.unlink(f.name)
        self.assertEqual(rc, 2)

    def test_bad_input_is_exit_2(self):
        rc, _ = run_judge('no result here\n')
        self.assertEqual(rc, 2)
        rc, _ = run_judge('x RESULT 52455643 00000000 00000078\n')
        self.assertEqual(rc, 2)
        exp = load()
        rc, _ = run_judge(line(exp, 'l4-3d-fifo', [0] * 13), '--profile', 'no-such-profile')
        self.assertEqual(rc, 2)

    def test_other_games_lines_are_ignored(self):
        exp = load()
        rc, _ = run_judge('hushwell-sys-ipc ' + line(exp, 'l4-3d-fifo', [0] * 13))
        self.assertEqual(rc, 2)    # nothing left to judge


if __name__ == '__main__':
    unittest.main()
