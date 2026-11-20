// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Demo builds (make DEMO=<name>): the rehearsal as its own ROM, and one demo per pier that measures
// only that pier's items, for emulator CI and the QA sheet. Every demo fills the same block layout
// (game/ledger.h; ids QYLO, QYBL, ...) and its own words only; the judge ignores the items of other piers.

#include <nds.h>
#include <string.h>

#include "core/sdcard.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "game/ledger.h"
#include "assets.h"
#include "game/devices.h"
#include "game/rehearsal.h"
#include "judge/measure.h"

typedef struct {
    const char *name;
    u32 mask;
    u32 id;    // the demo's block id, "QYLO" ... (expected.json boots)
} PierDemo;

#define ID4(a, b, c, d) ((u32)(a) | (u32)(b) << 8 | (u32)(c) << 16 | (u32)(d) << 24)

static const PierDemo pier_demos[] = {
    { "l2-loads", QD_LOADS, ID4('Q', 'Y', 'L', 'O') },
    { "l2-blocks", QD_BLOCKS, ID4('Q', 'Y', 'B', 'L') },
    { "l2-alu", QD_ALU, ID4('Q', 'Y', 'A', 'L') },
    { "l2-pu", QD_PU, ID4('Q', 'Y', 'P', 'U') },
    { "l2-tcm", QD_TCM, ID4('Q', 'Y', 'T', 'C') },
    { "l2-cache", QD_CACHE, ID4('Q', 'Y', 'C', 'A') },
};
#define N_PIER_DEMOS ((int)(sizeof(pier_demos) / sizeof(pier_demos[0])))

static void pier_run(const char *name)
{
    for (int i = 0; i < N_PIER_DEMOS; i++)
        if (strcmp(name, pier_demos[i].name) == 0)
        {
            ledger_clear();
            rehearsal_measure(pier_demos[i].mask);
            ledger_publish(pier_demos[i].id, cv_frame);
            return;
        }
}

static void l2_loads(const char *n) { pier_run(n); }
static void l2_blocks(const char *n) { pier_run(n); }
static void l2_alu(const char *n) { pier_run(n); }
static void l2_pu(const char *n) { pier_run(n); }
static void l2_tcm(const char *n) { pier_run(n); }
static void l2_cache(const char *n) { pier_run(n); }

// What the QA run's judge suite (core/qasuite.h) runs after the credits.
const QaSuiteDemo demo_suite[] = {
    { "l2-loads", l2_loads }, { "l2-blocks", l2_blocks }, { "l2-alu", l2_alu },
    { "l2-pu", l2_pu }, { "l2-tcm", l2_tcm }, { "l2-cache", l2_cache },
};
const int demo_suite_count = (int)(sizeof(demo_suite) / sizeof(demo_suite[0]));

// make DEMO=devcheck: runs every device of the harbour on both cranes with every setup and lists where the hardware's
// chalk word differs from the model (the "crooked boxes"): values[0] = devices run, [1] = differences, [2..] = op | crane << 8 |
// p << 16 and the hardware word, up to 28 pairs. Development aid, not a judged ROM.
static void devcheck(void)
{

    u32 n = 0, diff = 0, w[61] = { 0 };
    for (int op = 0; op < QY_NUM_OPS; op++)
        for (int c = 0; c < 2; c++)
            for (int p = 0; p < qy_ops[op].setups; p++)
            {
                u32 m = qy_model[(op * 2 + c) * QY_MAX_SETUPS + p];
                if (m == 0xFFFFFFFFu)
                    continue;
                u32 h = hw_run(op, c, p);
                n++;
                if (h != m && h != QY_HW_NONE)
                {
                    if (diff < 28)
                    {
                        w[2 + diff * 2] = (u32)op | (u32)c << 8 | (u32)p << 16;
                        w[3 + diff * 2] = h;
                    }
                    diff++;
                }
            }
    w[0] = n;
    w[1] = diff;
    ledger_clear();
    for (int i = 0; i < 61; i++)
        qy_res[i] = w[i];
    ledger_publish(0x56454451u, cv_frame);      // "QDEV"
}

void demo_main(const char *name)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    if (strcmp(name, "rehearsal") == 0)
    {
        rehearsal_run();
        rehearsal_idle();
    }
    if (strcmp(name, "devcheck") == 0)
    {
        devcheck();
        for (;;)
            sys_vsync();
    }
    for (int i = 0; i < N_PIER_DEMOS; i++)
        if (strcmp(name, pier_demos[i].name) == 0)
        {
            pier_run(name);
            sd_result_save("quayholt", name);
            for (;;)
                sys_vsync();
        }
    for (;;)
        sys_vsync();
}
