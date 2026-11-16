// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Room 3, the clock wall: the ARM9 timers (GBATEK "DS Timers") at their edges. Every
// check is a counter relationship or a window of a frame or more, never a cycle count.
// A frame is about 547 ticks of a 1/1024 timer.
//
// Result bits of the audit (lg_res[LV_CLOCK]):
//   0  a count-up timer ignores its prescaler bits (set to 3): it counts exactly the
//      overflows of the timer before it (timer 3 after timer 2, whose IRQs are counted)
//   1  one 32-bit write of reload and start: the counter starts at the new reload (in R..R+2)
//   2  a reload written while the timer runs waits for the next overflow
//   3  writing the control register again with start still 1 does not reload
//   4  a stopped timer reads the frozen counter (twice, a frame apart)
// raw: values[16] = overflows counted by the IRQ, [17] = the count-up timer, [18] = counter
// after the 32-bit write, [19] = counter before / [20] = after the reload write,
// [21] = counter after the next overflow, [22] = counter after stop and start again,
// [23] = frozen counter.

#include <nds.h>

#include "core/hw.h"
#include "core/sys.h"
#include "game/ledger.h"
#include "game/rooms.h"

#define T_DATA32(n) (*(vu32 *)(0x04000100 + ((n) << 2)))

static volatile u32 irq_count;

static void t2_irq(void)
{
    irq_count++;
}

static void frames(int n)
{
    while (n-- > 0)
        sys_vsync();
}

static void timers_off(void)
{
    for (int i = 0; i < 4; i++)
        CV_TM_CNT(i) = 0;
}

// Timer 2 overflows every 4096 ticks of the 1/256 clock (32 a second) and raises an IRQ; timer 3
// counts those overflows with the given prescaler bits. Returns the count-up value; *irqs is
// the number of IRQs. Timer 2 is stopped first, so the two are an exact pair.
static u32 cascade_run(int prescaler_bits, u32 *irqs)
{
    timers_off();
    irq_count = 0;
    irqSet(IRQ_TIMER2, t2_irq);
    irqEnable(IRQ_TIMER2);
    CV_TM_DATA(3) = 0;
    CV_TM_CNT(3) = CV_TM_ON | CV_TM_CASCADE | (u16)prescaler_bits;
    CV_TM_DATA(2) = (u16)(0x10000 - 4096);
    CV_TM_CNT(2) = CV_TM_ON | CV_TM_IRQ | CV_TM_DIV256;
    frames(30);
    CV_TM_CNT(2) = 0;
    frames(2);
    irqDisable(IRQ_TIMER2);
    u32 t3 = CV_TM_DATA(3);
    *irqs = irq_count;
    timers_off();
    return t3;
}

// One 32-bit store of control and reload to timer 1; the counter read straight after.
static u32 stroke(u16 reload)
{
    timers_off();
    CV_TM_DATA(1) = 0;
    T_DATA32(1) = ((u32)(CV_TM_ON | CV_TM_DIV1024) << 16) | reload;
    u32 c = CV_TM_DATA(1);
    timers_off();
    return c;
}

#define RUN_RELOAD 0xE000u

static void run_start(void)
{
    timers_off();
    CV_TM_DATA(0) = RUN_RELOAD;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1024;
}

// A reload written while the timer runs: the counter before and after the write, and the
// counter just after the next overflow.
static void late_reload(u16 r2, u32 *before, u32 *after, u32 *wrapped)
{
    run_start();
    frames(2);
    *before = CV_TM_DATA(0);
    CV_TM_DATA(0) = r2;
    *after = CV_TM_DATA(0);
    int guard = 0;
    while (CV_TM_DATA(0) >= RUN_RELOAD && guard++ < 60)
        frames(1);
    *wrapped = CV_TM_DATA(0);
    timers_off();
}

// The control register written again: with the start bit kept (toggle = false) or cleared and
// set again (toggle = true). Returns the counter before and after.
static void rewrite(bool toggle, u32 *c1, u32 *c2)
{
    run_start();
    frames(2);
    *c1 = CV_TM_DATA(0);
    if (toggle)
        CV_TM_CNT(0) = 0;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1024;
    *c2 = CV_TM_DATA(0);
    timers_off();
}

// A timer stopped after `n` frames, read twice a frame apart.
static void frozen(int n, u32 *c1, u32 *c2)
{
    run_start();
    frames(n);
    CV_TM_CNT(0) = 0;
    *c1 = CV_TM_DATA(0);
    frames(1);
    *c2 = CV_TM_DATA(0);
}

// ---------------------------------------------------------------------------
// Puzzles

static const char *const pre_l[] = { "0", "1", "2", "3" };
static u32 p1_hw(int a, int b)
{
    u32 irqs;
    u32 t3 = cascade_run(a, &irqs);
    return t3 == irqs && irqs > 0;
}
static u32 p1_model(int a, int b) { return 1; }

static const char *const reload_l[] = { "F000", "FC00" };
static const u16 reload_v[] = { 0xF000, 0xFC00 };
static u32 p2_hw(int a, int b)
{
    u32 c = stroke(reload_v[a]);
    return c >= reload_v[a] && c <= reload_v[a] + 2u ? reload_v[a] : c;
}
static u32 p2_model(int a, int b) { return reload_v[a]; }

static const char *const late_l[] = { "4000", "8000" };
static const u16 late_v[] = { 0x4000, 0x8000 };
static u32 p3_hw(int a, int b)
{
    u32 before, after, wrapped;
    late_reload(late_v[a], &before, &after, &wrapped);
    return wrapped >= late_v[a] && wrapped < late_v[a] + 0x800u ? late_v[a] : wrapped;
}
static u32 p3_model(int a, int b) { return late_v[a]; }

static const char *const hand_l[] = { "KEEP START", "STOP, START" };
static u32 p4_hw(int a, int b)
{
    u32 c1, c2;
    rewrite(a != 0, &c1, &c2);
    return c2 >= c1;
}
static u32 p4_model(int a, int b) { return a == 0; }

static const char *const wait_l[] = { "1 FRAME", "2 FRAMES" };
static u32 p5_hw(int a, int b)
{
    u32 c1, c2;
    frozen(a + 1, &c1, &c2);
    timers_off();
    return c1 == c2;
}
static u32 p5_model(int a, int b) { return 1; }

static const char *const dash_l[] = { "-" };

static const Puzzle clock_puzzles[] = {
    { "THE CHAINED GEAR", "TURNS ON EVERY OVERFLOW", "NOTCH", pre_l, 4, "-", dash_l, 1, p1_hw, p1_model, 1 },
    { "ONE STROKE", "STARTS AT F000", "RELOAD", reload_l, 2, "-", dash_l, 1, p2_hw, p2_model, 0xF000 },
    { "THE LATE SPRING", "RELOAD 8000 AFTER WRAP", "SPRING", late_l, 2, "-", dash_l, 1, p3_hw, p3_model, 0x8000 },
    { "THE STEADY HAND", "THE COUNT GOES ON", "HAND", hand_l, 2, "-", dash_l, 1, p4_hw, p4_model, 1 },
    { "THE STOPPED CLOCK", "THE SAME TWICE", "WAIT", wait_l, 2, "-", dash_l, 1, p5_hw, p5_model, 1 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_clock(void)
{
    u32 f = 0, irqs;
    u32 t3 = cascade_run(3, &irqs);
    lg_res[LV_RAW + 8] = irqs;
    lg_res[LV_RAW + 9] = t3;
    if (t3 == irqs && irqs > 5)
        f |= 1u << 0;
    ledger_flags(LV_PRE, 1u << 3, irqs > 0);   // L1 prerequisite: a timer overflow raises its IRQ

    u32 c = stroke(0xF000);
    lg_res[LV_RAW + 10] = c;
    if (c >= 0xF000 && c <= 0xF002)
        f |= 1u << 1;

    u32 before, after, wrapped;
    late_reload(0x4000, &before, &after, &wrapped);
    lg_res[LV_RAW + 11] = before;
    lg_res[LV_RAW + 12] = after;
    lg_res[LV_RAW + 13] = wrapped;
    bool late = after >= before && after < 0xF000 && wrapped >= 0x4000 && wrapped < 0x4800;

    u32 c1, c2;
    rewrite(false, &c1, &c2);
    if (late)
        f |= 1u << 2;
    // A start bit that went 0 -> 1 does reload: the counter drops back to the reload value.
    u32 t1, t2;
    rewrite(true, &t1, &t2);
    lg_res[LV_RAW + 14] = t2;
    if (c2 >= c1 && c2 - c1 < 64 && t2 < c1)   // kept: no reload; stop and start: reload
        f |= 1u << 3;

    frozen(2, &c1, &c2);
    timers_off();
    lg_res[LV_RAW + 15] = c1;
    if (c1 == c2 && c1 > RUN_RELOAD)
        f |= 1u << 4;

    lg_res[LV_CLOCK] = f;
}

const Room room_clock = {
    "THE CLOCK WALL", "UPPER TOWER", "TIMERS", clock_puzzles,
    (int)(sizeof(clock_puzzles) / sizeof(clock_puzzles[0])), audit_clock,
};
