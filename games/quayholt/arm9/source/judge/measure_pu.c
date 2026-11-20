// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Piers D and E: the ARM9 protection unit and tightly coupled memory (see measure.h).
// GBATEK "ARM CP15 Control Register", "ARM CP15 Protection Unit (PU)", "ARM CP15 Tightly Coupled Memory (TCM)",
// "ARM CPU Exceptions", "DS DMA Transfers".
//
// Exception harness: while a test runs the control register's bit 13 is clear, so the vectors are at address 0
// in the ITCM, where this file puts its own table (judge/cp15.s). The handlers record what they found and
// resume the stub in system mode. Before anything is changed the whole CP15 state (control register, access
// permissions, the eight regions, cache and write buffer bits, DTCM and ITCM) is saved and the D-cache is
// cleaned; afterwards all of it is written back, the vector words at 0 are restored and the CPSR's I and F
// bits are put back. The tests use the two highest regions only where they must (7), and a window of main
// RAM (qy_win) nothing else lives in.

#include <nds.h>
#include <string.h>

#include "cv_ipc.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "judge/cp15.h"
#include "judge/measure.h"
#include "qy_class.h"

#define RAW_BASE 64   // ARM9 raw words: CV_QY_RAW[RAW_BASE + qv * 4 ..]

static u8 qy_win[4096] __attribute__((aligned(4096)));
static u8 qy_tcm_blk[0x4000] __attribute__((aligned(0x4000)));
static u32 qy_dma_dst[8] __attribute__((aligned(32)));
static u32 qy_dma_src[16] __attribute__((aligned(32)));

volatile QyExcRec qy_exc_rec;      // what the exception handlers (judge/cp15.s) recorded
u32 qy_exc_resume;                 // where they resume

static u32 st[QS_WORDS];   // the state before the harness
static u32 hs[QS_WORDS];   // the harness state tests go back to: low vectors, the vector area open
static u32 st_vec[16];
static u32 st_cpsr;

static void raw9(int qv, const u32 *w, int n)
{
    for (int i = 0; i < n && i < 4; i++)
        CV_QY_RAW[RAW_BASE + qv * 4 + i] = w[i];
}

#define WIN_REGION ((u32)(uintptr_t)qy_win | (11u << 1) | 1u)   // 4 KB at qy_win, enabled
#define DPERM_NIBBLE(v, n) (((v) >> (4 * (n))) & 0xFu)

static void set_nibble(u32 *v, int n, u32 x)
{
    *v = (*v & ~(0xFu << (4 * n))) | (x << (4 * n));
}

// Starts the exception harness: IRQs are masked by the caller (enterCriticalSection).
static void exc_begin(void)
{
    qy9_dc_flush_all();
    st_cpsr = qy9_get_cpsr();
    qy9_cp15_get(st);
    // The vector area (region 2, the null guard) must accept the vector fetches and the table writes whether
    // or not the TCM is checked against the regions.
    u32 dp = st[QS_DPERM], ip = st[QS_IPERM];
    set_nibble(&dp, 2, 3);
    set_nibble(&ip, 2, 3);
    qy9_set_dperm(dp);
    qy9_set_iperm(ip);
    volatile u32 *v = (volatile u32 *)0;
    for (int i = 0; i < 16; i++)
    {
        st_vec[i] = v[i];
        v[i] = qy9_exc_table[i];
    }
    qy9_set_ctrl(st[QS_CTRL] & ~(1u << 13));
    qy9_cp15_get(hs);
}

static void exc_end(void)
{
    qy9_set_ctrl(st[QS_CTRL] | (1u << 13));
    volatile u32 *v = (volatile u32 *)0;
    for (int i = 0; i < 16; i++)
        v[i] = st_vec[i];
    qy9_cp15_set(st);
    qy9_dc_flush_all();
    qy9_ic_inv_all();
    qy9_set_cpsr_c(st_cpsr & 0xFF);
}

// Puts the window under region 7 with the given data and instruction permissions (0..3: 0 none, 1 privileged only, 2 privileged read/write and user read-only, 3 all).
static void win_region(u32 dperm, u32 iperm)
{
    u32 dp = hs[QS_DPERM], ip = hs[QS_IPERM];
    set_nibble(&dp, 7, dperm);
    set_nibble(&ip, 7, iperm);
    // Region 7 normally maps the DTCM (the stack); while the window replaces it the stack is covered by region 5,
    // whose permission nibble must stay 3. The region is written again after the permissions: some emulators recompute every region only when a
    // region register is written, and a permission write alone leaves a lower region on top of a higher one.
    // (The first write comes first so the stack in the DTCM, region 7's old job, is never left without access.)
    qy9_set_region7(WIN_REGION);
    qy9_set_dperm(dp);
    qy9_set_iperm(ip);
    qy9_set_region7(WIN_REGION);
}

static void win_fill(u32 w)
{
    *(volatile u32 *)qy_win = w;
    qy9_dc_flush_range(qy_win, sizeof(qy_win));
}

static u32 mode(void)
{
    return qy_exc_rec.cpsr & 0x1F;
}

void measure_pu(void)
{
    // The harness needs address 0 (ITCM) open under region 2, which mpu_map_swram() repoints while the ARM9 owns
    // the shared WRAM: take the WRAM back to the ARM7 first.
    if (sys_wram_is_arm9())
        sys_wram_to_arm7();
    u32 out[4] = { 0, 0, 0, 0 };
    u32 v, a;
    int old = enterCriticalSection();

    // D1: the control register. Writing bits that are not writable (1, 8-11, 20-31) changes nothing; bits 3-6
    // always read 1 (GBATEK: the R/W bits are 0, 2, 7, 12-19).
    {
        const u32 W = 0x000FF085u, NW = 0xFFF00F7Au;
        u32 c = qy9_get_ctrl();
        qy9_set_ctrl((c & W) | NW);
        u32 r1 = qy9_get_ctrl();
        qy9_set_ctrl(c & W);
        u32 r2 = qy9_get_ctrl();
        qy9_set_ctrl(c);
        u32 want = (c & W) | 0x78u;
        out[0] = r1;
        out[1] = r2;
        out[2] = want;
        raw9(QV_D1, out, 3);
        qy_res[QV_D1] = (r1 == want && r2 == want) ? QY_GBATEK : QY_OTHER;
    }

    exc_begin();

    // Prerequisite: the harness catches SWI and an undefined instruction, LR = the instruction + 4.
    {
        a = qy9_do_swi();
        bool ok = qy_exc_rec.id == 2 && qy_exc_rec.lr == a + 4 && (qy_exc_rec.spsr & 0x1F) == 0x1F;
        a = qy9_do_undef();
        ok = ok && qy_exc_rec.id == 1 && qy_exc_rec.lr == a + 4;
        ledger_flags(QV_PRE, 1u << 3, ok);
    }

    // D2: BKPT takes the prefetch abort vector (0Ch) in abort mode, LR = BKPT + 4. 2 = it was taken as an
    // undefined instruction.
    a = qy9_do_bkpt();
    out[0] = qy_exc_rec.id;
    out[1] = qy_exc_rec.lr - a;
    out[2] = mode();
    out[3] = 0;
    raw9(QV_D2, out, 4);
    if (qy_exc_rec.id == 3 && mode() == 0x17 && qy_exc_rec.lr == a + 4)
        qy_res[QV_D2] = QY_GBATEK;
    else if (qy_exc_rec.id == 1)
        qy_res[QV_D2] = QY_ALT2;
    else
        qy_res[QV_D2] = QY_OTHER;

    // Prerequisite: a region with read/write permission gives no abort for a store and a load.
    win_fill(0x13572468u);
    win_region(3, 3);
    qy9_do_store((u32)(uintptr_t)qy_win + 4, 0x2468ACE0u);
    bool st_ok = qy_exc_rec.id == 0;
    qy9_do_load((u32)(uintptr_t)qy_win + 4, &v);
    ledger_flags(QV_PRE, 1u << 4, st_ok && qy_exc_rec.id == 0 && v == 0x2468ACE0u);
    qy9_cp15_set(hs);

    // D3: of overlapping regions the higher number wins. Case A: region 4 is the window with no access and
    // region 6 (4 MB of main RAM, holds the window) allows it: the load must work. Case B: region 7 is the window
    // with no access over region 6's read/write: the load must abort.
    {
        win_fill(0x13572468u);
        u32 dp = hs[QS_DPERM];
        set_nibble(&dp, 4, 0);
        qy9_set_region4(WIN_REGION);
        qy9_set_dperm(dp);
        qy9_set_region4(WIN_REGION);
        qy9_do_load((u32)(uintptr_t)qy_win, &v);
        u32 idA = qy_exc_rec.id;
        qy9_cp15_set(hs);
        win_region(0, 3);
        qy9_do_load((u32)(uintptr_t)qy_win, &v);
        u32 idB = qy_exc_rec.id;
        qy9_cp15_set(hs);
        out[0] = idA;
        out[1] = idB;
        out[2] = out[3] = 0;
        raw9(QV_D3, out, 4);
        if (idA == 0 && idB == 4)
            qy_res[QV_D3] = QY_GBATEK;
        else if (idA == 4 && idB == 0)
            qy_res[QV_D3] = QY_ALT2;      // the lower region won
        else
            qy_res[QV_D3] = QY_OTHER;
    }

    // D4: an access outside every region is a data abort (LR = the instruction + 8).
    a = qy9_do_load(0x10000000u, &v);
    out[0] = qy_exc_rec.id;
    out[1] = qy_exc_rec.lr - a;
    out[2] = mode();
    out[3] = 0;
    raw9(QV_D4, out, 4);
    if (qy_exc_rec.id == 4 && mode() == 0x17 && qy_exc_rec.lr == a + 8)
        qy_res[QV_D4] = QY_GBATEK;
    else if (qy_exc_rec.id == 0)
        qy_res[QV_D4] = QY_ALT2;          // no abort at all
    else
        qy_res[QV_D4] = QY_OTHER;

    // D5: access permission 2 (privileged read/write, user read-only): in user mode the read works and the write aborts.
    {
        u32 ud[4] = { 0, 0, 0, 0 };
        win_fill(0x77665544u);
        win_region(2, 3);
        a = qy9_do_user_rw((u32)(uintptr_t)qy_win, ud);
        qy9_cp15_set(hs);
        out[0] = ud[0];
        out[1] = qy_exc_rec.id;
        out[2] = qy_exc_rec.lr - a;
        out[3] = *(volatile u32 *)qy_win;
        raw9(QV_D5, out, 4);
        if (ud[0] == 0x77665544u && qy_exc_rec.id == 4 && qy_exc_rec.lr == a + 8 && out[3] == 0x77665544u)
            qy_res[QV_D5] = QY_GBATEK;
        else if (qy_exc_rec.id == 2)
            qy_res[QV_D5] = QY_ALT2;      // the write was not refused
        else
            qy_res[QV_D5] = QY_OTHER;
    }

    // D6: instruction permission 0 makes a fetch from the region a prefetch abort (abort mode). The LR is only
    // recorded: a branch into the region reports it differently on the emulators.
    {
        *(volatile u32 *)qy_win = 0xE12FFF1Eu;   // bx lr
        qy9_dc_flush_range(qy_win, sizeof(qy_win));
        qy9_ic_inv_all();
        win_region(3, 0);
        a = qy9_do_call((u32)(uintptr_t)qy_win);
        u32 id = qy_exc_rec.id, lr = qy_exc_rec.lr, md = mode();
        qy9_cp15_set(hs);
        out[0] = id;
        out[1] = lr - a;
        out[2] = md;
        out[3] = 0;
        raw9(QV_D6, out, 4);
        if (id == 3 && md == 0x17)
            qy_res[QV_D6] = QY_GBATEK;
        else if (id == 0)
            qy_res[QV_D6] = QY_ALT2;      // the code ran
        else
            qy_res[QV_D6] = QY_OTHER;
    }

    exc_end();
    leaveCriticalSection(old);
    ledger_flags(QV_DONE, QD_PU, true);
}

void measure_tcm(void)
{
    u32 out[4] = { 0, 0, 0, 0 };
    u32 v;
    int old = enterCriticalSection();

    DC_FlushAll();
    u32 dtcm = 0, itcm = 0;
    {
        u32 s[QS_WORDS];
        qy9_cp15_get(s);
        dtcm = s[QS_DTCM];
        itcm = s[QS_ITCM];
    }

    // Prerequisite: the DTCM reads back what was written to it.
    {
        volatile u32 *p = (volatile u32 *)((dtcm & ~0xFFFu) + 0x1000);
        u32 keep = *p;
        *p = 0x600DF00Du;
        bool ok = *p == 0x600DF00Du;
        *p = keep;
        ledger_flags(QV_PRE, 1u << 5, ok);
    }

    // Prerequisite: a DMA channel copies main RAM to main RAM.
    {
        for (int i = 0; i < 16; i++)
            qy_dma_src[i] = 0xC0DE0000u + (u32)i;
        memset(qy_dma_dst, 0, sizeof(qy_dma_dst));
        DC_FlushRange(qy_dma_src, sizeof(qy_dma_src));
        DC_FlushRange(qy_dma_dst, sizeof(qy_dma_dst));
        dmaCopyWords(3, qy_dma_src, qy_dma_dst, sizeof(qy_dma_dst));
        DC_InvalidateRange(qy_dma_dst, sizeof(qy_dma_dst));
        bool ok = true;
        for (int i = 0; i < 8; i++)
            ok = ok && qy_dma_dst[i] == 0xC0DE0000u + (u32)i;
        ledger_flags(QV_PRE, 1u << 6, ok);
    }

    // E1: the ITCM wins over an overlapping DTCM. The ITCM took the word (out[0]), the DTCM's own word is untouched.
    qy9_tcm_e1(out);
    raw9(QV_E1, out, 2);
    if (out[0] == 0xB1B2B3B4u && out[1] != 0xB1B2B3B4u)
        qy_res[QV_E1] = QY_GBATEK;
    else if (out[1] == 0xB1B2B3B4u)
        qy_res[QV_E1] = QY_ALT2;          // the DTCM won
    else
        qy_res[QV_E1] = QY_OTHER;

    // E2: a 32 KB virtual DTCM mirrors the 16 KB behind it (+4000h reads what +0 holds).
    qy9_tcm_e2(out);
    raw9(QV_E2, out, 1);
    qy_res[QV_E2] = qy_class(out[0], 0xA5C3E10Fu, 0xFFFFFFFFu, 0xFFFFFFFEu);
    if (qy_res[QV_E2] == QY_OTHER)
        qy_res[QV_E2] = QY_ALT2;          // not mirrored

    // E3: with a 16 KB virtual ITCM the mirror word at 01004000h is no longer ITCM (the bus behind answers); 2 = it
    // still reads the ITCM word.
    {
        volatile u32 *p = (volatile u32 *)0x01004000;
        u32 keep = *p;
        *p = 0x1CE10001u;
        qy9_set_itcm(0x0A);              // base 0, size 512 << 5 = 16 KB
        v = *p;
        qy9_set_itcm(itcm);
        *p = keep;
        out[0] = v;
        raw9(QV_E3, out, 1);
        qy_res[QV_E3] = (v == 0x1CE10001u) ? QY_ALT2 : QY_GBATEK;
    }

    // E4: a DMA read of a DTCM address returns the main RAM behind it. A static 16 KB block of main RAM is
    // reached through two mirrors: U (uncached) to put P1 in it, M for the DTCM, which is moved there.
    {
        u32 blk = (u32)(uintptr_t)qy_tcm_blk;
        u32 m = 0x02800000u | (blk & 0x3FFFFFu);
        u32 u = 0x02400000u | (blk & 0x3FFFFFu);
        DC_FlushRange(qy_tcm_blk, sizeof(qy_tcm_blk));
        DC_FlushRange(qy_dma_dst, sizeof(qy_dma_dst));
        memset(qy_dma_dst, 0, sizeof(qy_dma_dst));
        DC_FlushRange(qy_dma_dst, sizeof(qy_dma_dst));
        qy9_tcm_e4(out, m, u, (u32)(uintptr_t)qy_dma_dst);
        DC_InvalidateRange(qy_dma_dst, sizeof(qy_dma_dst));
        out[0] = qy_dma_dst[0];
        raw9(QV_E4, out, 1);
        if (out[0] == 0x4D41494Eu)
            qy_res[QV_E4] = QY_GBATEK;    // the main RAM
        else if (out[0] == 0xD7D70001u)
            qy_res[QV_E4] = QY_ALT2;      // the DMA saw the DTCM
        else
            qy_res[QV_E4] = QY_OTHER;

        // E5: DTCM load mode (control bit 17): the CPU's reads of the DTCM go to the bus behind (the main RAM
        // word P1) while writes still reach the DTCM. out[0] = normal read, out[1] = load mode read.
        DC_FlushRange(qy_tcm_blk, sizeof(qy_tcm_blk));
        qy9_tcm_e5(out, m, u);
        raw9(QV_E5, out, 2);
        if (out[0] == 0xD7D70001u && out[1] == 0x4D41494Eu)
            qy_res[QV_E5] = QY_GBATEK;
        else if (out[0] == 0xD7D70001u && out[1] == 0xD7D70001u)
            qy_res[QV_E5] = QY_ALT2;      // bit 17 ignored
        else
            qy_res[QV_E5] = QY_OTHER;
    }

    // E6: ITCM load mode (control bit 19): reads of the ITCM region (from code in main RAM) do not return the ITCM word.
    qy9_tcm_e6(out);
    raw9(QV_E6, out, 2);
    if (out[0] == 0xD7D70002u && out[1] != 0xD7D70002u)
        qy_res[QV_E6] = QY_GBATEK;
    else if (out[0] == 0xD7D70002u)
        qy_res[QV_E6] = QY_ALT2;          // bit 19 ignored
    else
        qy_res[QV_E6] = QY_OTHER;

    DC_FlushAll();
    leaveCriticalSection(old);
    ledger_flags(QV_DONE, QD_TCM, true);
}
