// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The bench: CPU, bus and sound timing measured from the ARM9 (and, in later benches, the ARM7)
// with edge finding. A device that ends its work with a busy flag is started, the CPU then runs a
// sled of n ARM nops in ITCM (one ARM9 cycle each = half a bus cycle) and reads the flag once.
// The smallest n at which the flag is clear is the busy time up to a constant (the jump into the
// sled, the store and the load), and every value reported is the difference of two such edges, so
// the constant cancels. No polling loop, so no poll jitter. Everything runs with IME = 0.
// See AGENTS.md and expected.json.

#ifndef PD_BENCH_H
#define PD_BENCH_H

#include <nds.h>
#include <stdbool.h>

#define BENCH_NOT_MEASURED 0xFFFFFFFFu

// Math unit edges, in ARM9 cycles (half bus cycles).
enum { MATH_DIV32, MATH_DIV6432, MATH_DIV6464, MATH_SQRT, MATH_N };

typedef struct {
    u32 edge[MATH_N];   // sled length at which the busy flag was first clear (BENCH_NOT_MEASURED: never)
    bool results_ok;    // every quotient, remainder and root of the probe operations was right
    bool done;          // every edge was found
} BenchMath;

// Runs the four edge searches. Leaves the math unit idle and DIVCNT/SQRTCNT in mode 0.
void bench_math(BenchMath *m);

// Cascade check: timer 1 (counting timer 0's overflows) advances. Uses and leaves timers 0 and 1 stopped.
bool bench_timer_cascade(void);

#endif // PD_BENCH_H
