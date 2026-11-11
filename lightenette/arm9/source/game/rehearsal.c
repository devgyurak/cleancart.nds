// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/act1.h"
#include "game/rehearsal.h"
#include "gfx/oam.h"
#include "judge/judge.h"

// One drag per slider (columns at x = 32, 96, 160, 224); the touch y gives the
// target pose's angles 50, 35 (36 after rounding), 25 and -20.
static const Act1Step act1_script[] = {
    { 10, 32, 51 },  { 14, 0, 0 },
    { 20, 96, 64 },  { 24, 0, 0 },
    { 30, 160, 73 }, { 34, 0, 0 },
    { 40, 224, 114 }, { 44, 0, 0 },
};

#define ACT1_END 240

static void tick(void)
{
    oam_commit();
    sys_vsync();
    sys_handle_lid();
    input_update();
}

void rehearsal_run(void)
{
    Act1Outcome o = { 0, false, -1 };
    act1_replay(act1_script, sizeof(act1_script) / sizeof(act1_script[0]), ACT1_END, &o);

    judge_run(NULL);

    char line[33];
    snprintf(line, sizeof(line), "ACT1 %3d %s F%d", o.score, o.cleared ? "OK" : "NG", o.clear_frame);
    judge_text(21, o.cleared ? TP_GREEN : TP_RED, line);

    volatile CvResult *r = CV_RESULT;
    r->demo_id = REHEARSAL_ID;
    r->frame = cv_frame;
    r->values[9] = (u32)o.score;
    r->values[10] = o.cleared;
    r->values[11] = (u32)o.clear_frame;
    r->values[12] = 1;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

void rehearsal_idle(void)
{
    for (;;)
    {
        judge_frame();
        tick();
    }
}
