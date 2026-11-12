# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 Corvane contributors
"""Run: python3 -m unittest discover -s tools/tests   (from the elevane folder)"""

import json
import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))

import judge  # noqa: E402

EXP = os.path.join(os.path.dirname(HERE), '..', 'expected.json')


def block(flags=0x1ff, **over):
    v = [0] * 61
    v[0], v[1], v[2], v[3] = flags, 1, 5574, 4138
    v[12], v[13], v[15] = 3072, 3072, 4096
    for k, x in over.items():
        v[int(k[1:])] = x
    return [judge.MAGIC, 0x48525645, 4000] + v


def text(words):
    return 'RESULT ' + ' '.join('%08x' % w for w in words) + '\n'


class Judge(unittest.TestCase):
    def run_judge(self, words, *extra):
        import io
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
            f.write(text(words))
        try:
            out = io.StringIO()
            old, sys.stdout = sys.stdout, out
            try:
                rc = judge.main(['--expected', EXP, '--json'] + list(extra) + [f.name])
            finally:
                sys.stdout = old
        finally:
            os.unlink(f.name)
        return rc, (json.loads(out.getvalue()) if out.getvalue() else None)

    def test_clean_block_passes(self):
        rc, r = self.run_judge(block())
        self.assertEqual((rc, r['failing']), (0, []))

    def test_gating_failure_fails(self):
        rc, r = self.run_judge(block(flags=0x1df))   # hdma holes
        self.assertEqual(rc, 1)
        self.assertIn('hdma-holes-zero', r['gating_failing'])

    def test_unconfirmed_is_informational(self):
        rc, r = self.run_judge(block(v15=2048))
        self.assertEqual((rc, r['failing']), (0, ['rear-plane-full']))

    def test_counters_unavailable_is_na(self):
        rc, r = self.run_judge(block(flags=0xf7))
        self.assertEqual(r['items']['poly-passes'], 'NA')

    def test_profile_must_match_exactly(self):
        rc, _ = self.run_judge(block(v15=2048), '--profile', 'melonds-1.1-gl-classic')
        self.assertEqual(rc, 0)
        rc, _ = self.run_judge(block(), '--profile', 'melonds-1.1-gl-classic')   # newly passing
        self.assertEqual(rc, 1)

    def test_values_from_a_console_screen(self):
        import io
        out = io.StringIO()
        old, sys.stdout = sys.stdout, out
        try:
            rc = judge.main(['--expected', EXP, '--json', '--values', '1ff', '1', '15c6', '102a'])
        finally:
            sys.stdout = old
        self.assertEqual(rc, 0)
        self.assertEqual(json.loads(out.getvalue())['gating_failing'], [])

    def test_bad_input_is_exit_2(self):
        rc, _ = self.run_judge([0] * 64)
        self.assertEqual(rc, 2)
        w = block()
        w[1] = 0x12345678
        rc, _ = self.run_judge(w)
        self.assertEqual(rc, 2)

    def test_every_profile_names_known_items(self):
        with open(EXP) as f:
            exp = json.load(f)
        ids = {i['id'] for i in exp['items']}
        for name, p in exp['known'].items():
            self.assertTrue(set(p['ng']) <= ids, name)


if __name__ == '__main__':
    unittest.main()
