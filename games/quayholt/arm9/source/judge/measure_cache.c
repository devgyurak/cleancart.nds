// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Pier F: the ARM9 caches (GBATEK "ARM CP15 Cache Control"): the data cache is write-back and its lines are
// only as fresh as the last clean or fill, the instruction cache is not coherent with data writes until it is
// invalidated. Every test uses a 32-byte buffer of its own in the cacheable main RAM (region 6), so exactly one
// line is involved; nothing here depends on which line the pseudo-random replacement picks. The items are
// unconfirmed until a hardware run agrees (cache behaviour is where an emulator that models no cache differs most).

#include <nds.h>

#include "cv_ipc.h"
#include "game/ledger.h"
#include "judge/cp15.h"
#include "judge/measure.h"
#include "qy_class.h"

static u32 cb[8] __attribute__((aligned(32)));        // the line under test
static u32 csrc[8] __attribute__((aligned(32)));      // DMA source / destination words
static u32 cdst[8] __attribute__((aligned(32)));
static u32 ccode[8] __attribute__((aligned(32)));     // the code the instruction cache test runs

#define RAW_BASE 64

static void raw9(int qv, const u32 *w, int n)
{
    for (int i = 0; i < n && i < 4; i++)
        CV_QY_RAW[RAW_BASE + qv * 4 + i] = w[i];
}

static void dma_word(const u32 *src, u32 *dst)
{
    DC_FlushRange(src, 32);
    dmaCopyWords(3, src, dst, 4);
}

static void inval_line(const void *p)
{
    __asm__ volatile("" ::: "memory");
    DC_InvalidateRange(p, 32);
}

static u32 run_code(void)
{
    return ((u32 (*)(void))(uintptr_t)ccode)();
}

static void code_set(u32 value)
{
    ccode[0] = 0xE3A00000u | value;     // mov r0, #value
    ccode[1] = 0xE12FFF1Eu;             // bx lr
    DC_FlushRange(ccode, 32);           // the data side is in memory
}

void measure_cache(void)
{
    u32 out[4] = { 0, 0, 0, 0 };
    int old = enterCriticalSection();
    volatile u32 *b = cb;
    volatile u32 *d = cdst;

    // Prerequisite: control bits 2 (data cache) and 12 (instruction cache) can be cleared and set and read back.
    {
        u32 c = qy9_get_ctrl();
        DC_FlushAll();
        qy9_set_ctrl(c & ~((1u << 2) | (1u << 12)));
        u32 off = qy9_get_ctrl();
        qy9_set_ctrl(c | (1u << 2) | (1u << 12));
        u32 on = qy9_get_ctrl();
        qy9_set_ctrl(c);
        ledger_flags(QV_PRE, 1u << 7, (off & ((1u << 2) | (1u << 12))) == 0 && (on & ((1u << 2) | (1u << 12))) == ((1u << 2) | (1u << 12)));
    }

    // F1: after a DMA has written the main RAM behind a cached line, the CPU still reads the old cached word.
    b[0] = 0xAAAA0001u;
    DC_FlushRange(cb, 32);
    (void)b[0];                                 // the line is filled
    csrc[0] = 0xBBBB0002u;
    dma_word(csrc, cb);                         // memory now holds BBBB0002h, the line AAAA0001h (clean)
    out[0] = b[0];
    raw9(QV_F1, out, 1);
    qy_res[QV_F1] = qy_class(out[0], 0xAAAA0001u, 0xBBBB0002u, 0xFFFFFFFFu);
    DC_FlushRange(cb, 32);

    // F2: a write to a cached line stays in the cache (write-back): a DMA read of the memory sees the old word.
    b[0] = 0xAAAA0003u;
    DC_FlushRange(cb, 32);
    (void)b[0];
    b[0] = 0xCCCC0004u;                         // dirty line, memory still AAAA0003h
    d[0] = 0;
    DC_FlushRange(cdst, 32);
    dmaCopyWords(3, cb, cdst, 4);
    inval_line(cdst);
    out[0] = d[0];
    raw9(QV_F2, out, 1);
    qy_res[QV_F2] = qy_class(out[0], 0xAAAA0003u, 0xCCCC0004u, 0xFFFFFFFFu);
    DC_FlushRange(cb, 32);

    // F3: invalidating a dirty line without cleaning it throws the written word away.
    b[0] = 0xAAAA0005u;
    DC_FlushRange(cb, 32);
    (void)b[0];
    b[0] = 0xCCCC0006u;
    inval_line(cb);
    out[0] = b[0];
    raw9(QV_F3, out, 1);
    qy_res[QV_F3] = qy_class(out[0], 0xAAAA0005u, 0xCCCC0006u, 0xFFFFFFFFu);

    // F4: code that was rewritten and cleaned out of the data cache keeps running in its old form until the
    // instruction cache is invalidated (CP15 c7,c5,0). out[0] first run, out[1] after the rewrite, out[2] after the invalidate.
    code_set(1);
    IC_InvalidateAll();
    out[0] = run_code();                        // fills the instruction cache line with "mov r0,#1"
    code_set(2);                                // memory holds "mov r0,#2" now
    out[1] = run_code();
    IC_InvalidateAll();
    out[2] = run_code();
    raw9(QV_F4, out, 3);
    if (out[0] == 1 && out[1] == 1 && out[2] == 2)
        qy_res[QV_F4] = QY_GBATEK;
    else if (out[0] == 1 && out[1] == 2 && out[2] == 2)
        qy_res[QV_F4] = QY_ALT2;                // coherent: no instruction cache
    else
        qy_res[QV_F4] = QY_OTHER;

    leaveCriticalSection(old);
    ledger_flags(QV_DONE, QD_CACHE, true);
}

u32 probe_cache(u32 which, u32 p)
{
    int old = enterCriticalSection();
    volatile u32 *b = cb;
    u32 res;
    if (which == 0)
    {
        // write C into a cached line, [clean it], DMA D into the memory behind, [invalidate the line], read the word.
        b[0] = 0xAAAA0001u;
        DC_FlushRange(cb, 32);
        (void)b[0];
        b[0] = 0xCCCC0002u;
        if (p & 1)
            qy9_dc_clean_range(cb, 32);
        csrc[0] = 0xBBBB0003u;
        dma_word(csrc, cb);
        if (p & 2)
            inval_line(cb);
        res = b[0];
        DC_FlushRange(cb, 32);
    }
    else
    {
        code_set(1);
        IC_InvalidateAll();
        (void)run_code();
        code_set(2);
        if (p == 0)
            IC_InvalidateAll();
        res = run_code();
    }
    leaveCriticalSection(old);
    return res;
}
