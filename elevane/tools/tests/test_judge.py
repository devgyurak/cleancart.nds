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
    v[5], v[6] = 31, 5
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
        rc, r = self.run_judge(block(flags=0x1fe))   # puzzles not delivered
        self.assertEqual(rc, 1)
        self.assertIn('puzzles-delivered', r['gating_failing'])

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

    def values_args(self, words=None, crc=None, drop=0):
        v = (words or block()[3:21])[:18]
        crc_v = judge.crc16(b''.join(x.to_bytes(4, 'little') for x in v)) if crc is None else crc
        toks = ['%x' % x for x in v][:18 - drop] + ['%04x' % crc_v]
        return toks

    def run_values(self, toks, *extra):
        import io
        out, err = io.StringIO(), io.StringIO()
        o, e, sys.stdout, sys.stderr = sys.stdout, sys.stderr, out, err
        try:
            rc = judge.main(['--expected', EXP, '--json'] + list(extra) + ['--values'] + toks)
        finally:
            sys.stdout, sys.stderr = o, e
        return rc, out.getvalue(), err.getvalue()

    def test_values_from_a_console_screen(self):
        rc, out, _ = self.run_values(self.values_args())
        self.assertEqual(rc, 0)
        self.assertEqual(json.loads(out)['gating_failing'], [])

    def test_values_must_be_complete_hex_with_a_matching_crc(self):
        rc, _, err = self.run_values(self.values_args(drop=1))      # a value missing
        self.assertEqual(rc, 2)
        toks = self.values_args()
        toks[3] = '1g4'                                              # not hex
        self.assertEqual(self.run_values(toks)[0], 2)
        toks = self.values_args()
        toks[2] = '15c7'                                             # misread digit: CRC mismatch
        rc, _, err = self.run_values(toks)
        self.assertEqual(rc, 2)
        self.assertIn('CRC', err)
        self.assertEqual(self.run_values(['1ff'])[0], 2)             # the old "one value passes" case

    def test_values_from_the_real_rom_screen(self):
        # Read off the rehearsal ROM's own screen (melonDS 1.1 software, CRC computed in C by the
        # ROM), not derived by judge.crc16, so the two implementations are tied together.
        toks = ('1ff 1 134b 12c2 0 1f 5 46204a7 11881298 0 0 0 c00 c00 f56f1baa 1000 46204a7 1a1a'
                ' 1fd7').split()
        rc, out, _ = self.run_values(toks)
        self.assertEqual(rc, 0)
        toks[-1] = '1fd8'
        self.assertEqual(self.run_values(toks)[0], 2)

    def test_short_result_line_is_exit_2(self):
        rc, _ = self.run_judge(block()[:30])
        self.assertEqual(rc, 2)

    def test_profile_also_compares_the_na_set(self):
        import copy
        with open(EXP) as f:
            exp = json.load(f)
        exp['known']['melonds-1.1-software']['na'] = ['poly-passes']
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
            json.dump(exp, f)
        try:
            import io
            out = io.StringIO()
            o, sys.stdout = sys.stdout, out
            try:
                with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as lf:
                    lf.write(text(block()))
                rc = judge.main(['--expected', f.name, '--json', '--profile', 'melonds-1.1-software', lf.name])
            finally:
                sys.stdout = o
                os.unlink(lf.name)
        finally:
            os.unlink(f.name)
        self.assertEqual(rc, 1)   # the item is graded now, the profile said n/a

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
