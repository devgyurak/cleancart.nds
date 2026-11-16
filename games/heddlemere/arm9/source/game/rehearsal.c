// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "core/sdcard.h"
#include "core/sys.h"
#include "game/order.h"
#include "game/play.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "game/script.h"
#include "judge/timing.h"
#include "gfx/fx.h"
#include "gfx/text.h"

static u32 result[RH_VALUES];

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

u32 rehearsal_flags(void)
{
    return sys_ipcsync_seen() == CV_SYNC_ARM7_READY ? RH_FLAG_ARM7 : 0;
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

void rehearsal_measure(u32 *r)
{
    u32 tv[TM_ITEMS];
    u32 a7 = sys_arm7_hblank_low();   // first: it needs the frame loop (IME on)
    bool done = timing_run(tv);
    for (int i = 1; i < TM_ITEMS; i++)
        r[i] = tv[i];
    r[TM_HBLANK7] = a7;
    r[RH_FLAGS] = rehearsal_flags() | (done ? RH_FLAG_DONE : 0) | (a7 != TM_NOT_MEASURED ? RH_FLAG_ARM7_SPAN : 0);
    r[TM_LYC] = tv[TM_LYC];
}

void rehearsal_run(void)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    memset(result, 0, sizeof(result));
    rehearsal_measure(result);

    // The first order of every chapter, through the live play scene.
    hm_play_hook = script_hook;
    u32 chapters = 0;
    for (int ch = 1; ch <= HM_CHAPTERS; ch++)
    {
        if (order_chapter_size(ch) == 0)
            continue;
        hm_chapter = ch;
        hm_index = 1;
        scene_play();
        chapters |= (u32)order_was_done(ch, 1) << (ch - 1);
    }
    hm_play_hook = NULL;
    result[RH_ORDERS] = (u32)order_done_count();
    result[RH_CHAPTERS] = chapters;
    result[RH_PLAN_HASH] = order_plan_hash();
    publish(REHEARSAL_ID, result, RH_VALUES);
}

// The result on screen, for runs on a console where the RAM block cannot be read.
static void show_result(void)
{
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "REHEARSAL RESULT");
    text_printf(&top, 1, 3, TP_WHITE, "FLAGS %03lX  FRAME %lu", (unsigned long)result[0], (unsigned long)cv_frame);
    for (int i = 0; i < RH_VALUES; i++)
        text_printf(&top, 1 + (i / 16) * 14, 5 + i % 16, TP_GRAY, "%2d %08lX", i, (unsigned long)result[i]);
    text_center(&bottom, 5, TP_GRAY, "PHOTOGRAPH THE TOP SCREEN");
}

void rehearsal_idle(void)
{
    show_result();
    sd_result_save("heddlemere", "rehearsal");   // only with cleancart/heddlemere/ on the card
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
