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
                rc = judge.main(['--expected', EXP, '--json', '--partial'] + list(extra) + [f.name])
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
        rc, _ = self.run_judge(block(flags=0x1df), '--profile', 'melonds-1.1-gl-classic')   # newly failing
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
                rc = judge.main(['--expected', f.name, '--json', '--partial', '--profile', 'melonds-1.1-software', lf.name])
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
                rc = judge.main(['--expected', EXP, '--json', '--partial'] + list(extra) + [f.name])
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

    # --- review fixes: exit codes, full runs, gating-only profiles, prerequisites ------------------
    def cli(self, log, *extra, exp=None, json_out=False):
        """Run judge.main on a log text; exp is an expected.json dict (default: the real one)."""
        import io
        import tempfile
        paths = []
        try:
            exp_path = EXP
            if exp is not None:
                with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as ef:
                    json.dump(exp, ef)
                exp_path = ef.name
                paths.append(ef.name)
            with tempfile.NamedTemporaryFile('w', suffix='.log', delete=False) as lf:
                lf.write(log)
            paths.append(lf.name)
            out, err = io.StringIO(), io.StringIO()
            o, e, sys.stdout, sys.stderr = sys.stdout, sys.stderr, out, err
            try:
                rc = judge.main(['--expected', exp_path] + (['--json'] if json_out else []) + list(extra) + [lf.name])
            finally:
                sys.stdout, sys.stderr = o, e
        finally:
            for pth in paths:
                os.unlink(pth)
        return rc, out.getvalue(), err.getvalue()

    def real_exp(self):
        with open(EXP) as f:
            return json.load(f)

    CLEAN_DEMOS = {'l3-acc-dual': dict(v0=1, v3=0x1f), 'l3-sys-dma-hblank': dict(v4=22, v6=1)}

    def full_log(self, skip=(), **over):
        log = '' if 'rehearsal' in skip else 'elevane-rehearsal ' + text(block())
        for name in self.demo_ids():
            if name in skip:
                continue
            vals = dict(self.CLEAN_DEMOS.get(name, {}))
            vals.update(over.get(name, {}))
            log += self.demo_text(name, **vals)
        return log

    def test_all_zero_block_is_exit_2(self):
        for extra in ((), ('--partial',)):
            rc, _, err = self.cli('RESULT ' + ' '.join(['0'] * 64) + '\n', *extra)
            self.assertEqual(rc, 2, err)
            # a demo block that is all zero (the demo never finished), named like mds.sh demos prints it
            rc, _, err = self.cli('elevane-l3-acc-dual RESULT ' + ' '.join(['0'] * 16) + '\n', *extra)
            self.assertEqual(rc, 2, err)
        # one good block does not hide a zero block next to it
        rc, _, _ = self.cli(self.demo_text('l3-sys-dma-hblank', v4=22, v6=1)
                            + 'elevane-l3-acc-dual RESULT ' + ' '.join(['0'] * 16) + '\n', '--partial')
        self.assertEqual(rc, 2)

    def test_zero_block_of_a_foreign_rom_is_only_a_warning(self):
        # emu.sh/mds.sh demos also run the save variants, which finish no block
        rc, _, err = self.cli('elevane-save-flash RESULT ' + ' '.join(['0'] * 64) + '\n' + self.demo_text('l3-sys-dma-hblank', v4=22, v6=1),
                              '--partial')
        self.assertEqual(rc, 0)
        self.assertIn('warning', err)

    def test_full_run_needs_the_rehearsal_and_every_demo(self):
        rc, _, err = self.cli(self.full_log(), json_out=True)
        self.assertEqual(rc, 0, err)
        rc, _, err = self.cli(self.full_log(skip=('rehearsal',)))
        self.assertEqual(rc, 2)
        self.assertIn('rehearsal', err)
        rc, _, err = self.cli(self.full_log(skip=('l3-acc-split-hdma',)))
        self.assertEqual(rc, 2)
        self.assertIn('l3-acc-split-hdma', err)
        rc, _, _ = self.cli(self.full_log(skip=('l3-acc-split-hdma',)), '--partial')     # --partial grades what is there
        self.assertEqual(rc, 0)
        rc, _, _ = self.cli(self.full_log(skip=('l3-acc-split-hdma',)), '--demo', 'elevane-l3-acc-dual')
        self.assertEqual(rc, 0)

    def test_full_run_without_a_graded_target_is_exit_2(self):
        exp = self.real_exp()
        # force n/a: bit 30 of values[0] is never set
        for it in exp['items']:
            if it['role'] == 'target':
                it['requires'] = {'value': 0, 'bit': 30}
        rc, _, err = self.cli(self.full_log(), '--allow-na', exp=exp)
        self.assertEqual(rc, 2)
        self.assertIn('no target item', err)
        rc, _, _ = self.cli(self.full_log(), '--allow-na', '--partial', exp=exp)
        self.assertEqual(rc, 0)

    def test_hole_items_are_gating_level_3_targets(self):
        exp = self.real_exp()
        by = {i['id']: i for i in exp['items']}
        for k in ('hdma-holes-zero', 'acc-split-hdma-holes'):
            self.assertEqual((by[k]['role'], by[k]['level'], by[k]['source']), ('target', 3, 'GBATEK'), k)
        self.assertIn('2618', by['hdma-holes-zero']['title'])
        self.assertIn('2618', by['acc-split-hdma-holes']['title'])
        rc, out, _ = self.cli(self.full_log(**{'l3-acc-split-hdma': dict(v3=0x808)}), json_out=True)
        self.assertEqual(rc, 1)
        self.assertEqual(json.loads(out)['gating_failing'], ['acc-split-hdma-holes'])
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block(flags=0x1df)), '--partial', json_out=True)
        self.assertEqual((rc, json.loads(out)['gating_failing']), (1, ['hdma-holes-zero']))

    def test_profile_compares_gating_items_only(self):
        # an informational item that changed is reported, the profile still matches
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block(v4=7)), '--partial', '--profile', 'melonds-1.1-software')
        self.assertEqual(rc, 0)
        self.assertIn('INFO CHANGED dual-cross-zero', out)
        self.assertIn('MATCH', out)
        # a gating item that changed is a mismatch
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block(flags=0x1df)), '--partial', '--profile', 'melonds-1.1-software')
        self.assertEqual(rc, 1)
        self.assertIn('NEWLY FAILING hdma-holes-zero', out)
        # and a gating item the profile lists as failing that now passes
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block()), '--partial', '--profile', 'melonds-4ce2188-gl-classic')
        self.assertEqual(rc, 1)
        self.assertIn('NEWLY PASSING hdma-holes-zero', out)
        # the n/a set is compared for gating items only: a profile that lists an informational item as n/a still matches
        exp = self.real_exp()
        exp['known']['melonds-1.1-software']['na'] = ['dual-front']
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block()), '--partial', '--profile', 'melonds-1.1-software', exp=exp)
        self.assertEqual(rc, 0, out)

    def test_prerequisite_semantics(self):
        # gating prerequisite fails: PREREQ NG first, exit 1
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block(flags=0x1fe)), '--partial')
        self.assertEqual(rc, 1)
        self.assertTrue(out.splitlines()[1].startswith('PREREQ NG puzzles-delivered'), out)
        # informational prerequisite fails: PREREQ INFO, exit code unchanged
        rc, out, _ = self.cli('elevane-rehearsal ' + text(block(v4=7)), '--partial')
        self.assertEqual(rc, 0)
        self.assertIn('PREREQ INFO dual-cross-zero', out)
        self.assertNotIn('PREREQ NG', out)

    def test_roles_count_gating_and_informational_apart(self):
        rows = [('target', True, False), ('target', True, False), ('prerequisite', True, False),
                ('prerequisite', False, False), ('prerequisite', True, True), ('prerequisite', False, True),
                ('relocate', False, True)]
        self.assertEqual(judge.role_summary(rows),
                         'target 2/2 OK \u00b7 prerequisite 1/2 OK +2 informational \u00b7 relocate 1 informational')
        self.assertEqual(judge.role_summary([('prerequisite', True, True)]), 'prerequisite 1 informational')
        rc, out, _ = self.cli(self.full_log())
        self.assertEqual(rc, 0)
        self.assertIn('ROLES target 3/3 OK \u00b7 prerequisite 5/5 OK +12 informational', out)

    def test_malformed_expected_is_exit_2_not_a_traceback(self):
        log = 'elevane-rehearsal ' + text(block())
        exp = self.real_exp()
        del exp['items']
        self.assertEqual(self.cli(log, '--partial', exp=exp)[0], 2)
        for key in ('source', 'check', 'title', 'values', 'id', 'level', 'role'):
            exp = self.real_exp()
            del exp['items'][3][key]
            rc, _, err = self.cli(log, '--partial', exp=exp)
            self.assertEqual(rc, 2, key)
            self.assertNotIn('Traceback', err)
        exp = self.real_exp()
        del exp['items'][3]['expect' if 'expect' in exp['items'][3] else 'bit']
        self.assertEqual(self.cli(log, '--partial', exp=exp)[0], 2)
        for key in ('boot', 'known'):
            exp = self.real_exp()
            del exp[key]
            self.assertEqual(self.cli(log, '--partial', exp=exp)[0], 2, key)
        exp = self.real_exp()
        exp['items'][0]['demo'] = 'l3-nope'
        self.assertEqual(self.cli(log, '--partial', exp=exp)[0], 2)


if __name__ == '__main__':
    unittest.main()
