// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/hw.h"
#include "judge/bench.h"

#define NOINLINE __attribute__((noinline))

#define SLED_MAX 160
#define BUSY     0x8000u
#define SPIN_LIMIT 4000000u

// A wait that gives up (a unit that never goes idle) returns false instead of hanging the ROM.
#define SPIN_WHILE(cond) ({ u32 n_ = 0; bool ok_ = true; while (cond) { if (++n_ > SPIN_LIMIT) { ok_ = false; break; } } ok_; })

// One trial, all in one ARM routine so nothing but the store, the jump into the sled and the load sits
// between the start of the device and the read of its busy register:
//   str val -> *start; n nops (the add skips SLED_MAX - n of them; pc reads 8 ahead, so the filler nop after the
//   add is skipped as well); ldrh *busy.
// u32 pd_probe(volatile u32 *start /*r0*/, u32 val /*r1*/, volatile u16 *busy /*r2*/, u32 n /*r3, <= SLED_MAX*/)
extern u32 pd_probe(volatile u32 *start, u32 val, volatile u16 *busy, u32 n);
asm(".section .itcm.text.pd_probe,\"ax\",%progbits\n"
    ".arm\n"
    ".balign 4\n"
    ".global pd_probe\n"
    ".type pd_probe, %function\n"
    "pd_probe:\n"
    "    str   r1, [r0]\n"
    "    rsb   r3, r3, #160\n"
    "    add   pc, pc, r3, lsl #2\n"
    "    nop\n"
    "    .rept 160\n"
    "    nop\n"
    "    .endr\n"
    "    ldrh  r0, [r2]\n"
    "    bx    lr\n"
    ".size pd_probe, .-pd_probe\n"
    ".previous\n");

static inline u32 probe(volatile u32 *start, u32 val, volatile u16 *busy, u32 n)
{
    return pd_probe(start, val, busy, n > SLED_MAX ? SLED_MAX : n);
}

// The smallest sled length at which the busy flag reads clear; the device is idle before every trial.
static ITCM_CODE NOINLINE u32 find_edge(volatile u32 *start, u32 val, volatile u16 *busy)
{
    for (u32 n = 0; n <= SLED_MAX; n++)
    {
        if (!SPIN_WHILE(*busy & BUSY))
            return BENCH_NOT_MEASURED;
        if (!(probe(start, val, busy, n) & BUSY))
            return n;
    }
    return BENCH_NOT_MEASURED;
}

#define DIV_DENOM_HI (*(volatile u32 *)0x0400029C)

static bool idle_div(void) { return SPIN_WHILE(CV_DIVCNT & BUSY); }
static bool idle_sqrt(void) { return SPIN_WHILE(CV_SQRTCNT & BUSY); }

void bench_math(BenchMath *m)
{
    u32 ime = REG_IME;
    REG_IME = 0;
    m->results_ok = true;
    m->done = true;

    // Mode 0: 32 / 32. 100000 / 7 = 14285 remainder 5.
    CV_DIVCNT = 0;
    CV_DIV_NUMER32 = 100000;
    CV_DIV_DENOM32 = 7;
    idle_div();
    m->results_ok &= CV_DIV_RESULT32 == 14285 && CV_DIV_REM32 == 5;
    m->edge[MATH_DIV32] = find_edge((volatile u32 *)&CV_DIV_DENOM32, 7, &CV_DIVCNT);

    // Mode 1: 64 / 32. 0x123456789ABC / 12345.
    const s64 num = 0x123456789ABCll;
    CV_DIVCNT = 1;
    CV_DIV_NUMER = num;
    CV_DIV_DENOM32 = 12345;
    idle_div();
    m->results_ok &= CV_DIV_RESULT64_LO() == (u32)(num / 12345) && CV_DIV_RESULT64_HI() == (u32)((num / 12345) >> 32)
                     && CV_DIV_REM32 == (u32)(num % 12345);
    m->edge[MATH_DIV6432] = find_edge((volatile u32 *)&CV_DIV_DENOM32, 12345, &CV_DIVCNT);

    // Mode 2: 64 / 64.
    CV_DIVCNT = 2;
    CV_DIV_NUMER = num;
    DIV_DENOM_HI = 0;
    CV_DIV_DENOM32 = 12345;
    idle_div();
    m->results_ok &= CV_DIV_RESULT64_LO() == (u32)(num / 12345) && CV_DIV_RESULT64_HI() == (u32)((num / 12345) >> 32)
                     && CV_DIV_REM32 == (u32)(num % 12345);
    m->edge[MATH_DIV6464] = find_edge((volatile u32 *)&CV_DIV_DENOM32, 12345, &CV_DIVCNT);

    // Square root, 32-bit input: sqrt(1000000) = 1000.
    CV_SQRTCNT = 0;
    CV_SQRT_PARAM32 = 1000000;
    idle_sqrt();
    m->results_ok &= CV_SQRT_RESULT == 1000;
    m->edge[MATH_SQRT] = find_edge(&CV_SQRT_PARAM32, 1000000, &CV_SQRTCNT);

    // An edge at sled length 0 is a floor, not a measurement (the busy time is shorter than the fixed path).
    for (int i = 0; i < MATH_N; i++)
        if (m->edge[i] == BENCH_NOT_MEASURED || m->edge[i] == 0)
            m->done = false;
    idle_div();
    idle_sqrt();
    CV_DIVCNT = 0;
    REG_IME = ime;
}

bool bench_timer_cascade(void)
{
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    CV_TM_DATA(0) = 0;
    CV_TM_DATA(1) = 0;
    CV_TM_CNT(1) = CV_TM_ON | CV_TM_CASCADE;
    CV_TM_CNT(0) = CV_TM_ON | CV_TM_DIV1;
    // Timer 0 wraps after 65536 bus cycles; wait for the first wrap.
    bool ok = SPIN_WHILE(CV_TM_DATA(1) == 0) && CV_TM_DATA(1) == 1;
    CV_TM_CNT(0) = 0;
    CV_TM_CNT(1) = 0;
    return ok;
}
