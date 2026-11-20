#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Pendulane's expected.json: the checks, n/a, profiles, the boots.

    python3 games/pendulane/tools/tests/test_judge.py
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
            v[i] = it['min'] & 0xFFFFFFFF   # signed items hold s32 words
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

    def test_a_difference_outside_the_window_fails_the_item(self):
        v = good_values('rehearsal')
        v[3] = 14   # DIV 32/32 minus SQRT: 14 half cycles, the book says 10 +- 2
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      math-div32-vs-sqrt', out)

    def test_a_negative_difference_is_signed(self):
        v = good_values('rehearsal')
        v[5] = (-1) & 0xFFFFFFFF   # math-div6464-eq-div6432: -1 is inside -2..2
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)
        v[5] = (-3) & 0xFFFFFFFF
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      math-div6464-eq-div6432', out)

    def test_an_unmeasured_edge_is_not_graded_ok(self):
        v = good_values('rehearsal')
        v[3] = v[4] = v[5] = 0xFFFF   # the ROM's "not measured" marker
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      math-div64-vs-div32', out)

    def test_a_prerequisite_failure_is_reported(self):
        v = good_values('rehearsal')
        v[23] = 0
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('PREREQ NG', out)

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
    def test_profile_matches_when_the_known_items_fail(self):
        v = good_values('rehearsal')
        by_id = {i['id']: i for i in EXP['items']}
        for name in EXP['known']['melonds-1.1-software']['ng']:
            v[by_id[name]['values'][0]] = 0   # the value melonDS reads for the known NG item is outside its window
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)
        self.assertIn('MATCH', out)

    def test_profile_reports_a_newly_passing_item(self):
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, good_values('rehearsal')))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY PASSING  mem7-code-main-seq', out)
        self.assertNotIn('mem7-main-nonseq', out.split('LEVELS')[1])   # melonDS reads 384: inside the window, no longer a known NG

    def test_profile_reports_a_newly_failing_item(self):
        v = good_values('rehearsal')
        v[3] = 0   # math-div32-vs-sqrt
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY FAILING  math-div32-vs-sqrt', out)

    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


class HardwareValues(unittest.TestCase):
    """Expected values taken from the DS Lite runs (docs/HW-QA.md): the console values pass, the old GBATEK values do not."""

    def grade(self, idx, value, extra=()):
        v = good_values('rehearsal')
        v[idx] = value
        return run_judge(list(extra), block_line(REHEARSAL_ID, v))

    def test_main_ram_nonseq_is_384(self):
        for value in (383, 384):   # console, melonDS
            self.assertIn('OK      mem7-main-nonseq', self.grade(16, value)[1])
        for value in (576, 64):    # the old GBATEK value, DeSmuME
            self.assertIn('NG      mem7-main-nonseq', self.grade(16, value)[1])

    def test_code_from_main_ram_is_142(self):
        self.assertIn('OK      mem7-code-main-seq', self.grade(17, 142)[1])
        for value in (128, 150):   # the old GBATEK value (melonDS, DeSmuME)
            self.assertIn('NG      mem7-code-main-seq', self.grade(17, value)[1])

    def test_profiles_follow_the_console_values(self):
        for name, p in EXP['known'].items():
            self.assertIn('mem7-code-main-seq', p['ng'], name)
            self.assertEqual('mem7-main-nonseq' in p['ng'], name.startswith('desmume'), name)

    def test_sources_are_hardware(self):
        by = {i['id']: i for i in EXP['items']}
        for i in ('mem7-main-nonseq', 'mem7-code-main-seq'):
            self.assertEqual(by[i]['source'], 'hw:dslite-ntr')
            self.assertIn('hw:games/pendulane/docs/HW-QA.md', by[i]['level_basis'])
        self.assertEqual(by['snd-rate-slope']['source'], 'unconfirmed')


class ExpectedJson(unittest.TestCase):
    def test_levels_and_roles(self):
        self.assertEqual(EXP['target_level'], 3)
        for it in EXP['items']:
            if it['role'] == 'target':
                self.assertEqual(it['level'], 3, it['id'])
            else:
                self.assertLessEqual(it['level'], 3, it['id'])
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
