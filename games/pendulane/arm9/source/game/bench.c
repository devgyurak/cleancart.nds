// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// The bench ledger: runs the math bench live and prints the edges next to the book values (GBATEK, in
// ARM9 cycles = half bus cycles). This is a look at the measurement, not a game; the verdict is judge.py's.

#include <nds.h>

#include "core/input.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/bench.h"

PdScene scene_bench(void)
{
    CvText top, bot;
    scene_text_screens(&top, &bot);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "BENCH LEDGER");
    text_center(&top, 3, TP_GRAY, "EDGE = ARM9 CYCLES TO BUSY CLEAR");
    scene_frame_end();

    BenchMath m;
    bench_math(&m);
    static const char *const names[MATH_N] = { "DIV 32/32", "DIV 64/32", "DIV 64/64", "SQRT" };
    static const int book[MATH_N] = { 36, 68, 68, 26 };
    text_printf(&top, 1, 5, TP_GRAY, "UNIT        EDGE  BOOK");
    for (int i = 0; i < MATH_N; i++)
    {
        if (m.edge[i] == BENCH_NOT_MEASURED)
            text_printf(&top, 1, 7 + i, TP_RED, "%-10s  ---   %3d", names[i], book[i]);
        else
            text_printf(&top, 1, 7 + i, TP_WHITE, "%-10s  %3lu   %3d", names[i], (unsigned long)m.edge[i], book[i]);
    }
    if (m.done)
    {
        text_printf(&top, 1, 13, TP_WHITE, "DIV32-SQRT   %4ld  (10)", (long)((s32)m.edge[MATH_DIV32] - (s32)m.edge[MATH_SQRT]));
        text_printf(&top, 1, 14, TP_WHITE, "DIV64-DIV32  %4ld  (32)", (long)((s32)m.edge[MATH_DIV6432] - (s32)m.edge[MATH_DIV32]));
        text_printf(&top, 1, 15, TP_WHITE, "6464-6432    %4ld  ( 0)", (long)((s32)m.edge[MATH_DIV6464] - (s32)m.edge[MATH_DIV6432]));
    }
    text_printf(&top, 1, 17, m.results_ok ? TP_WHITE : TP_RED, "MATH RESULTS %s", m.results_ok ? "RIGHT" : "WRONG");
    text_center(&bot, 10, TP_GRAY, "B: BACK");
    for (;;)
    {
        scene_frame_end();
        if (cv_in.pressed & KEY_B)
            return SC_TITLE;
    }
}
