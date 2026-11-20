// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/sdcard.h"
#include "core/qasuite.h"
#include "core/sys.h"
#include "game/rehearsal.h"
#include "core/input.h"
#include "game/model.h"
#include "game/scenes.h"
#include "game/voyage.h"
#include "gfx/fx.h"
#include "gfx/text.h"
#include "judge/lightcheck.h"

static u32 result[W_WORDS + 4];
static const char *shown_name = "rehearsal";

static void publish(u32 id)
{
    volatile CvResult *r = CV_RESULT;
    r->magic = 0;
    r->demo_id = id;
    r->frame = cv_frame;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < (int)(sizeof(result) / sizeof(*result)) ? result[i] : 0;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

// The scripted player: replaces the input after the real input was read (like the QA pilot).
static u16 prev_keys;
static u32 hook_frame;
static void script_hook(void)
{
    u16 keys = by_pilot_keys(hook_frame++);
    cv_in.held = keys;
    cv_in.pressed = keys & ~prev_keys;
    cv_in.repeat = cv_in.pressed;
    cv_in.released = prev_keys & ~keys;
    cv_in.touching = false;
    prev_keys = keys;
}

void rehearsal_run(void)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    memset(result, 0, sizeof(result));
    // The first approach of every chapter, through the live voyage scene.
    by_play_hook = script_hook;
    by_cleared_bits = 0;
    for (int ch = 1; ch <= BY_CHAPTERS; ch++)
    {
        by_chapter = ch;
        by_approach = 0;
        scene_voyage();
    }
    by_play_hook = NULL;
    lightcheck_all(result);
    result[19] = (u32)by_cleared_count();
    result[20] = by_plan_hash();
    publish(REHEARSAL_ID);
}

// "ck-<scene>" packed little-endian, the id the judge matches.
static u32 check_id(int scene)
{
    const char *n = lightcheck_scene_name(scene);
    char t[5] = { 'c', 'k', n[0], n[1], 0 };
    return (u32)t[0] | (u32)t[1] << 8 | (u32)t[2] << 16 | (u32)t[3] << 24;
}

void check_run(int scene)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    memset(result, 0, sizeof(result));
    shown_name = lightcheck_scene_name(scene);
    lightcheck_scene(scene, result);
    publish(check_id(scene));
}

// The result on screen, for runs on a console where the RAM block cannot be read.
static void show_result(const char *title)
{
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, title);
    text_printf(&top, 1, 3, TP_WHITE, "CAPS %02lX  FRAME %lu", (unsigned long)result[0], (unsigned long)cv_frame);
    for (int i = 1; i < W_WORDS; i++)
        text_printf(&top, 1 + ((i - 1) / 10) * 14, 5 + (i - 1) % 10, TP_GRAY, "%2d %08lX", i, (unsigned long)result[i]);
    text_center(&bottom, 5, TP_GRAY, "PHOTOGRAPH THE TOP SCREEN");
}

static void idle_block(const char *kit_name)
{
    for (;;)
    {
        sys_vsync();
        volatile CvResult *r = CV_RESULT;
        r->magic = 0;
        r->frame = cv_frame;
        DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
        r->magic = CV_RESULT_MAGIC;
        DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
        (void)kit_name;
    }
}

void rehearsal_idle(void)
{
    show_result("REHEARSAL RESULT");
    sd_result_save("buoyane", "rehearsal");   // only with cleancart/buoyane/ on the card
    idle_block("rehearsal");
}

void check_idle(void)
{
    show_result("CHECK RESULT");
    char name[16];
    snprintf(name, sizeof(name), "check-%s", shown_name);
    sd_result_save("buoyane", name);
    idle_block(name);
}
