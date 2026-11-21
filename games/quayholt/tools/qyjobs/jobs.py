# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The 30 jobs: six piers of five. A job names a device, the cranes it may use and the chalk word the foreman
wants; the model alone decides whether a setup delivers it. Each job lists the setup its author had in mind
(`crane`, `p`); check.py proves that setup works and that the job is not solved by every setup."""

from .model import OP_ID, OLD, NEW, model

PIERS = ('LOADS', 'BLOCKS', 'TALLY', 'CUSTOMS', 'LOCKER', 'LEDGER BOARD')

# pier, title (<= 24 characters), device, cranes allowed (1 old, 2 new, 3 either), the intended crane and setup, the order text (<= 30)
JOBS = (
    (0, 'HALF BOX, ODD SPOT',     'LDRH_ODD',    3, OLD, 1, 'CHALK THE TURNED HALF'),
    (0, 'SIGNED HALF, ODD SPOT',  'LDRSH_ODD',   3, OLD, 1, 'CHALK THE SIGNED BYTE'),
    (0, 'SHORT HOOK, ODD SPOT',   'LDRH_THUMB',  3, OLD, 3, 'CHALK THE TURNED HALF'),
    (0, 'FLAT HALF, NEW CRANE',   'LDRH_ODD',    3, NEW, 1, 'CHALK THE FLAT HALF'),
    (0, 'SIGNED HALF, NEW CRANE', 'LDRSH_ODD',   3, NEW, 1, 'CHALK THE SIGNED HALF'),
    (1, 'GANG, BASE NOT FIRST',   'STM_BASE',    3, OLD, 2, 'LAST SLOT SHOWS NEW MARK'),
    (1, 'LOAD GANG, OLD CRANE',   'LDM_BASE',    3, OLD, 1, 'KEEP THE LOADED MARK'),
    (1, 'LOAD GANG, NEW CRANE',   'LDM_BASE',    3, NEW, 2, 'KEEP THE LOADED MARK'),
    (1, 'EMPTY GANG, OLD CRANE',  'STM_EMPTY',   1, OLD, 0, 'TROLLEY RUNS 16 SLOTS'),
    (1, 'EMPTY GANG, NEW CRANE',  'LDM_EMPTY',   2, NEW, 2, 'TROLLEY RUNS 16 SLOTS'),
    (2, 'HOOK READS AHEAD',       'ALU_PC',      1, OLD, 1, 'CHALK THREE SLOTS AHEAD'),
    (2, 'DOUBLE-WEIGH, SATURATE', 'QDADD',       2, NEW, 0, 'SCALE RINGS, SUM FITS'),
    (2, 'DOUBLE-WEIGH, SMALL',    'QDADD',       2, NEW, 2, 'SCALE STAYS QUIET'),
    (2, 'WEIGH AND ADD',          'SMLA',        2, NEW, 0, 'SCALE RINGS, TOTAL WRAPS'),
    (2, 'KEEP THE OLD HABIT',     'LDRPC_B15',   2, NEW, 1, 'HOOK STAYS ON OLD TRACK'),
    (3, 'CUSTOMS: STAMP READ',    'PU_ACCESS',   2, NEW, 5, 'PRIVATE LOOK ALLOWED'),
    (3, 'CUSTOMS: GUEST WRITE',   'PU_ACCESS',   2, NEW, 14, 'GUEST WRITE REFUSED'),
    (3, 'CUSTOMS: BOTH ZONES',    'PU_PRIORITY', 2, NEW, 1, 'HIGHER ZONE WINS: PASS'),
    (3, 'CUSTOMS: HORN',          'VECTOR',      2, NEW, 0, 'HORN GOES TO OFFICE'),
    (3, 'CUSTOMS: RED FLAG',      'VECTOR',      2, NEW, 2, 'FLAG GOES TO CUSTOMS'),
    (4, 'LOCKER: MIRROR',         'TCM_MIRROR',  2, NEW, 1, 'LOCKER SHOWS TWICE'),
    (4, 'LOCKER: RESIZE',         'ITCM_SIZE',   2, NEW, 1, 'SMALL LOCKER LOSES SHELF'),
    (4, 'LOCKER: BARGE VIEW',     'TCM_VIEW',    2, NEW, 1, 'BARGE SEES THE WAREHOUSE'),
    (4, 'LOCKER: HATCH',          'TCM_LOADMODE', 2, NEW, 1, 'FILL-ONLY HATCH'),
    (4, 'LOCKER: HATCH SHUT',     'TCM_LOADMODE', 2, NEW, 0, 'PLAIN LOCKER'),
    (5, 'BOARD: KEEP MY WRITE',   'CACHE_SEQ',   2, NEW, 0, 'BOARD HOLDS MY WORD'),
    (5, 'BOARD: WAREHOUSE WINS',  'CACHE_SEQ',   2, NEW, 2, 'WAREHOUSE WORD WINS'),
    (5, 'BOARD: NOTHING WRITTEN', 'CACHE_SEQ',   2, NEW, 4, 'BOARD HOLDS THE OLD WORD'),
    (5, 'NEW ORDERS, OLD BOARD',  'ICACHE_SEQ',  2, NEW, 1, 'OLD ORDERS KEEP RUNNING'),
    (5, 'NEW ORDERS, WIPED',      'ICACHE_SEQ',  2, NEW, 0, 'NEW ORDERS RUN'),
)


def job_list():
    """[(pier, title, op id, crane mask, solution crane, solution p, order text, goal)] with the goal worked out
    from the model."""
    out = []
    for pier, title, op, mask, crane, p, text in JOBS:
        goal = model(OP_ID[op], crane, p)
        out.append((pier, title, OP_ID[op], mask, crane, p, text, goal))
    return out
