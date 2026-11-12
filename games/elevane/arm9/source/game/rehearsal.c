// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>
#include <stdio.h>
#include <string.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "game/script.h"
#include "gfx/dual.h"
#include "gfx/fx.h"
#include "gfx/text.h"

// CRC-16 (poly 0xA001 reflected, init 0xFFFF), the same as the BIOS GetCRC16. Computed here
// so that an emulator's BIOS replacement cannot be mistaken for a misread digit.
static u16 crc16(const void *data, u32 len)
{
    const u8 *p = data;
    u16 crc = 0xFFFF;
    while (len--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++)
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
    }
    return crc;
}

#define SHOW_FRAMES 120

// --- the scripted player --------------------------------------------------------

// What each puzzle left behind for the block.
static u32 front_d, plan_c, cross_d, cross_c;
static u32 last_blocks, last_par;
static bool delivered_ok;
static ScrPhase last_phase;
static u16 prev_keys;

// Called every frame after the real input was read: replaces it with the script
// (game/script.c plays the stored solution with the keys a person uses).
static void script_hook(void)
{
    u16 keys;
    ScrPhase ph = script_step(&keys, false);
    cv_in.held = keys;
    cv_in.pressed = keys & ~prev_keys;
    cv_in.repeat = cv_in.pressed;
    cv_in.released = prev_keys & ~keys;
    cv_in.touching = false;
    prev_keys = keys;
    if (ph == SCR_DELIVER && last_phase == SCR_WAIT)
    {
        // The pile is right and a few front/plan pairs were captured: measure.
        front_d = dual_probe.front_d;
        plan_c = dual_probe.plan_c;
        cross_d = dual_probe.cross_d;
        cross_c = dual_probe.cross_c;
        last_blocks = (u32)play_blocks();
        last_par = (u32)play_par();
    }
    if (ph == SCR_DONE)
        delivered_ok = play_delivered();
    last_phase = ph;
}

// --- the run ----------------------------------------------------------------------

static u32 flags, played_mask, delivered_count;
static bool front_ok = true, plan_ok = true;

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

static u32 result[24];

void rehearsal_run(void)
{
    memset((void *)CV_RESULT_ADDR, 0, sizeof(CvResult));
    dual_probe_enable(true);
    ev_play_hook = script_hook;
    bool all_delivered = true;
    for (int ch = 1; ch <= 5; ch++)
    {
        if (pz_chapter_size(ch) == 0)
            continue;
        ev_chapter = ch;
        ev_index = 1;
        ev_view3d = true;
        last_phase = SCR_BUILD;
        delivered_ok = false;
        scene_play();
        played_mask |= 1u << (ch - 1);
        delivered_count += delivered_ok;
        all_delivered &= delivered_ok;
        front_ok &= front_d > 0 && cross_d == 0;
        plan_ok &= plan_c > 0 && cross_c == 0;
    }
    ev_play_hook = NULL;
    dual_probe_enable(false);

    ShowStats a, b;
    showcase_run(SHOW_HDMA, SHOW_FRAMES, &a);
    showcase_run(SHOW_REAR, SHOW_FRAMES, &b);

    bool counters = a.polys[0] + a.polys[1] > 0;
    bool under = a.polys[0] <= 2048 && a.polys[1] <= 2048 && a.polys[0] + a.polys[1] > 2048;
    bool no_over = !a.over[0] && !a.over[1] && !b.over[0] && !b.over[1];
    flags = all_delivered | front_ok << 1 | plan_ok << 2 | (counters ? under : 0) << 3
          | no_over << 4 | (a.holes_p == 0 && a.holes_q == 0) << 5
          | (a.left > 0 && a.right > 0) << 6 | (b.left > 0) << 7 | counters << 8;
    result[0] = flags;
    result[1] = 1;
    result[2] = front_d;
    result[3] = plan_c;
    result[4] = cross_d << 16 | cross_c;
    result[5] = played_mask;
    result[6] = delivered_count;
    result[7] = (u32)a.polys[0] | (u32)a.polys[1] << 16;
    result[8] = (u32)a.verts[0] | (u32)a.verts[1] << 16;
    result[9] = a.over[0] | a.over[1] << 1 | b.over[0] << 2 | b.over[1] << 3;
    result[10] = a.holes_p;
    result[11] = a.holes_q;
    result[12] = a.left;
    result[13] = a.right;
    result[14] = a.hash;
    result[15] = b.left;
    result[16] = (u32)b.polys[0] | (u32)b.polys[1] << 16;
    result[17] = last_blocks << 8 | last_par;
    publish(REHEARSAL_ID, result, 18);
}

// The result on screen, for runs on a console where the RAM block cannot be read: the
// values as hex, to be photographed and graded with `judge.py --values`.
static void show_result(void)
{
    CvText top, bottom;
    scene_text_screens(&top, &bottom);
    fx_bright(FX_BOTH, 0);
    text_center(&top, 1, TP_YELLOW, "REHEARSAL RESULT");
    text_printf(&top, 1, 3, TP_WHITE, "FLAGS %03lX  FRAME %lu", (unsigned long)result[0], (unsigned long)cv_frame);
    for (int i = 1; i < 18; i += 2)
    {
        int row = 5 + i / 2;
        if (i + 1 < 18)
            text_printf(&top, 1, row, TP_GRAY, "%2d %08lX  %2d %08lX", i, (unsigned long)result[i],
                        i + 1, (unsigned long)result[i + 1]);
        else
            text_printf(&top, 1, row, TP_GRAY, "%2d %08lX", i, (unsigned long)result[i]);
    }
    // A CRC over the 18 values (little endian bytes), so a misread digit shows.
    text_printf(&top, 1, 15, TP_WHITE, "CRC %04X", (unsigned)crc16(result, 18 * 4));
    text_center(&bottom, 3, TP_GRAY, "VALUES 0..17 IN HEX + CRC");
    text_center(&bottom, 5, TP_GRAY, "PHOTOGRAPH THE TOP SCREEN");
    text_center(&bottom, 7, TP_GRAY, "tools/judge.py --values ...");
}

void rehearsal_idle(void)
{
    show_result();
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
