# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
"""The devices of the six piers and what GBATEK says each one answers.

A device (op) runs on one of two cranes: CRANE_OLD is the ARM7 (ARMv4T), CRANE_NEW the ARM9 (ARMv5TE).
`model(op, crane, p)` is the "chalk word" a correct console writes when the device runs with setup `p`
(0 .. setups - 1); None when the crane cannot run the device. The hardware side (arm9/source/game/devices.c)
computes the same word by running real instructions; the game compares the two.

Test data: the strip of boxes is the bytes 11h 82h 33h 44h 55h 66h 77h 88h (words 44338211h, 88776655h).
"""

OLD, NEW = 0, 1
CRANE_NAMES = ('OLD', 'NEW')

STRIP = (0x11, 0x82, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88)
M32 = 0xFFFFFFFF
BUF_WORDS = 0xB0000000      # LDM test data: word i = B0000000h + i

# cache and TCM test words
CACHE_A, CACHE_C, CACHE_D = 0xAAAA0001, 0xCCCC0002, 0xBBBB0003
TCM_P1, TCM_P2 = 0x4D41494E, 0xD7D70001     # the main RAM word behind the DTCM, the word written into the DTCM


def _hw(off):
    return STRIP[off] | STRIP[off + 1] << 8


def _ror(v, n):
    n &= 31
    return ((v >> n) | (v << (32 - n))) & M32 if n else v


def _sext(v, bits):
    v &= (1 << bits) - 1
    return (v - (1 << bits)) & M32 if v >> (bits - 1) else v


def _s32(v):
    v &= M32
    return v - (1 << 32) if v >> 31 else v


def _sat(v):
    if v > 0x7FFFFFFF:
        return 0x7FFFFFFF, True
    if v < -0x80000000:
        return 0x80000000, True
    return v & M32, False


# --- the devices -------------------------------------------------------------------------------

def ldrh_odd(crane, p):
    v = _hw(p & ~1)
    return _ror(v, 8) if (crane == OLD and p & 1) else v


def ldrsh_odd(crane, p):
    if crane == OLD and p & 1:
        return _sext(STRIP[p], 8)       # LDRSH [odd] acts like LDRSB [odd]
    return _sext(_hw(p & ~1), 16)


def stm_base(crane, p):
    # STMIA Rb!,{r1,r2,r3} with Rb = r(1+p): the word stored for Rb, as an offset from the start of the strip.
    return 0 if (crane == NEW or p == 0) else 12


def ldm_base(crane, p):
    # LDMIA Rb!,{r1,r2,r3} with Rb = r(1+p), loading B0000000h + i. The ARM7 never writes back with Rb in the list;
    # the ARM9 does unless Rb is last. Chalk: 12 for "written back" (the pointer moved by 12 bytes), else the loaded word.
    if crane == NEW and p < 2:
        return 12
    return BUF_WORDS + p


def stm_empty(crane, p):
    # p 0: empty list in ARM state, p 1: a list of one register (ARM), p 2: empty list in Thumb state.
    # An empty list moves the base by 40h (ARMv4 and v5); one register moves it by 4.
    return 4 if p == 1 else 0x40


def ldm_empty(crane, p):
    return 4 if p == 1 else 0x40


def alu_pc(crane, p):
    # ADD r0,pc,r1,LSL #0 reads PC + 8; with the shift taken from a register PC + 12; MOV r0,pc reads PC + 8.
    return (8, 12, 8)[p]


QD_CASES = ((0x80000000, 0x40000000), (0x10000000, 0x40000000), (1, 1), (0x7FFFFFFF, 1))


def qdadd(crane, p):
    rm, rn = QD_CASES[p]
    d, q1 = _sat(2 * _s32(rn))
    res, q2 = _sat(_s32(rm) + _s32(d))
    return (res >> 16 & 0xFFFF) | (1 << 16 if (q1 or q2) else 0)


SMLA_CASES = ((0x7FFF, 0x7FFF, 0x7FFFFFFF), (3, 4, 5), (0x8000, 0x7FFF, 0x80000000))


def smla(crane, p):
    a, b, acc = SMLA_CASES[p]
    prod = _s32(_sext(a, 16)) * _s32(_sext(b, 16))
    s = prod + _s32(acc)
    q = s > 0x7FFFFFFF or s < -0x80000000
    return ((s & M32) >> 16 & 0xFFFF) | (1 << 16 if q else 0)


def ldrpc_b15(crane, p):
    return 1 if p == 1 else 2       # control bit 15 set: the PC load stays in ARM state (1), else it goes to Thumb (2)


# protection unit: p = ap * 5 + k, k: 0 priv read, 1 priv write, 2 priv fetch, 3 user read, 4 user write
PU_KINDS = ('PRIV READ', 'PRIV WRITE', 'PRIV FETCH', 'USER READ', 'USER WRITE')


def pu_access(crane, p):
    ap, k = divmod(p, 5)
    if k in (0, 1):                      # privileged data access: any permission but 0 allows it
        return 4 if ap == 0 else 0
    if k == 2:                           # privileged fetch: instruction permission 0 aborts
        return 3 if ap == 0 else 0
    if k == 3:                           # user read: permission 2 and 3
        return 0 if ap >= 2 else 4
    return 0 if ap == 3 else 4           # user write: permission 3 only


def pu_priority(crane, p):
    # p 0: the higher-numbered of two overlapping regions denies access, p 1: the lower one does.
    return 4 if p == 0 else 0


def vector(crane, p):
    return (2, 1, 3)[p]                  # SWI -> 2, undefined instruction -> 1, BKPT -> 3 (prefetch abort)


def tcm_mirror(crane, p):
    return 1 if p == 1 else 0            # a 32 KB virtual DTCM mirrors its 16 KB


def itcm_size(crane, p):
    return 1 if p == 0 else 0            # 32 MB virtual: the mirror word is ITCM; 16 KB virtual: no longer


def tcm_view(crane, p):
    return TCM_P2 if p == 0 else TCM_P1  # CPU read of a DTCM address sees the DTCM; a DMA read sees the main RAM behind


def tcm_loadmode(crane, p):
    return TCM_P2 if p == 0 else TCM_P1  # control bit 17 set: reads of the DTCM go to the bus behind


def cache_seq(crane, p):
    # [write C into a cached line], [clean it], DMA D into the RAM behind, [invalidate the line], read the word.
    # p 0-3: write C; bit 0 = clean it before the DMA, bit 1 = invalidate the line before the read.
    # p 4, 5: no write (the line holds the old word A, clean); p 5 invalidates the line before the read.
    # Documentation: GBATEK "ARM CP15 Cache Control" for the clean/invalidate operations; that a DMA bypasses the cache
    # and that a write-back line holds the new word until it is cleaned follow from the ARM946E-S technical reference manual.
    if p == 4:
        return CACHE_A
    return CACHE_C if p < 2 else CACHE_D


def icache_seq(crane, p):
    return 2 if p == 0 else 1            # p 0: invalidate after the rewrite (new code runs), p 1: not (old code runs)


# id order is the C enum (generated header)
OPS = (
    # name, pier, crane mask (1 old, 2 new), setups, function, setup label function
    ('LDRH_ODD',   0, 3, 4, ldrh_odd),
    ('LDRSH_ODD',  0, 3, 4, ldrsh_odd),
    ('LDRH_THUMB', 0, 3, 4, ldrh_odd),
    ('STM_BASE',   1, 3, 3, stm_base),
    ('LDM_BASE',   1, 3, 3, ldm_base),
    ('STM_EMPTY',  1, 1, 3, stm_empty),
    ('LDM_EMPTY',  1, 2, 3, ldm_empty),
    ('ALU_PC',     2, 1, 3, alu_pc),
    ('QDADD',      2, 2, 4, qdadd),
    ('SMLA',       2, 2, 3, smla),
    ('LDRPC_B15',  2, 2, 2, ldrpc_b15),
    ('PU_ACCESS',  3, 2, 20, pu_access),
    ('PU_PRIORITY', 3, 2, 2, pu_priority),
    ('VECTOR',     3, 2, 3, vector),
    ('TCM_MIRROR', 4, 2, 2, tcm_mirror),
    ('ITCM_SIZE',  4, 2, 2, itcm_size),
    ('TCM_VIEW',   4, 2, 2, tcm_view),
    ('TCM_LOADMODE', 4, 2, 2, tcm_loadmode),
    ('CACHE_SEQ',  5, 2, 6, cache_seq),
    ('ICACHE_SEQ', 5, 2, 2, icache_seq),
)
OP_ID = {o[0]: i for i, o in enumerate(OPS)}
MAX_SETUPS = 20


def model(op, crane, p):
    name, pier, mask, n, fn = OPS[op]
    if not mask >> crane & 1 or not 0 <= p < n:
        return None
    return fn(crane, p) & M32


def setup_label(op, p):
    """What the dial shows for setup p of a device (<= 16 characters)."""
    name = OPS[op][0]
    table = {
        'STM_BASE': ('MARK FIRST', 'MARK MIDDLE', 'MARK LAST'),
        'LDM_BASE': ('MARK FIRST', 'MARK MIDDLE', 'MARK LAST'),
        'STM_EMPTY': ('EMPTY GANG', 'ONE BOX', 'EMPTY, SHORT HOOK'),
        'LDM_EMPTY': ('EMPTY GANG', 'ONE BOX', 'EMPTY, SHORT HOOK'),
        'ALU_PC': ('PLAIN SHIFT', 'SHIFT BY SLOT', 'PLAIN COPY'),
        'QDADD': ('BIG AND BIGGER', 'TOP AND BIG', 'ONE AND ONE', 'TOP AND ONE'),
        'SMLA': ('BIG BOXES', 'SMALL BOXES', 'LOW STACK'),
        'LDRPC_B15': ('HABIT OFF', 'HABIT ON'),
        'PU_PRIORITY': ('HIGH ZONE REFUSES', 'LOW ZONE REFUSES'),
        'VECTOR': ('HORN', 'BLANK ORDER', 'RED FLAG'),
        'TCM_MIRROR': ('LOCKER 16K', 'LOCKER 32K'),
        'ITCM_SIZE': ('BIG SHELF', 'SMALL SHELF'),
        'TCM_VIEW': ('HAND LOOKS', 'BARGE LOOKS'),
        'TCM_LOADMODE': ('HATCH CLOSED', 'HATCH OPEN'),
        'CACHE_SEQ': ('KEEP, NO WIPE', 'CLEAN, NO WIPE', 'KEEP, WIPE', 'CLEAN, WIPE', 'NO WRITE, KEEP', 'NO WRITE, WIPE'),
        'ICACHE_SEQ': ('WIPE ORDERS', 'KEEP ORDERS'),
    }
    if name in ('LDRH_ODD', 'LDRSH_ODD', 'LDRH_THUMB'):
        return 'SLOT %d' % p
    if name == 'PU_ACCESS':
        return 'RANK %d %s' % (p // 5, PU_KINDS[p % 5])
    return table[name][p]
