// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 4, the belt: ARM9 DMA (GBATEK "DS DMA Transfers") at its edges. Channels CV_DMA_CH_HBLANK (1) and
// CV_DMA_CH_MISC (3) for the lines, channel 0 for the long copy of the ITCM item; CV_DMA_CH_VBLANK (2) carries
// the OAM upload and is left alone.
//
// Result bits of the audit (lg_res[LV_BELT]):
//   0  a VBlank repeat DMA does not reload SAD: frame k copies the k-th piece of the source
//   1  destination mode 3 (increment / reload) reloads DAD every repeat
//   2  destination mode 0 continues where the last repeat stopped
//   3  after a transfer SAD, DAD and CNT_L read their initial values and the enable bit is off
//   4  (L1 prerequisite) source decrement with destination increment reverses a block
//   5  the CPU keeps running ITCM code while a long DMA runs: a DMA of 128 KB plus an ITCM loop
//      of half its length takes about the DMA alone, not DMA plus loop (ratio of two timer
//      readings, a factor of 2 apart)
// raw: values[24] = dst[0] / [25] = dst[2] after the mode 3 repeat, [26] = dst[7] after mode 0,
// [27..29] = SAD / DAD / CNT after the immediate copy, [30] = DMA only / [31] = DMA plus loop
// (timer ticks), [32] = the ITCM function's address.

#include <nds.h>
#include <string.h>

#include "core/hw.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "game/rooms.h"

#define SRC(n) (*(vu32 *)(0x040000B0 + (n) * 12))
#define DST(n) (*(vu32 *)(0x040000B4 + (n) * 12))
#define CNT(n) (*(vu32 *)(0x040000B8 + (n) * 12))


static u16 src16[16] __attribute__((aligned(32)));
static u16 dst16[16] __attribute__((aligned(32)));
static u32 src32[8] __attribute__((aligned(32)));
static u32 dst32[8] __attribute__((aligned(32)));

static void arrays_init(void)
{
    for (int i = 0; i < 16; i++)
    {
        src16[i] = (u16)(i + 1);
        dst16[i] = 0xFFFF;
    }
    for (int i = 0; i < 8; i++)
    {
        src32[i] = 0x11110000u + (u32)i + 1;
        dst32[i] = 0xFFFFFFFFu;
    }
    DC_FlushRange(src16, sizeof(src16));
    DC_FlushRange(dst16, sizeof(dst16));
    DC_FlushRange(src32, sizeof(src32));
    DC_FlushRange(dst32, sizeof(dst32));
}

// A VBlank repeat DMA on channel 1 copies two 16-bit units a frame; run `n` frames.
static void reel(int n, u32 dst_mode)
{
    arrays_init();
    CNT(1) = 0;
    SRC(1) = (u32)src16;
    DST(1) = (u32)dst16;
    CNT(1) = CV_DMA_ON | CV_DMA_REPEAT | CV_DMA_VBLANK | CV_DMA_16 | dst_mode | 2;
    for (int i = 0; i < n; i++)
        sys_vsync();
    while (REG_VCOUNT < 100 || REG_VCOUNT > 200)
        ;
    CNT(1) = 0;
    DC_InvalidateRange(dst16, sizeof(dst16));
}

// An immediate 32-bit copy of 8 words on channel 3, optionally from the end of the source back.
static void copy8(bool reverse, u32 *sad, u32 *dad, u32 *cnt)
{
    arrays_init();
    CNT(3) = 0;
    SRC(3) = (u32)(reverse ? &src32[7] : &src32[0]);
    DST(3) = (u32)dst32;
    CNT(3) = CV_DMA_ON | CV_DMA_NOW | CV_DMA_32 | (reverse ? DMA_SRC_DEC : 0) | 8;
    while (CNT(3) & CV_DMA_ON)
        ;
    if (sad)
    {
        *sad = SRC(3);
        *dad = DST(3);
        *cnt = CNT(3);
    }
    DC_InvalidateRange(dst32, sizeof(dst32));
}

// ---------------------------------------------------------------------------
// DMA while ITCM code runs

#define BIG_WORDS 32768
static u32 big_src[BIG_WORDS] __attribute__((aligned(32)));
static u32 big_dst[BIG_WORDS] __attribute__((aligned(32)));

static u32 now32(void)
{
    u32 hi, lo;
    do
    {
        hi = CV_TM_DATA(1);
        lo = CV_TM_DATA(0);
    } while (hi != CV_TM_DATA(1));
    return hi << 16 | lo;
}

static void clock_start(void)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    CV_TM_DATA(0) = 0;
    CV_TM_DATA(1) = 0;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1;
}

// Starts the big copy on channel 0 (when `dma`), spins `n` times, then waits for the copy to end.
// The two variants differ in where the code lives (ITCM, or main RAM behind the instruction cache).
static inline __attribute__((always_inline)) void dma_start(void)
{
    SRC(0) = (u32)big_src;
    DST(0) = (u32)big_dst;
    CNT(0) = CV_DMA_ON | CV_DMA_NOW | CV_DMA_32 | BIG_WORDS;
}

// The loop of the main-RAM variant is a function of its own on its own cache line: it has to come over the
// bus after the DMA started, not ride along in the line that held the code that started it.
__attribute__((noinline, aligned(32))) static void spin_loop_main(u32 n)
{
    while (n--)
        asm volatile("");
}

ITCM_CODE __attribute__((noinline)) static void spin_itcm(u32 n, bool dma)
{
    if (dma)
        dma_start();
    while (n--)
        asm volatile("");
    if (dma)
        while (CNT(0) & CV_DMA_ON)
            ;
}

__attribute__((noinline)) static void spin_main(u32 n, bool dma)
{
    if (dma)
        dma_start();
    spin_loop_main(n);
    if (dma)
        while (CNT(0) & CV_DMA_ON)
            ;
}

typedef void (*SpinFn)(u32, bool);

static u32 timed(SpinFn fn, u32 n, bool dma)
{
    u32 t0 = now32();
    fn(n, dma);
    return now32() - t0;
}

// 1 when the loop overlaps the DMA. t_dma and t_both are returned for the diagnostics.
static u32 overlap(SpinFn fn, u32 *t_dma, u32 *t_both)
{
    for (u32 i = 0; i < BIG_WORDS; i++)
        big_src[i] = i * 2654435761u;
    DC_FlushRange(big_src, sizeof(big_src));
    DC_FlushRange(big_dst, sizeof(big_dst));
    int ime = REG_IME;
    REG_IME = 0;
    clock_start();
    u32 td = timed(fn, 0, true);
    const u32 n0 = 20000;
    u32 tl0 = timed(fn, n0, false);
    u32 n = (u32)((u64)n0 * td / (2 * (tl0 ? tl0 : 1)));
    // The main-RAM variant has run twice and sits in the instruction cache: the ITCM question is about
    // code that must come over the bus, so start the third run cold.
    IC_InvalidateAll();
    u32 tb = timed(fn, n, true);
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    REG_IME = ime;
    *t_dma = td;
    *t_both = tb;
    // The loop alone takes about td / 2: overlapped the total stays near td, serialized it is 1.5 td.
    // A DMA that ends at once (td under 32768 ticks for 32768 words) says nothing about the CPU.
    return td >= BIG_WORDS && tb < td + td / 4;
}

// ---------------------------------------------------------------------------
// Puzzles

static const char *const frames_l[] = { "2", "3", "4" };
static u32 p1_hw(int a, int b)
{
    reel(a + 2, CV_DMA_DST_RELOAD);
    return dst16[0];
}
static u32 p1_model(int a, int b) { return 2u * (a + 2 - 1) + 1; }

static const char *const dmode_l[] = { "RELOAD", "COUNT ON" };
static u32 p2_hw(int a, int b)
{
    reel(3, a ? DMA_DST_INC : CV_DMA_DST_RELOAD);
    return dst16[2];
}
static u32 p2_model(int a, int b) { return a ? 3 : 0xFFFF; }

static const char *const reg_l[] = { "SAD", "DAD", "COUNT" };
static u32 p3_hw(int a, int b)
{
    u32 sad, dad, cnt;
    copy8(false, &sad, &dad, &cnt);
    u32 now = a == 0 ? sad : a == 1 ? dad : cnt & 0xFFFF;
    u32 was = a == 0 ? (u32)src32 : a == 1 ? (u32)dst32 : 8;
    return now == was;
}
static u32 p3_model(int a, int b) { return 1; }

static const char *const dir_l[] = { "FORWARD", "BACKWARD" };
static u32 p4_hw(int a, int b)
{
    copy8(a != 0, 0, 0, 0);
    return dst32[0] & 0xFFFF;
}
static u32 p4_model(int a, int b) { return a ? 8 : 1; }

static const char *const where_l[] = { "MAIN RAM", "ITCM" };
static u32 p5_hw(int a, int b)
{
    u32 td, tb;
    return overlap(a ? spin_itcm : spin_main, &td, &tb);
}
static u32 p5_model(int a, int b) { return a != 0; }

static const char *const dash_l[] = { "-" };

static const Puzzle belt_puzzles[] = {
    { "THE FILM REEL", "PIECE 7 ON THE GLASS", "FRAMES", frames_l, 3, "-", dash_l, 1, p1_hw, p1_model, 7 },
    { "THE FIXED SPOOL", "THE THIRD SLOT FILLED", "SPOOL", dmode_l, 2, "-", dash_l, 1, p2_hw, p2_model, 3 },
    { "THE UNMOVED DIAL", "THE DIAL SHOWS ITS START", "DIAL", reg_l, 3, "-", dash_l, 1, p3_hw, p3_model, 1 },
    { "THE BACKWARD REEL", "FIRST SLOT HOLDS 8", "WIND", dir_l, 2, "-", dash_l, 1, p4_hw, p4_model, 8 },
    { "WORK WHILE IT TURNS", "THE CLERK KEEPS WORKING", "CLERK AT", where_l, 2, "-", dash_l, 1,
      p5_hw, p5_model, 1 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_belt(void)
{
    u32 f = 0;

    reel(4, CV_DMA_DST_RELOAD);
    lg_res[LV_RAW + 16] = dst16[0];
    lg_res[LV_RAW + 17] = dst16[2];
    if (dst16[0] == 7 && dst16[1] == 8)
        f |= 1u << 0;
    if (dst16[2] == 0xFFFF)
        f |= 1u << 1;

    reel(4, DMA_DST_INC);
    lg_res[LV_RAW + 18] = dst16[7];
    bool seq = true;
    for (int i = 0; i < 8; i++)
        seq = seq && dst16[i] == i + 1;
    if (seq)
        f |= 1u << 2;

    u32 sad, dad, cnt;
    copy8(false, &sad, &dad, &cnt);
    lg_res[LV_RAW + 19] = sad;
    lg_res[LV_RAW + 20] = dad;
    lg_res[LV_RAW + 21] = cnt;
    if (sad == (u32)src32 && dad == (u32)dst32 && (cnt & 0xFFFF) == 8 && !(cnt & CV_DMA_ON))
        f |= 1u << 3;

    copy8(true, 0, 0, 0);
    bool rev = true;
    for (int i = 0; i < 8; i++)
        rev = rev && dst32[i] == 0x11110000u + (u32)(8 - i);
    if (rev)
        f |= 1u << 4;

    u32 td, tb;
    if (overlap(spin_itcm, &td, &tb))
        f |= 1u << 5;
    lg_res[LV_RAW + 22] = td;
    lg_res[LV_RAW + 23] = tb;
    lg_res[LV_RAW + 24] = (u32)(void *)spin_itcm;

    lg_res[LV_BELT] = f;
}

const Room room_belt = {
    "THE BELT", "LOWER TOWER", "DMA", belt_puzzles,
    (int)(sizeof(belt_puzzles) / sizeof(belt_puzzles[0])), audit_belt,
};
