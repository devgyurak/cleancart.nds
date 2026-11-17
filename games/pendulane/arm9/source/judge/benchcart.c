// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// See benchcart.h. The delay between the start store and the read of ROMCTRL is d ARM9 cycles = q iterations of a countdown loop
// (subs + taken branch, k cycles each in ITCM) plus r nops (0 <= r < k). k is not assumed: it is measured with the stopwatch (timers 0
// and 1) over two long runs of the loop, and the benchmark stops when it is not within 1/16 of a whole number. A binary search over d
// finds the smallest delay at which busy is clear (busy is monotone in d). The same routine is used for every gap1 and clock rate,
// so the fixed path (the store, the call, the load) cancels in the difference.

#include <nds.h>

#include "core/cart.h"
#include "core/hw.h"
#include "judge/benchcart.h"

#define NOINLINE __attribute__((noinline))

#define ROMCTRL     (*(volatile u32 *)0x040001A4)
#define ROMCMD(i)   (((volatile u8 *)0x040001A8)[i])
#define ROM_BUSY    (1u << 31)
#define ROM_START   (1u << 31)
#define ROM_RESET   (1u << 29)
#define ROM_CLK8    (1u << 27)       // 0: 6.7 MHz (bus / 5), 1: 4.2 MHz (bus / 8)
#define ROM_GAP_CLK (1u << 28)       // gap1 is counted in clock pulses
#define ROM_GAP(n)  ((u32)(n) & 0x1FFF)

#define MAX_DELAY   40000u           // ARM9 cycles: far above gap1 = 0x200 at CLK/8 (about 8 * 0x200 * 2 + the transfer)
#define SPIN_LIMIT  4000000u

// r0 = start register, r1 = value, r2 = ROMCTRL, r3 = q (loop iterations), [sp] = r (nops, < 8): store, delay, load.
extern u32 pd_cart_probe(volatile u32 *start, u32 val, volatile u32 *busy, u32 q, u32 r);
// r0 = q: the loop alone (for the calibration).
extern void pd_cart_loop(u32 q);

asm(".section .itcm.text.pd_cart,\"ax\",%progbits\n"
    ".arm\n"
    ".balign 4\n"
    ".global pd_cart_probe\n"
    ".type pd_cart_probe, %function\n"
    "pd_cart_probe:\n"
    "    ldr   r12, [sp]\n"
    "    str   r1, [r0]\n"
    "    cmp   r3, #0\n"
    "    beq   2f\n"
    "1:  subs  r3, r3, #1\n"
    "    bne   1b\n"
    "2:  rsb   r12, r12, #8\n"
    "    add   pc, pc, r12, lsl #2\n"
    "    nop\n"
    "    .rept 8\n"
    "    nop\n"
    "    .endr\n"
    "    ldr   r0, [r2]\n"
    "    bx    lr\n"
    ".size pd_cart_probe, .-pd_cart_probe\n"
    ".global pd_cart_loop\n"
    ".type pd_cart_loop, %function\n"
    "pd_cart_loop:\n"
    "1:  subs  r0, r0, #1\n"
    "    bne   1b\n"
    "    bx    lr\n"
    ".size pd_cart_loop, .-pd_cart_loop\n"
    ".previous\n");

static bool wait_idle(void)
{
    for (u32 n = 0; n < SPIN_LIMIT; n++)
    {
        u32 r = ROMCTRL;
        if (!(r & ROM_BUSY))
            return true;
    }
    return false;
}

static u32 stopwatch(void)
{
    u32 h1, l, h2;
    do {
        h1 = CV_TM_DATA(1);
        l = CV_TM_DATA(0);
        h2 = CV_TM_DATA(1);
    } while (h1 != h2);
    return h1 << 16 | l;
}

// Bus cycles x 2 (= ARM9 cycles) taken by q loop iterations, one run.
static NOINLINE u32 loop_time(u32 q)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    CV_TM_DATA(0) = 0;
    CV_TM_DATA(1) = 0;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1;
    u32 a = stopwatch();
    pd_cart_loop(q);
    u32 b = stopwatch();
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    return (b - a) * 2;
}

// Cycles per loop iteration x 16 (two long runs, so the call and the stopwatch reads cancel); 0 on a time that is not plausible.
static u32 calibrate(void)
{
    const u32 q1 = 2000, q2 = 6000;
    u32 t1 = loop_time(q1), t2 = loop_time(q2);
    if (t2 <= t1)
        return 0;
    return (t2 - t1) * 16 / (q2 - q1);
}

// One trial: wait for idle, start the transfer and read busy after a delay of d ARM9 cycles.
static u32 trial(u32 ctrl, u32 d, u32 k)
{
    if (!wait_idle())
        return 0xFFFFFFFFu;
    return pd_cart_probe(&ROMCTRL, ctrl, &ROMCTRL, d / k, d % k);
}

// Smallest delay (ARM9 cycles) at which busy reads clear; 0xFFFFFFFF if it never does within MAX_DELAY (or the bus hung).
static NOINLINE u32 find_edge(u32 ctrl, u32 k)
{
    u32 r = trial(ctrl, MAX_DELAY, k);
    if (r == 0xFFFFFFFFu || (r & ROM_BUSY))
        return 0xFFFFFFFFu;
    u32 lo = 0, hi = MAX_DELAY;
    while (lo < hi)
    {
        u32 mid = (lo + hi) / 2;
        r = trial(ctrl, mid, k);
        if (r == 0xFFFFFFFFu)
            return r;
        if (r & ROM_BUSY)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo;
}

static bool skip;

void benchcart_skip(bool on)
{
    skip = on;
}

bool benchcart_run(s32 out[5])
{
    for (int i = 0; i < 5; i++)
        out[i] = -1;
    if (skip)
        return false;   // asked to skip (the nocart marker on the card)
    u16 ime = REG_IME;
    REG_IME = 0;        // the calibration needs the same quiet as the search
    u32 k16 = calibrate();
    out[4] = (s32)k16;
    u32 k = (k16 + 8) / 16;
    if (k < 1 || k > 7 || (k16 > k * 16 ? k16 - k * 16 : k * 16 - k16) > 1)
    {
        REG_IME = ime;
        return false;
    }

    bool prev = cart_bus_claim();
    REG_AUXSPICNTH = CARD_ENABLE >> 8;
    for (int i = 0; i < 8; i++)
        ROMCMD(i) = i == 0 ? 0xB8 : 0;
    bool ok = wait_idle();
    const u32 gap_small = 0x20, gap_big = 0x20 + 0x100;
    for (int clk = 0; ok && clk < 2; clk++)
    {
        // The header's own control word keeps the KEY2 and timing bits the card is in use with; gap1, rate, block size are ours.
        u32 base = (__NDSHeader->cardControl13 & ~(0x1FFFu | (1u << 15) | (7u << 24) | ROM_CLK8 | ROM_GAP_CLK | ROM_START))
                 | ROM_START | ROM_RESET | (clk ? ROM_CLK8 : 0);
        u32 a = find_edge(base | ROM_GAP(gap_small), k);
        u32 b = find_edge(base | ROM_GAP(gap_big), k);
        out[2 + clk] = (s32)a;   // raw edge, ARM9 cycles
        if (a == 0xFFFFFFFFu || b == 0xFFFFFFFFu)
        {
            ok = false;   // the busy flag never cleared
            break;
        }
        // An edge at 0 is a floor (the transfer ended before the delay could look), not a measurement.
        out[clk] = (a == 0 || b == 0) ? -1 : ((s32)b - (s32)a) / 2;   // half bus cycles to bus cycles
    }
    REG_IME = ime;
    cart_bus_release(prev);
    return ok;
}
