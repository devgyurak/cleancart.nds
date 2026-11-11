// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "gfx/dual.h"
#include "gfx/fx.h"

#define SHOW_FRAMES 120

// --- the scripted player --------------------------------------------------------

enum { PH_BUILD, PH_WAIT, PH_DELIVER, PH_EXIT };

static int phase, tick, wait;
static u8 target[PZ_MAX_DIM][PZ_MAX_DIM];

// What each puzzle left behind for the block.
static u32 front_d, plan_c, cross_d, cross_c;
static u32 last_blocks, last_par;
static bool delivered_ok;

static bool fresh;   // the puzzle is loaded by scene_play, so the targets are read on its first frame

static void script_begin(void)
{
    fresh = true;
    phase = PH_BUILD;
    tick = wait = 0;
    delivered_ok = false;
}

// Called every frame after the real input was read: replaces it with the script.
static void script_hook(void)
{
    cv_in.pressed = cv_in.repeat = cv_in.held = cv_in.released = 0;
    cv_in.touching = false;
    const PzPart *p = play_part();
    const PzGrid *g = play_grid();
    if (fresh)
    {
        memset(target, 0, sizeof(target));
        for (int i = 0; i < p->nsol; i++)
            target[p->sol[3 * i + 1]][p->sol[3 * i]]++;
        fresh = false;
    }
    if (++tick % 3)
        return;
    switch (phase)
    {
        case PH_BUILD:
        {
            int tx = -1, ty = -1;
            for (int y = 0; y < p->d && tx < 0; y++)
                for (int x = 0; x < p->w; x++)
                    if (pz_height(g, x, y) < target[y][x])
                    {
                        tx = x;
                        ty = y;
                        break;
                    }
            if (tx < 0)
            {
                phase = PH_WAIT;
                wait = 0;
                return;
            }
            int cx, cy;
            play_cursor(&cx, &cy);
            if (cx < tx)       cv_in.repeat = KEY_RIGHT;
            else if (cx > tx)  cv_in.repeat = KEY_LEFT;
            else if (cy < ty)  cv_in.repeat = KEY_UP;
            else if (cy > ty)  cv_in.repeat = KEY_DOWN;
            else               cv_in.pressed = KEY_A;
            break;
        }
        case PH_WAIT:
            // Let a few front/plan pairs be captured, then measure and deliver.
            if (++wait > 40)
            {
                front_d = dual_probe.front_d;
                plan_c = dual_probe.plan_c;
                cross_d = dual_probe.cross_d;
                cross_c = dual_probe.cross_c;
                last_blocks = (u32)pz_blocks(g, p);
                last_par = p->par;
                if (play_matched())
                    cv_in.pressed = KEY_A;
                phase = PH_DELIVER;
                wait = 0;
            }
            break;
        case PH_DELIVER:
            if (++wait > 20)
            {
                delivered_ok = play_delivered();
                cv_in.pressed = KEY_START;   // back to the puzzle list: scene_play returns
                phase = PH_EXIT;
            }
            break;
        default:
            break;
    }
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
        script_begin();
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

void rehearsal_idle(void)
{
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
