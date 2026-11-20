#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Unit tests of tools/judge.py with Ledgerine's expected.json: the checks, the done bits, profiles, the boots.

    python3 games/ledgerine/tools/tests/test_judge.py
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

    def test_a_cleared_flag_fails_the_item(self):
        v = good_values('rehearsal')
        v[1] &= ~1   # math-div0 needs all four of its bits
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      math-div0', out)

    def test_an_unaudited_room_is_not_graded_ok(self):
        v = good_values('rehearsal')
        v[0] &= ~(1 << 1)   # the tube never finished: its flag word means nothing
        rc, out = run_judge([], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NG      ipc-send-full', out)

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


PROFILE = sorted(EXP['known'])[0] if EXP['known'] else None


class Profiles(unittest.TestCase):
    def test_profile_reports_a_newly_failing_item(self):
        v = good_values('rehearsal')
        v[1] = 0   # the press flags all wrong
        rc, out = run_judge(['--profile', PROFILE], block_line(REHEARSAL_ID, v))
        self.assertEqual(rc, 1, out)
        self.assertIn('NEWLY FAILING  math-div0', out)

    def test_every_profile_lists_only_known_items(self):
        ids = {i['id'] for i in EXP['items']}
        for name, p in EXP['known'].items():
            self.assertLessEqual(set(p['ng']) | set(p.get('na', [])), ids, name)


class HardwareValues(unittest.TestCase):
    """Expected values taken from the DS Lite runs (docs/HW-QA.md): the console value passes, the old GBATEK value does not."""

    def grade(self, value):
        v = good_values('rehearsal')
        v[2] = (v[2] & ~0xC) | value
        return run_judge([], block_line(REHEARSAL_ID, v))[1]

    def test_empty_read_after_remote_clear_is_not_zero(self):
        self.assertIn('OK      ipc-empty-cleared', self.grade(0x4))    # the console: bit 3 clear, acknowledge set
        self.assertIn('NG      ipc-empty-cleared', self.grade(0xC))    # returns 0 (GBATEK, melonDS, DeSmuME)
        self.assertIn('NG      ipc-empty-cleared', self.grade(0x0))    # acknowledge broken as well

    def test_empty_read_after_remote_clear_returns_a15(self):
        for word, verdict in ((0x7A00000F, 'OK'), (0, 'NG'), (0x7B000000, 'NG'), (0x7C000000, 'NG')):
            v = good_values('rehearsal')
            v[44] = word
            out = run_judge([], block_line(REHEARSAL_ID, v))[1]
            self.assertIn('%-7s ipc-empty-cleared-word' % verdict, out, hex(word))

    def test_every_profile_knows_the_console_difference(self):
        for name, p in EXP['known'].items():
            self.assertIn('ipc-empty-cleared', p['ng'], name)
            self.assertIn('ipc-empty-cleared-word', p['ng'], name)

    def test_sources_are_hardware(self):
        by = {i['id']: i for i in EXP['items']}
        self.assertEqual(by['ipc-empty-cleared']['source'], 'hw:dslite-ntr')
        self.assertEqual(by['ipc-empty-cleared-word']['source'], 'hw:dslite-ntr')
        for i in ('ipc-empty-cleared', 'ipc-empty-cleared-word', 'ipc-empty-last', 'dma-itcm-runs', 'bios-arm7-guard'):
            self.assertIn('hw:games/ledgerine/docs/HW-QA.md', by[i]['level_basis'], i)


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
