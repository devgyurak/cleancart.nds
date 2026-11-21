// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The harbour's devices on real hardware: `hw_run(op, crane, p)` runs the device with the player's setup and returns
// the chalk word (tools/qyjobs/model.py says what a correct console writes). The old crane is the ARM7, the new
// crane the ARM9. Instruction-level devices build one instruction and run it on the chosen CPU with the same
// one-instruction runner; the system devices go through the probes of judge/measure_pu.c and measure_cache.c.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/sys.h"
#include "game/devices.h"
#include "judge/measure.h"
#include "judge/stubs9.h"

typedef struct {
    u32 instr;
    int thumb;
    u32 base_mask;          // registers that get the test buffer's address added
    u32 r[8];
    u32 buf[8];
    // answer
    u32 out[9];             // r0..r7, CPSR
    u32 bufout[8];
    u32 bufaddr, codeaddr;
} Req;

static u32 d_buf[8] __attribute__((aligned(32)));
static u32 d_code[8] __attribute__((aligned(32)));

static void run_req(int crane, Req *q)
{
    if (crane == 1)
    {
        for (int i = 0; i < 8; i++)
            d_buf[i] = q->buf[i];
        u32 in[8], addr = (u32)(uintptr_t)d_code;
        for (int i = 0; i < 8; i++)
            in[i] = q->r[i] + ((q->base_mask >> i & 1) ? (u32)(uintptr_t)d_buf : 0);
        if (q->thumb)
        {
            ((u16 *)d_code)[0] = (u16)q->instr;
            ((u16 *)d_code)[1] = 0x4770;
            addr |= 1;
        }
        else
        {
            d_code[0] = q->instr;
            d_code[1] = 0xE12FFF1Eu;
        }
        DC_FlushRange(d_code, sizeof(d_code));
        DC_FlushRange(d_buf, sizeof(d_buf));
        int old = enterCriticalSection();
        IC_InvalidateAll();
        qy9_run1(in, q->out, addr);
        leaveCriticalSection(old);
        DC_FlushRange(d_buf, sizeof(d_buf));
        for (int i = 0; i < 8; i++)
            q->bufout[i] = d_buf[i];
        q->bufaddr = (u32)(uintptr_t)d_buf;
        q->codeaddr = (u32)(uintptr_t)d_code;
        return;
    }
    volatile u32 *r = CV_QY_REQ;
    r[0] = q->instr;
    r[1] = (u32)q->thumb;
    r[2] = q->base_mask;
    for (int i = 0; i < 8; i++)
    {
        r[3 + i] = q->r[i];
        r[11 + i] = q->buf[i];
    }
    DC_FlushRange((void *)CV_QY_REQ, 32 * 4);
    DC_InvalidateRange((void *)(CV_QY_RAW + QY_RUN1_OUT), 32 * 4);
    sys_step7(CV_QY_RUN1);
    DC_InvalidateRange((void *)(CV_QY_RAW + QY_RUN1_OUT), 32 * 4);
    volatile u32 *o = CV_QY_RAW + QY_RUN1_OUT;
    for (int i = 0; i < 9; i++)
        q->out[i] = o[i];
    for (int i = 0; i < 8; i++)
        q->bufout[i] = o[9 + i];
    q->bufaddr = o[17];
    q->codeaddr = o[18];
}

static void req_init(Req *q, u32 instr)
{
    memset(q, 0, sizeof(*q));
    q->instr = instr;
}

static void fill_strip(Req *q)
{
    q->buf[0] = 0x44338211u;      // bytes 11h 82h 33h 44h
    q->buf[1] = 0x88776655u;
}

static void fill_words(Req *q, u32 base)
{
    for (int i = 0; i < 8; i++)
        q->buf[i] = base + (u32)i;
}

static u32 qflag(const Req *q)
{
    return q->out[8] >> 27 & 1;
}

u32 hw_run(int op, int crane, int p)
{
    Req q;
    switch (op)
    {
        case QYOP_LDRH_ODD:
            req_init(&q, 0xE1D100B0u);            // ldrh r0, [r1]
            fill_strip(&q);
            q.base_mask = 1u << 1;
            q.r[1] = (u32)p;
            run_req(crane, &q);
            return q.out[0];
        case QYOP_LDRSH_ODD:
            req_init(&q, 0xE1D100F0u);            // ldrsh r0, [r1]
            fill_strip(&q);
            q.base_mask = 1u << 1;
            q.r[1] = (u32)p;
            run_req(crane, &q);
            return q.out[0];
        case QYOP_LDRH_THUMB:
            req_init(&q, 0x5A88u);                // ldrh r0, [r1, r2]
            q.thumb = 1;
            fill_strip(&q);
            q.base_mask = 1u << 1;
            q.r[2] = (u32)p;
            run_req(crane, &q);
            return q.out[0];
        case QYOP_STM_BASE:
        {
            int b = 1 + p;
            req_init(&q, 0xE8A0000Eu | ((u32)b << 16));   // stmia rb!, {r1, r2, r3}
            fill_words(&q, 0xDEAD0000u);
            q.r[1] = 0x1111u; q.r[2] = 0x2222u; q.r[3] = 0x3333u;
            q.base_mask = 1u << b;
            q.r[b] = 0;
            run_req(crane, &q);
            return q.bufout[p] - q.bufaddr;
        }
        case QYOP_LDM_BASE:
        {
            int b = 1 + p;
            req_init(&q, 0xE8B0000Eu | ((u32)b << 16));   // ldmia rb!, {r1, r2, r3}
            fill_words(&q, 0xB0000000u);
            q.base_mask = 1u << b;
            run_req(crane, &q);
            return q.out[b] == q.bufaddr + 12 ? 12 : q.out[b];
        }
        case QYOP_STM_EMPTY:
        case QYOP_LDM_EMPTY:
        {
            bool st = op == QYOP_STM_EMPTY;
            static const u32 arm_st[] = { 0xE8A40000u, 0xE8A40002u }, arm_ld[] = { 0xE8B40000u, 0xE8B40002u };
            req_init(&q, p == 2 ? (st ? 0xC400u : 0xCC00u) : (st ? arm_st : arm_ld)[p]);
            q.thumb = p == 2;
            fill_words(&q, 0xDEAD0000u);
            q.base_mask = 1u << 4;
            run_req(crane, &q);
            return q.out[4] - q.bufaddr;
        }
        case QYOP_ALU_PC:
        {
            static const u32 ins[] = { 0xE08F0001u, 0xE08F0211u, 0xE1A0000Fu };   // add r0,pc,r1 / ..lsl r2 / mov r0,pc
            req_init(&q, ins[p]);
            run_req(crane, &q);
            return q.out[0] - q.codeaddr;
        }
        case QYOP_QDADD:
        {
            static const u32 rm[] = { 0x80000000u, 0x10000000u, 1u, 0x7FFFFFFFu }, rn[] = { 0x40000000u, 0x40000000u, 1u, 1u };
            req_init(&q, 0xE1420051u);            // qdadd r0, r1, r2
            q.r[1] = rm[p];
            q.r[2] = rn[p];
            run_req(crane, &q);
            return (q.out[0] >> 16 & 0xFFFFu) | (qflag(&q) << 16);
        }
        case QYOP_SMLA:
        {
            static const u32 a[] = { 0x7FFFu, 3u, 0x8000u }, b[] = { 0x7FFFu, 4u, 0x7FFFu }, c[] = { 0x7FFFFFFFu, 5u, 0x80000000u };
            req_init(&q, 0xE1003281u);            // smlabb r0, r1, r2, r3
            q.r[1] = a[p];
            q.r[2] = b[p];
            q.r[3] = c[p];
            run_req(crane, &q);
            return (q.out[0] >> 16 & 0xFFFFu) | (qflag(&q) << 16);
        }
        case QYOP_LDRPC_B15:
        {
            u32 out[4] = { 0, 0, 0, 0 };
            int old = enterCriticalSection();
            qy9_c4(out, (u32)p);
            leaveCriticalSection(old);
            return out[0];
        }
        case QYOP_PU_ACCESS:
            return probe_pu_access((u32)(p / 5), (u32)(p % 5));
        case QYOP_PU_PRIORITY:
            return probe_pu_priority((u32)p);
        case QYOP_VECTOR:
            return probe_vector((u32)p);
        case QYOP_TCM_MIRROR:
            return probe_tcm(0, (u32)p);
        case QYOP_ITCM_SIZE:
            return probe_tcm(1, (u32)p);
        case QYOP_TCM_VIEW:
            return probe_tcm(2, (u32)p);
        case QYOP_TCM_LOADMODE:
            return probe_tcm(3, (u32)p);
        case QYOP_CACHE_SEQ:
            return probe_cache(0, (u32)p);
        case QYOP_ICACHE_SEQ:
            return probe_cache(1, (u32)p);
        default:
            return QY_HW_NONE;
    }
}
