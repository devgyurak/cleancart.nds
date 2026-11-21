#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""Tests of the job model: constants worked out by hand from GBATEK, and the solvability proof.

    python3 games/quayholt/tools/tests/test_jobs.py
"""

import os
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..'))
from qyjobs import check  # noqa: E402
from qyjobs.model import NEW, OLD, OP_ID, OPS, model, setup_label  # noqa: E402


def m(name, crane, p):
    return model(OP_ID[name], crane, p)


class Model(unittest.TestCase):
    def test_odd_halfword_loads(self):
        # bytes 11 82 33 44: the halfword at 0 is 8211h; the ARM7 rotates it right by 8 at an odd address
        self.assertEqual(m('LDRH_ODD', OLD, 1), 0x11000082)
        self.assertEqual(m('LDRH_ODD', OLD, 0), 0x8211)
        self.assertEqual(m('LDRH_ODD', NEW, 1), 0x8211)
        self.assertEqual(m('LDRSH_ODD', OLD, 1), 0xFFFFFF82)       # acts like LDRSB at the odd address
        self.assertEqual(m('LDRSH_ODD', NEW, 1), 0xFFFF8211)
        self.assertEqual(m('LDRH_ODD', OLD, 3), 0x33000044)

    def test_block_transfers(self):
        self.assertEqual([m('STM_BASE', OLD, p) for p in range(3)], [0, 12, 12])      # new base unless first
        self.assertEqual([m('STM_BASE', NEW, p) for p in range(3)], [0, 0, 0])
        self.assertEqual([m('LDM_BASE', OLD, p) for p in range(3)], [0xB0000000, 0xB0000001, 0xB0000002])
        self.assertEqual([m('LDM_BASE', NEW, p) for p in range(3)], [12, 12, 0xB0000002])  # not last: written back
        self.assertEqual(m('STM_EMPTY', OLD, 0), 0x40)
        self.assertEqual(m('STM_EMPTY', OLD, 1), 4)

    def test_alu_and_saturation(self):
        self.assertEqual([m('ALU_PC', OLD, p) for p in range(3)], [8, 12, 8])
        # QDADD 80000000h + sat(2 * 40000000h) = FFFFFFFFh with Q: upper half FFFFh, Q at bit 16
        self.assertEqual(m('QDADD', NEW, 0), 0x1FFFF)
        self.assertEqual(m('QDADD', NEW, 2), 0x0000)               # 1 + 2 = 3, no Q
        # SMLABB 7FFFh * 7FFFh + 7FFFFFFFh = BFFF0000h wrapped, Q set
        self.assertEqual(m('SMLA', NEW, 0), 0x1BFFF)

    def test_protection_unit(self):
        self.assertEqual(m('PU_ACCESS', NEW, 0 * 5 + 0), 4)        # rank 0: no access at all
        self.assertEqual(m('PU_ACCESS', NEW, 2 * 5 + 3), 0)        # rank 2: user read allowed
        self.assertEqual(m('PU_ACCESS', NEW, 2 * 5 + 4), 4)        # rank 2: user write refused
        self.assertEqual(m('PU_ACCESS', NEW, 1 * 5 + 3), 4)        # rank 1: privileged only
        self.assertEqual([m('VECTOR', NEW, p) for p in range(3)], [2, 1, 3])

    def test_cranes_that_cannot_run_a_device(self):
        self.assertIsNone(m('PU_ACCESS', OLD, 0))
        self.assertIsNone(m('STM_EMPTY', NEW, 0))
        self.assertIsNone(m('LDRH_ODD', OLD, 4))

    def test_cache_sequence(self):
        self.assertEqual([m('CACHE_SEQ', NEW, p) for p in range(6)],
                         [0xCCCC0002, 0xCCCC0002, 0xBBBB0003, 0xBBBB0003, 0xAAAA0001, 0xBBBB0003])

    def test_labels_fit_the_dial(self):
        for op, o in enumerate(OPS):
            for p in range(o[3]):
                self.assertLessEqual(len(setup_label(op, p)), 17, (o[0], p))


class Jobs(unittest.TestCase):
    def test_the_check_passes(self):
        self.assertEqual(check.check(), [])


if __name__ == '__main__':
    unittest.main()
