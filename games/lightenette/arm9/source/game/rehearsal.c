// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>

#include "core/input.h"
#include "core/sys.h"
#include "game/act1.h"
#include "game/act2.h"
#include "game/act3.h"
#include "game/act4.h"
#include "game/act5.h"
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

// Act 2 keys: choose doll B and turn it, choose doll C and turn it. The
// third doll's back is the plain mirror, so the seam shows on hardware.
static const Act2Step act2_script[] = {
    { 10, KEY_RIGHT }, { 20, KEY_R },
    { 45, KEY_RIGHT }, { 55, KEY_L },
};

#define ACT2_END 240

// Act 3 keys: the lantern starts at (128, 96) and moves 2 px per frame. Visit
// mark 2 (112,128), mark 1 (48,48) and mark 3 (200,56), 15+ frames over each.
static const Act3Step act3_script[] = {
    { 0, 8, KEY_LEFT | KEY_DOWN },   { 8, 8, KEY_DOWN },
    { 40, 32, KEY_LEFT | KEY_UP },   { 72, 8, KEY_UP },
    { 100, 4, KEY_RIGHT | KEY_DOWN }, { 104, 72, KEY_RIGHT },
};

#define ACT3_END 400

// Act 4 keys: the strings start on pegs 1,3,0,2 (0-based). Swap pegs 0 and 1,
// then 1 and 3, then 2 and 3; the cursor starts on peg 0.
static const Act4Step act4_script[] = {
    { 10, KEY_A }, { 14, KEY_RIGHT }, { 18, KEY_A },
    { 28, KEY_A }, { 32, KEY_RIGHT }, { 36, KEY_RIGHT }, { 40, KEY_A },
    { 50, KEY_A }, { 54, KEY_LEFT }, { 58, KEY_A },
};
#define ACT4_END 240

// Act 5 keys: the lantern starts over doll B (centre 128,96) and moves 2 px per
// frame. Lift it onto the dolls' row, light B in place, go left to doll A, turn
// it to its back, go right to doll C, turn it to its back (the stitched doll).
static const Act5Step act5_script[] = {
    { 0, 8, KEY_UP },
    { 20, 36, KEY_LEFT },
    { 50, 1, KEY_R },
    { 75, 72, KEY_RIGHT },
    { 140, 1, KEY_R },
};
#define ACT5_END 400

#define SCRIPT_GETTER(N)                                                       \
    const Act##N##Step *rehearsal_act##N##_script(int *n, int *end)           \
    {                                                                          \
        *n = (int)(sizeof(act##N##_script) / sizeof(act##N##_script[0]));      \
        *end = ACT##N##_END;                                                   \
        return act##N##_script;                                                \
    }
SCRIPT_GETTER(1)
SCRIPT_GETTER(2)
SCRIPT_GETTER(3)
SCRIPT_GETTER(4)
SCRIPT_GETTER(5)

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

    Act2Outcome o2 = { false, -1, false, 0, 0 };
    act2_replay(act2_script, sizeof(act2_script) / sizeof(act2_script[0]), ACT2_END, &o2);

    Act3Outcome o3 = { false, -1, false, 0, { 0, 0, 0 }, 0, 0 };
    act3_replay(act3_script, sizeof(act3_script) / sizeof(act3_script[0]), ACT3_END, &o3);

    Act4Outcome o4 = { false, -1, false, 0, 0 };
    act4_replay(act4_script, sizeof(act4_script) / sizeof(act4_script[0]), ACT4_END, &o4);
    Act5Outcome o5 = { false, -1, false, 0, { 0, 0, 0 }, 0, { 0, 0, 0 }, 0, 0 };
    act5_replay(act5_script, sizeof(act5_script) / sizeof(act5_script[0]), ACT5_END, &o5);

    // The per-act blocks above carry magic; withdraw it until the combined block is
    // complete (it is written last).
    volatile CvResult *r = CV_RESULT;
    r->magic = 0;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    JudgeResult jr;
    judge_run(&jr, false);

    // One summary row above the scene (rows 0-5 are free across the screen;
    // below them the scene occupies x >= 128).
    char line[33];
    bool all = o.cleared && o2.cleared && o3.cleared && o4.cleared && o5.cleared;
    snprintf(line, sizeof(line), "A1 %s A2 %s A3 %s A4 %s A5 %s", o.cleared ? "OK" : "NG",
             o2.cleared ? "OK" : "NG", o3.cleared ? "OK" : "NG", o4.cleared ? "OK" : "NG",
             o5.cleared ? "OK" : "NG");
    judge_text(5, all ? TP_GREEN : TP_RED, line);

    r->demo_id = REHEARSAL_ID;
    r->frame = cv_frame;
    r->values[0] = (u32)jr.seam[0] | (u32)jr.seam[1] << 8 | (u32)jr.seam[2] << 16;
    r->values[1] = jr.edges;
    r->values[2] = jr.lines;
    r->values[3] = jr.flags;
    r->values[4] = jr.lamp[0];
    r->values[5] = jr.lamp[1];
    r->values[6] = jr.lamp[2];
    r->values[7] = jr.out_none;
    r->values[8] = jr.out_dark;
    r->values[9] = (u32)o.score;
    r->values[10] = o.cleared;
    r->values[11] = (u32)o.clear_frame;
    r->values[12] = 1;
    r->values[13] = o2.cleared;
    r->values[14] = o2.seam;
    r->values[15] = o2.edges;
    r->values[16] = (u32)o2.clear_frame;
    r->values[17] = o2.measured;
    r->values[18] = o3.cleared;
    r->values[19] = (u32)o3.found;
    r->values[20] = o3.lamp[0];
    r->values[21] = o3.lamp[1];
    r->values[22] = o3.lamp[2];
    r->values[23] = o3.out_none;
    r->values[24] = o3.out_dark;
    r->values[25] = o3.measured;
    r->values[26] = (u32)o3.clear_frame;
    r->values[27] = o4.cleared;
    r->values[28] = (u32)o4.clear_frame;
    r->values[29] = o4.hline;
    r->values[30] = o4.dline;
    r->values[31] = o4.measured;
    r->values[32] = o5.cleared;
    r->values[33] = (u32)o5.clear_frame;
    r->values[34] = o5.seam;
    r->values[35] = o5.edge[0];
    r->values[36] = o5.edge[1];
    r->values[37] = o5.edge[2];
    r->values[38] = o5.lines;
    r->values[39] = o5.lamp[0];
    r->values[40] = o5.lamp[1];
    r->values[41] = o5.lamp[2];
    r->values[42] = o5.out_none;
    r->values[43] = o5.out_dark;
    r->values[44] = o5.measured;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last: a reader never sees a half-filled block
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
