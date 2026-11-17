// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <string.h>

#include "core/sdcard.h"
#include "core/sys.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/bench.h"
#include "judge/bench7.h"

static u32 result[PD_VALUES];

void bench_fill(u32 *res)
{
    memset(res, 0, PD_VALUES * sizeof(u32));
    BenchMath m;
    bench_math(&m);
    bool cascade = bench_timer_cascade();
    res[PDV_FLAGS] = (m.done ? 1u : 0) | (cascade ? 4u : 0);
    res[PDV_PRE] = (m.results_ok ? 1u : 0) | (cascade ? 4u : 0);
    for (int i = 0; i < MATH_N; i++)
        res[PDV_RAW + i] = m.edge[i];
    // Edges that were not found stay 0xFFFF (never a passing difference).
    if (m.done)
    {
        res[PDV_MATH_DIV_SQRT] = (u32)((s32)m.edge[MATH_DIV32] - (s32)m.edge[MATH_SQRT]);
        res[PDV_MATH_64_32] = (u32)((s32)m.edge[MATH_DIV6432] - (s32)m.edge[MATH_DIV32]);
        res[PDV_MATH_6464_6432] = (u32)((s32)m.edge[MATH_DIV6464] - (s32)m.edge[MATH_DIV6432]);
    }
    else
    {
        res[PDV_MATH_DIV_SQRT] = res[PDV_MATH_64_32] = res[PDV_MATH_6464_6432] = 0xFFFF;
    }

    // The ARM7 benches. A measurement that did not finish stays 0xFFFF (NG).
    res[PDV_SPI] = res[PDV_MEM_MAIN_SEQ] = res[PDV_MEM_MAIN_NONSEQ] = res[PDV_MEM_CODE_MAIN] = res[PDV_MEM_VRAM_SEQ] = 0xFFFF;
    u32 spi[4];
    if (bench7_spi(spi))
    {
        res[0] |= 2;
        for (int i = 0; i < 4; i++)
            res[PDV_RAW_SPI + i] = spi[i];
        if (spi[0] != BENCH_NOT_MEASURED && spi[1] != BENCH_NOT_MEASURED && spi[2] != BENCH_NOT_MEASURED && spi[3] != BENCH_NOT_MEASURED)
            res[PDV_PRE] |= 8;
        // An edge at 0 is a floor (the byte ended before the sled could look), not a measurement.
        if (spi[0] != 0 && spi[0] != BENCH_NOT_MEASURED && spi[3] != BENCH_NOT_MEASURED)
        {
            res[0] |= 8;
            res[PDV_SPI] = (u32)((s32)spi[3] - (s32)spi[0]);
        }
    }
    u32 mem[CV_BENCH_WORDS];
    if (bench7_mem(mem))
    {
        res[0] |= 16;
        for (int i = 0; i < CV_BENCH_WORDS; i++)
            res[PDV_RAW_MEM + i] = mem[i];
        res[PDV_MEM_MAIN_SEQ] = mem[0];
        res[PDV_MEM_MAIN_NONSEQ] = mem[1];
        res[PDV_MEM_CODE_MAIN] = mem[2];
        res[PDV_MEM_VRAM_SEQ] = mem[3];
    }
}

static void publish(u32 id, const u32 *v, int n)
{
    volatile CvResult *r = CV_RESULT;
    r->magic = 0;
    r->demo_id = id;
    r->frame = cv_frame;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < n ? v[i] : 0;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

void rehearsal_run(void)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    bench_fill(result);
    publish(REHEARSAL_ID, result, PD_VALUES);
}

// The result on screen, for runs on a console where the RAM block cannot be read.
static void show_result(void)
{
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "REHEARSAL RESULT");
    text_printf(&top, 1, 3, TP_WHITE, "FLAGS %03lX  FRAME %lu", (unsigned long)result[0], (unsigned long)cv_frame);
    for (int i = 3; i < 28; i++)
        text_printf(&top, 1 + ((i - 3) / 13) * 14, 5 + (i - 3) % 13, TP_GRAY, "%2d %08lX", i, (unsigned long)result[i]);
    text_center(&bottom, 5, TP_GRAY, "PHOTOGRAPH THE TOP SCREEN");
}

void rehearsal_idle(void)
{
    show_result();
    sd_result_save("pendulane", "rehearsal");   // only with cleancart/pendulane/ on the card
    for (;;)
    {
        sys_vsync();
        // Keep the frame counter of the block fresh, magic last.
        volatile CvResult *r = CV_RESULT;
        r->magic = 0;
        r->frame = cv_frame;
        DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
        r->magic = CV_RESULT_MAGIC;
        DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    }
}
