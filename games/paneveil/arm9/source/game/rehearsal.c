// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/sdcard.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/rehearsal.h"
#include "judge/bench.h"
#include "judge/lightbox.h"
#include "game/scenes.h"
#include "game/script.h"
#include "game/window.h"
#include "core/input.h"
#include "gfx/fx.h"
#include "gfx/text.h"

#define NVALUES 32

static u32 result[NVALUES];

// The scripted player: replaces the input after the real input was read (like the QA pilot).
static u16 prev_keys;
static void script_hook(void)
{
    u16 keys;
    script_step(&keys, false);
    cv_in.held = keys;
    cv_in.pressed = keys & ~prev_keys;
    cv_in.repeat = cv_in.pressed;
    cv_in.released = prev_keys & ~keys;
    cv_in.touching = false;
    prev_keys = keys;
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
    memset(result, 0, sizeof(result));

    // The kiln check (judge/bench.c): values[3..15].
    u32 kiln[KILN_ITEMS];
    kiln_run(kiln);
    for (int i = 0; i < KILN_ITEMS; i++)
        result[3 + i] = kiln[i];
    result[0] |= 1u | (kiln_ok(kiln) ? 2u : 0);

    // The first window of every chapter that has windows, through the live play scene.
    pv_play_hook = script_hook;
    u32 played = 0;
    for (int ch = 1; ch <= PV_CHAPTERS; ch++)
    {
        if (win_chapter_size(ch) == 0)
            continue;
        pv_chapter = ch;
        pv_index = 1;
        pv_view3d = true;
        scene_play();
        played |= 1u << (ch - 1);
    }
    pv_play_hook = NULL;
    result[1] = (u32)win_baked_count();
    result[2] = played;

    // The light box (judge/lightbox.c): values[16..30], capture bits in flags 8..12.
    LightboxResult lb;
    lightbox_run(&lb);
    for (int i = 0; i < LB_ITEMS; i++)
        result[16 + i] = lb.v[i];
    result[0] |= lb.done << 8;
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
    for (int i = 3; i < 31; i++)
        text_printf(&top, 1 + ((i - 3) / 14) * 14, 5 + (i - 3) % 14, TP_GRAY, "%2d %08lX", i, (unsigned long)result[i]);
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
