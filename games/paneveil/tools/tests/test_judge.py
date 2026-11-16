#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Paneveil's expected.json: the checks, n/a, profiles, the boots.

    python3 games/paneveil/tools/tests/test_judge.py
"""

import io
import json
import os
import sys
import tempfile
import unittest
from contextlib import redirect_stdout

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
import judge  # noqa: E402

EXPECTED = os.path.join(HERE, '..', '..', 'expected.json')
with open(EXPECTED) as _f:
    EXP = json.load(_f)
MAGIC = EXP['block']['magic']
REHEARSAL_ID = EXP['boots']['rehearsal']['item_id']
QA_ID = EXP['boots']['qa']['item_id']


def good_values(boot):
    """The values a correct ROM reports: every item of the boot at its expected value."""
    v = [0] * 61
    for it in EXP['items']:
        if boot not in it['boots']:
            continue
        i = it['values'][0]
        if it['check'] == 'eq':
            v[i] = it['expect']
        elif it['check'] == 'mask':
            v[i] |= it['expect']
        elif it['check'] == 'range':
            v[i] = it['min']
        else:
            raise AssertionError('test helper does not know check %s' % it['check'])
    return v


def block_line(item_id, values, frame=400):
    words = [MAGIC, item_id, frame] + list(values) + [0] * (61 - len(values))
    return 'RESULT %s' % ' '.join('%08x' % w for w in words)


def run_judge(args, log_text):
    with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
        f.write(log_text)
        path = f.name
    try:
        out = io.StringIO()
        with redirect_stdout(out):
            rc = judge.main(args + [path])
        return rc, out.getvalue()
    finally:
        os.unlink(path)


class Grading(unittest.TestCase):
    def test_a_correct_block_passes(self):
        rc, out = run_judge([], block_line(REHEARSAL_ID, good_values('rehearsal')))
        self.assertEqual(rc, 0, out)
        self.assertIn('RESULT: OK', out)

    def test_a_wrong_matrix_word_fails_the_item(self):
        v = good_values('rehearsal')
        v[12] = 3   # mtx-load-mult-clip: three CLIPMTX_RESULT words differ
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      mtx-load-mult-clip', out)

    def test_a_wrong_stack_level_fails_the_item(self):
        v = good_values('rehearsal')
        v[3] = 2   # mtx-push-level
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      mtx-push-level', out)

    def test_boot_must_agree_with_the_block(self):
        rc, out = run_judge(['--boot', 'qa'], block_line(REHEARSAL_ID, good_values('rehearsal')))
        self.assertEqual(rc, 2, out)

    def test_unfinished_block_is_not_graded(self):
        rc, out = run_judge([], 'RESULT ' + ' '.join(['00000000'] * 64))
        self.assertEqual(rc, 2, out)

    def test_unknown_item_id(self):
        rc, out = run_judge([], block_line(0x12345678, good_values('rehearsal')))
        self.assertEqual(rc, 2, out)


class Profiles(unittest.TestCase):
    def test_profile_matches_when_nothing_fails(self):
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, good_values('rehearsal')))
        self.assertEqual(rc, 0, out)
        self.assertIn('MATCH', out)

    def test_profile_reports_a_newly_failing_item(self):
        v = good_values('rehearsal')
        v[15] = 1   # gx-direct-ports
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY FAILING  gx-direct-ports', out)

    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


class ExpectedJson(unittest.TestCase):
    def test_levels_and_roles(self):
        self.assertEqual(EXP['target_level'], 1)
        for it in EXP['items']:
            self.assertEqual(it['level'], 1, it['id'])
            self.assertIn(it['role'], ('target', 'prerequisite'), it['id'])

    def test_value_indices_do_not_collide(self):
        seen = {}
        for it in EXP['items']:
            for i in it['values']:
                key = (i, it['check'])
                if it['check'] == 'mask':
                    continue
                self.assertNotIn(i, seen, '%s and %s read the same value' % (it['id'], seen.get(i)))
                seen[i] = it['id']


if __name__ == '__main__':
    unittest.main()
