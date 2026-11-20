#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Buoyane's expected.json: the checks, n/a, profiles, the boots.

    python3 games/buoyane/tools/tests/test_judge.py
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

    def test_a_missing_dot_bit_fails_the_item(self):
        v = good_values('rehearsal')
        v[1] &= ~0x20   # dot-only-zero-size: the far 1x0 polygon is not drawn
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      dot-only-zero-size', out)

    def test_a_band_outside_the_window_fails_the_item(self):
        v = good_values('rehearsal')
        v[10] = 80   # zeq-slope-band-left
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      zeq-slope-band-left', out)

    def test_an_unconfirmed_item_never_fails_the_run(self):
        v = good_values('rehearsal')
        v[3] &= ~1   # fog-alpha-glitch is informational
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)
        self.assertIn('INFO-NG fog-alpha-glitch', out)

    def test_an_unfinished_capture_is_not_graded_ok(self):
        v = good_values('rehearsal')
        v[0] &= ~1       # the dot scene did not finish its captures: its bits mean nothing
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      dot-mixed-w-first-colour', out)

    def test_a_check_rom_grades_only_its_scene(self):
        boot = EXP['boots']['check-fog']
        rc, out = run_judge([], block_line(boot['item_id'], good_values('check-fog')))
        self.assertEqual(rc, 0, out)
        self.assertIn('fog-density-127-full', out)
        self.assertNotIn('edge-front-only', out)

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
    def test_profile_matches_the_recorded_failing_set(self):
        v = good_values('rehearsal')
        for it in EXP['items']:
            if it['id'] in EXP['known']['melonds-1.1-software']['ng']:
                v[it['values'][0]] &= ~it.get('mask', 0xFFFFFFFF)
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 0, out)   # the gating items fail, exactly as recorded
        self.assertIn('MATCH', out)

    def test_profile_reports_a_newly_failing_item(self):
        v = good_values('rehearsal')
        v[15] = 0   # the blend bits
        rc, out = run_judge(['--profile', 'melonds-1.1-software'], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY FAILING  highlight-clamp', out)

    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


class ExpectedJson(unittest.TestCase):
    def test_levels_and_roles(self):
        self.assertEqual(EXP['target_level'], 2)
        for it in EXP['items']:
            self.assertEqual(it['level'], 2 if it['role'] == 'target' else 1, it['id'])
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
