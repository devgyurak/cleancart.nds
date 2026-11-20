// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// See measure.h. Expected values are worked out by hand from GBATEK ("ARM CPU Memory Alignments",
// "ARM Opcodes: Memory: Block Data Transfer (LDM, STM)", "THUMB Opcodes: Memory Multiple Load/Store",
// "ARM Opcodes: Data Processing (ALU)", "ARM Opcodes: Special ARM9 Instructions (CLZ, QADD/QSUB)",
// "ARM Opcodes: Multiply and Multiply-Accumulate (MUL, MLA)", "ARM CP15 Control Register").

#include <nds.h>

#include "cv_ipc.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "judge/measure.h"
#include "judge/stubs9.h"
#include "qy_class.h"

static u32 buf[96] __attribute__((aligned(32)));

static void fill(u32 a, u32 b, u32 c, u32 d)
{
    for (int i = 0; i < 96; i++)
        buf[i] = 0xDEAD0000u + (u32)i;
    buf[0] = a;
    buf[1] = b;
    buf[2] = c;
    buf[3] = d;
}

// The ARM9 raw words of item `qv` go to the raw page next to the ARM7's.
static void raw9(int qv, const u32 *w, int n)
{
    for (int i = 0; i < n && i < 4; i++)
        CV_QY_RAW[64 + qv * 4 + i] = w[i];
}

// ARM7 result code of a step (0xFFFF when it never answered).
static u32 step7(u32 step)
{
    u32 r = sys_step7(step);
    return r == 0xFFFFFFFFu ? QV_UNMEASURED : r;
}

void measure_loads(void)
{
    qy_res[QV_A1] = step7(CV_QY_A1);
    qy_res[QV_A2] = step7(CV_QY_A2);
    qy_res[QV_A3] = step7(CV_QY_A3);
    ledger_flags(QV_DONE, QD_LOADS, true);
}

void measure_blocks(void)
{
    u32 out[4];
    u32 b = (u32)(uintptr_t)buf;

    qy_res[QV_B1] = step7(CV_QY_B1);
    qy_res[QV_B2] = step7(CV_QY_B2);

    // B3: ARMv5 LDM with the base in the list writes back unless the base is the LAST register (GBATEK:
    // "Writeback with Rb included in Rlist: ... ARMv5: writeback if Rb is the only register, or NOT the
    // last"). buf[0] = A0000001h, buf[1] = A0000002h: {r4,r5} from r4 -> buf+8, {r4,r5} from r5 -> A0000002h,
    // {r4} alone -> buf+4.
    fill(0xA0000001u, 0xA0000002u, 0, 0);
    int old = enterCriticalSection();
    qy9_b3(out, buf);
    leaveCriticalSection(old);
    raw9(QV_B3, out, 4);
    if (out[0] == b + 8 && out[1] == 0xA0000002u && out[2] == b + 4)
        qy_res[QV_B3] = QY_GBATEK;
    else if (out[0] == 0xA0000001u)
        qy_res[QV_B3] = QY_ALT2;      // no writeback (the ARMv4 rule)
    else
        qy_res[QV_B3] = QY_OTHER;

    qy_res[QV_B4] = step7(CV_QY_B4);
    qy_res[QV_B5] = step7(CV_QY_B5);

    // B6: STM {..PC} stores the address of the STM + 12 on both CPUs (GBATEK); 2 = +8 on both, 3 = they differ.
    u32 r7 = step7(CV_QY_B6);
    fill(0, 0, 0, 0);
    old = enterCriticalSection();
    qy9_b6(out, buf);
    leaveCriticalSection(old);
    raw9(QV_B6, out, 4);
    u32 r9 = qy_class(out[0], 12, 8, 0xFFFFFFFFu);
    qy_res[QV_B6] = r7 == r9 ? r9 : (r7 == QV_UNMEASURED ? QV_UNMEASURED : QY_ALT3);

    // B7: ARM9 empty list: no register is loaded, the base moves by 40h (ARM and Thumb).
    fill(0xC0DE0000u, 0, 0, 0);
    old = enterCriticalSection();
    qy9_b7(out, buf);
    leaveCriticalSection(old);
    raw9(QV_B7, out, 4);
    if (out[0] == b + 0x40 && out[1] == b + 0x100 + 0x40)
        qy_res[QV_B7] = QY_GBATEK;
    else if (out[0] == b && out[1] == b + 0x100)
        qy_res[QV_B7] = QY_ALT2;      // the empty list does nothing
    else
        qy_res[QV_B7] = QY_OTHER;
    ledger_flags(QV_DONE, QD_BLOCKS, true);
}

void measure_alu(void)
{
    u32 out[4];

    qy_res[QV_C1] = step7(CV_QY_C1);

    // C2: QDADD Rd,Rm,Rn = sat(Rm + sat(2 * Rn)) and sets Q when either saturates: 80000000h + sat(80000000h
    // -> 7FFFFFFFh) = FFFFFFFFh, in range, with Q set. (Doubling without its own saturation gives 80000000h.)
    int old = enterCriticalSection();
    qy9_c2(out);
    leaveCriticalSection(old);
    raw9(QV_C2, out, 4);
    qy_res[QV_C2] = (out[0] == 0xFFFFFFFFu && out[1] == 1) ? QY_GBATEK : (out[0] == 0x80000000u ? QY_ALT2 : QY_OTHER);

    // C3: SMLABB wraps (3FFF0001h + 7FFFFFFFh = BFFF0000h) and sets Q; SMLALBB wraps 64 bits and leaves Q alone.
    old = enterCriticalSection();
    qy9_c3(out);
    leaveCriticalSection(old);
    raw9(QV_C3, out, 4);
    if (out[0] == 0xBFFF0000u && out[1] == 1 && out[2] == 0x80000000u && out[3] == 0)
        qy_res[QV_C3] = QY_GBATEK;
    else if (out[0] == 0x7FFFFFFFu)
        qy_res[QV_C3] = QY_ALT2;      // saturated like QADD
    else if (out[0] == 0xBFFF0000u && out[3] == 1)
        qy_res[QV_C3] = QY_ALT3;      // the long multiply sets Q too
    else
        qy_res[QV_C3] = QY_OTHER;

    // C4: with CP15 control bit 15 set the ARM9 behaves like an ARMv4 core: LDR PC ignores bit 0 and stays
    // in ARM state (marker 1). Marker 2: it switched to Thumb (bit 15 ignored).
    old = enterCriticalSection();
    qy9_c4(out, 1);
    leaveCriticalSection(old);
    raw9(QV_C4, out, 1);
    qy_res[QV_C4] = qy_class(out[0], 1, 2, 0xFFFFFFFFu);
    ledger_flags(QV_DONE, QD_ALU, true);
}

void measure_prereq(void)
{
    u32 out[4];
    u32 ok = 0;

    // bit 0: aligned LDR/STR words round-trip (ARM9 here, ARM7 through its step).
    fill(0x12345678u, 0x9ABCDEF0u, 0, 0);
    volatile u32 *p = buf;
    p[2] = p[0] ^ 0xFFFFFFFFu;
    if (p[2] == 0xEDCBA987u && p[1] == 0x9ABCDEF0u && step7(CV_QY_PRE_WORD) == 1)
        ok |= 1;

    // bit 1: plain LDM/STM round trip in ARM and Thumb state on both CPUs.
    fill(0, 0, 0, 0);
    int old = enterCriticalSection();
    qy9_pre_ldm(out, buf);
    leaveCriticalSection(old);
    if (out[0] == 1 && out[1] == 1 && step7(CV_QY_PRE_LDM) == 1)
        ok |= 2;

    // bit 2: the ARM7 answers a measurement step at all.
    if (step7(CV_QY_PING) == 0xA5)
        ok |= 4;
    qy_res[QV_PRE] = (qy_res[QV_PRE] & ~7u) | ok;
}
