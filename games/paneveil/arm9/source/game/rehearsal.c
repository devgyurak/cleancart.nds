// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/sdcard.h"
#include "core/sys.h"
#include "game/rehearsal.h"
#include "judge/bench.h"
#include "game/scenes.h"
#include "gfx/fx.h"
#include "gfx/text.h"

#define NVALUES 32

static u32 result[NVALUES];

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
    memset(result, 0, sizeof(result));

    // The kiln check (judge/bench.c): values[3..15].
    u32 kiln[KILN_ITEMS];
    kiln_run(kiln);
    for (int i = 0; i < KILN_ITEMS; i++)
        result[3 + i] = kiln[i];
    result[0] |= 1u | (kiln_ok(kiln) ? 2u : 0);
    publish(REHEARSAL_ID, result, NVALUES);
}

// The result on screen, for runs on a console where the RAM block cannot be read.
static void show_result(void)
{
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "REHEARSAL RESULT");
    text_printf(&top, 1, 3, TP_WHITE, "FLAGS %03lX  FRAME %lu", (unsigned long)result[0], (unsigned long)cv_frame);
    for (int i = 0; i < KILN_ITEMS; i++)
        text_printf(&top, 1 + (i / 7) * 14, 5 + i % 7, TP_GRAY, "%2d %08lX", 3 + i, (unsigned long)result[3 + i]);
    text_center(&bottom, 5, TP_GRAY, "PHOTOGRAPH THE TOP SCREEN");
}

void rehearsal_idle(void)
{
    show_result();
    sd_result_save("paneveil", "rehearsal");   // only with cleancart/paneveil/ on the card
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
