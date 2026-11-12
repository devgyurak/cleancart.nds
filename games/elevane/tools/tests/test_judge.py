# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
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
        rc, r = self.run_judge(block(v4=7))     # dual-cross-zero is unconfirmed
        self.assertEqual((rc, r['failing']), (0, ['dual-cross-zero']))

    def test_counters_unavailable_is_na(self):
        rc, r = self.run_judge(block(flags=0xf7))
        self.assertEqual(r['items']['poly-passes'], 'NA')

    def test_profile_must_match_exactly(self):
        rc, _ = self.run_judge(block(), '--profile', 'melonds-1.1-gl-classic')
        self.assertEqual(rc, 0)
        rc, _ = self.run_judge(block(v4=7), '--profile', 'melonds-1.1-gl-classic')   # newly failing
        self.assertEqual(rc, 1)

    def test_every_item_has_a_level(self):
        with open(EXP) as f:
            exp = json.load(f)
        judge.check_levels(exp['items'])
        for it in exp['items']:
            self.assertIn(it['level_basis'].split(':')[0], ('gbatek', 'report', 'hw', 'none'), it['id'])

    def test_missing_or_bad_level_is_an_input_error(self):
        for bad in ({'id': 'x'}, {'id': 'x', 'level': 0}, {'id': 'x', 'level': 5}, {'id': 'x', 'level': True},
                    {'id': 'x', 'level': '2'}):
            with self.assertRaises(judge.Fatal):
                judge.check_levels([bad])

    def test_level_summary(self):
        rows = [(1, True, False), (1, False, False), (2, True, False), (4, True, True), (4, False, True), (3, True, False), (3, True, True)]
        self.assertEqual(judge.level_summary(rows),
                         'L1 1/2 OK \u00b7 L2 1/1 OK \u00b7 L3 1/1 OK +1 informational \u00b7 L4 informational 2')

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

    def test_roles_are_checked(self):
        with open(EXP) as f:
            exp = json.load(f)
        judge.check_roles(exp)
        for mutate in (lambda e: e.pop('target_level'),
                       lambda e: e['items'][0].pop('role'),
                       lambda e: e['items'][0].update(role='target', level=4),
                       lambda e: e['items'][0].update(role='prerequisite', level=4),
                       lambda e: e['items'][0].update(role='relocate', to=None)):
            bad = json.loads(json.dumps(exp))
            mutate(bad)
            with self.assertRaises(judge.Fatal):
                judge.check_roles(bad)

    # --- level 3 demo blocks ("demos" in expected.json, 16 words each) --------------------------
    def demo_ids(self):
        with open(EXP) as f:
            return json.load(f)['demos']

    def demo_text(self, name, **vals):
        v = [0] * 13
        for k, x in vals.items():
            v[int(k[1:])] = x
        return '%s RESULT %s\n' % ('elevane-' + name, ' '.join('%08x' % w for w in
                                                               [judge.MAGIC, self.demo_ids()[name], 120] + v))

    def run_log(self, log, *extra):
        import io
        import tempfile
        with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as f:
            f.write(log)
        try:
            out, err = io.StringIO(), io.StringIO()
            o, e, sys.stdout, sys.stderr = sys.stdout, sys.stderr, out, err
            try:
                rc = judge.main(['--expected', EXP, '--json'] + list(extra) + [f.name])
            finally:
                sys.stdout, sys.stderr = o, e
        finally:
            os.unlink(f.name)
        return rc, (json.loads(out.getvalue()) if out.getvalue() else None)

    def test_demo_ids_are_fnv1a_of_the_names(self):
        for name, bid in self.demo_ids().items():
            h = 2166136261
            for c in name.encode():
                h = ((h ^ c) * 16777619) & 0xFFFFFFFF
            self.assertEqual(h, bid, name)

    def test_demo_blocks_are_graded_alone(self):
        log = self.demo_text('l3-acc-dual', v0=1, v3=0x1f) + self.demo_text('l3-sys-dma-hblank', v4=22, v6=1)
        rc, r = self.run_log(log)
        self.assertEqual(rc, 0)
        self.assertEqual(r['demos'], ['l3-acc-dual', 'l3-sys-dma-hblank'])
        self.assertEqual(r['items']['acc-dual-latency'], 'OK')
        self.assertEqual(r['items']['sys-dma-hblank-pixels'], 'OK')
        self.assertNotIn('puzzles-delivered', r['items'])          # no rehearsal block in the log

    def test_demo_gating_failure_and_informational(self):
        rc, r = self.run_log(self.demo_text('l3-sys-dma-hblank', v5=3))
        self.assertEqual((rc, r['gating_failing']), (1, ['sys-dma-hblank-pixels']))
        rc, r = self.run_log(self.demo_text('l3-acc-dual', v0=2, v3=0))   # unconfirmed: informational
        self.assertEqual((rc, r['failing']), (0, ['acc-dual-latency', 'acc-dual-latency-flag']))

    def test_demo_and_rehearsal_blocks_in_one_log(self):
        log = 'elevane-rehearsal ' + text(block()) + self.demo_text('l3-acc-dual', v0=1, v3=0x10)
        rc, r = self.run_log(log)
        self.assertEqual((rc, r['failing']), (0, []))
        self.assertIn('puzzles-delivered', r['items'])
        self.assertIn('acc-dual-latency-flag', r['items'])

    def test_unfinished_or_short_demo_block_is_exit_2(self):
        rc, _ = self.run_log(self.demo_text('l3-acc-dual').replace(' 52455643 ', ' 00000000 ', 1))
        self.assertEqual(rc, 2)
        rc, _ = self.run_log(' '.join(self.demo_text('l3-acc-dual').split()[:10]) + '\n')
        self.assertEqual(rc, 2)

    def test_every_demo_item_names_a_listed_demo(self):
        with open(EXP) as f:
            exp = json.load(f)
        for it in exp['items']:
            if 'demo' in it:
                self.assertIn(it['demo'], exp['demos'], it['id'])
                self.assertTrue(it['demo'].startswith('l3-'), it['id'])


if __name__ == '__main__':
    unittest.main()
