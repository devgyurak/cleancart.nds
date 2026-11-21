#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Quayholt's expected.json: the checks, the done bits, profiles, the boots.

    python3 games/quayholt/tools/tests/test_judge.py
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
        if 'done' in it:
            v[it['done']['value']] |= it['done']['mask']
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

    def test_an_alternative_code_fails_the_item(self):
        v = good_values('rehearsal')
        v[1] = 2   # ldrh7-odd: the aligned halfword, no rotation
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      ldrh7-odd', out)

    def test_an_unmeasured_pier_is_not_graded_ok(self):
        v = good_values('rehearsal')
        v[0] &= ~(1 << 1)   # the blocks pier never finished: its codes mean nothing
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      stm7-base-new', out)

    def test_unmeasured_marker_is_ng(self):
        v = good_values('rehearsal')
        v[4] = 0xFFFF
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)

    def test_a_pier_demo_grades_only_its_pier(self):
        v = [0] * 61
        v[0] = 1
        v[1] = v[2] = v[3] = 1
        v[31] = 7
        rc, out = run_judge([], block_line(EXP['boots']['l2-loads']['item_id'], v))
        self.assertEqual(rc, 0, out)
        self.assertNotIn('stm7-base-new', out)

    def test_a_good_qa_block_passes(self):
        rc, out = run_judge([], block_line(QA_ID, good_values('qa'), frame=2000))
        self.assertEqual(rc, 0, out)

    def test_a_failed_qa_verdict_fails(self):
        v = good_values('qa')
        v[48] = 2
        rc, out = run_judge([], block_line(QA_ID, v, frame=2000))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      qa-verdict', out)

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
    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)

    def test_the_four_profiles_exist(self):
        self.assertEqual(sorted(EXP['known']), sorted(['melonds-1.1-software', 'melonds-1.1-gl-classic',
                                                      'melonds-1.1-gl-compute', 'desmume-pydesmume-0.0.9']))

    def test_a_profile_passes_with_its_own_failures(self):
        for name, p in EXP['known'].items():
            v = good_values('rehearsal')
            for it in EXP['items']:
                if it['id'] in p['ng'] and 'rehearsal' in it['boots']:
                    v[it['values'][0]] = 0xFF if it['check'] == 'eq' else 0
            rc, out = run_judge(['--profile', name], block_line(REHEARSAL_ID, v))
            self.assertEqual(rc, 0, (name, out))


class Items(unittest.TestCase):
    def test_every_target_has_a_gbatek_or_report_basis(self):
        for it in EXP['items']:
            if it['role'] == 'target':
                self.assertTrue('gbatek:' in it['level_basis'] or 'report:' in it['level_basis']
                                or 'hw:' in it['level_basis'], it['id'])

    def test_unconfirmed_items_are_targets_awaiting_hardware(self):
        for it in EXP['items']:
            if it['source'] == 'unconfirmed':
                self.assertEqual(it['role'], 'target')


if __name__ == '__main__':
    unittest.main()
