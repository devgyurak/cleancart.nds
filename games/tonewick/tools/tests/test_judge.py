#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Tonewick's expected.json: the checks, the done bits, profiles, the boots.

    python3 games/tonewick/tools/tests/test_judge.py
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
        elif it['check'] == 'same':
            for j in it['values']:
                v[j] = 8
        else:
            raise AssertionError('test helper does not know check %s' % it['check'])
        if 'done' in it:
            v[it['done']['value']] |= it['done']['mask']
    v[19] = 8   # RTC samples at an hour of 12 or later: the AM/PM rule was exercised
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

    def test_a_wrong_measurement_fails_the_item(self):
        v = good_values('rehearsal')
        v[2] = 40000   # duty 7 must stay LOW: a mean of +7232 is not the floor
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      snd-duty7-low', out)

    def test_an_unaudited_district_is_not_graded_ok(self):
        v = good_values('rehearsal')
        v[0] &= ~(1 << 1)   # the splice never finished: its numbers mean nothing
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      snd-short-busy', out)

    def test_a_capture_that_never_ran_fails(self):
        v = good_values('rehearsal')
        v[10] = 0xFFFF   # short-tape peak: 0xFFFF is "never measured", not "silent"
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      snd-short-silent', out)

    def test_save_items_are_n_a_without_a_chip(self):
        v = good_values('qa')
        v[32] = v[33] = v[34] = 0xFFFF
        rc, out = run_judge(['--allow-na'], block_line(QA_ID, v, frame=2000))
        self.assertEqual(rc, 0, out)
        self.assertIn('N/A     save-wel-reset', out)

    def test_rtc_item_needs_an_afternoon_and_an_answer(self):
        v = good_values('rehearsal')
        v[19] = 0   # before noon: the flag is 0 either way, nothing was tested
        rc, out = run_judge(['--allow-na'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)
        self.assertIn('N/A     rtc-ampm-24h', out)
        v = good_values('rehearsal')
        v[25] = v[26] = 0xFFFF   # the ARM7 never answered: equal, but not a result
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      rtc-ampm-24h', out)

    def test_pm_rule_n_a_for_an_unknown_model(self):
        v = good_values('rehearsal')
        v[31] = 2
        rc, out = run_judge(['--allow-na'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)
        self.assertIn('N/A     pm-reg-mirror', out)

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


PROFILE = 'melonds-1.1-software'


class Profiles(unittest.TestCase):
    def test_profile_reports_a_newly_failing_item(self):
        v = good_values('rehearsal')
        v[18] = 0   # capture length 0 wrote nothing
        rc, out = run_judge(['--profile', PROFILE], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY FAILING  cap-len-zero', out)

    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


class ExpectedJson(unittest.TestCase):
    def test_levels_and_roles(self):
        self.assertEqual(EXP['target_level'], 2)
        for it in EXP['items']:
            if it['role'] == 'target':
                self.assertEqual(it['level'], 2, it['id'])
                self.assertIn('done', it, it['id'])
            else:
                self.assertEqual(it['role'], 'prerequisite', it['id'])

    def test_masks_stay_inside_the_word(self):
        for it in EXP['items']:
            if it['check'] == 'mask':
                self.assertEqual(it['expect'] & ~it['mask'], 0, it['id'])


if __name__ == '__main__':
    unittest.main()
