// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 2, the splice: channel edges (GBATEK "DS Sound Channels", "DS Sound Notes"): a tape shorter than
// four words, the hold latch, the noise shift register.

#include <nds.h>
#include <stdlib.h>

#include "game/ear.h"
#include "game/lab.h"
#include "game/rooms.h"
#include "game/scope.h"

static u32 q(s32 v)
{
    return (u32)((v + 128) >> 8);
}

static const char *const dash_l[] = { "-" };

// ---------------------------------------------------------------------------
// Line 4, the short tape: PNT+LEN below 4 words keeps the busy flag up and makes no sound.
// Result: 2 = busy and silent, 3 = busy and audible.

static const char *const words_l[] = { "2 WORDS", "3 WORDS", "4 WORDS", "8 WORDS" };
static const int words_v[] = { 2, 3, 4, 8 };

static u32 p4_hw(int a, int b)
{
    EarStats st;
    int busy;
    if (!ear_splice(words_v[a], &busy, &st))
        return EAR_FAIL;
    return (u32)(busy << 1 | (st.peak > 64 ? 1 : 0));
}

static u32 p4_model(int a, int b)
{
    return words_v[a] < 4 ? 2u : 1u;   // below 4 words: busy for good and silent; else it plays and ends
}

// ---------------------------------------------------------------------------
// Line 5, the latch: a one-shot with the hold bit keeps its last sample.

static const char *const latch_l[] = { "OFF", "ON" };

static u32 p5_hw(int a, int b)
{
    EarStats st;
    return ear_hold(a == 1, &st) ? q(st.mean) : EAR_FAIL;
}

static u32 p5_model(int a, int b)
{
    return a == 1 ? q(0x4000) : 0u;
}

// ---------------------------------------------------------------------------
// Line 6, the noise cipher: the XOR mask of the shift register (GBATEK: 6000h). Result 1 = the hardware's
// sequence follows that mask for 200 bits or more.

static const char *const mask_l[] = { "XOR 4000", "XOR 6000", "XOR C000" };
static const u32 mask_v[] = { 0x4000, 0x6000, 0xC000 };

static u32 p6_hw(int a, int b)
{
    int run, phase;
    if (!ear_noise(mask_v[a], &run, &phase))
        return EAR_FAIL;
    return run >= 200 ? 1u : 0u;
}

static u32 p6_model(int a, int b)
{
    return mask_v[a] == 0x6000 ? 1u : 0u;
}

static void p4_wave(int a, int b, s16 *out)
{
    scope_dc(out, words_v[a] < 4 ? 0 : 0x4000);
}

static void p5_wave(int a, int b, s16 *out)
{
    scope_dc(out, a == 1 ? 0x4000 : 0);
}

static void p6_wave(int a, int b, s16 *out)
{
    scope_noise(out, mask_v[a]);
}

static const Puzzle splice_puzzles[] = {
    { "THE SHORT TAPE", "BUSY BUT SILENT", "TAPE", words_l, 4, "-", dash_l, 1, p4_hw, p4_model, 2, p4_wave },
    { "THE LATCH", "TAIL HELD AT LEVEL", "LATCH", latch_l, 2, "-", dash_l, 1, p5_hw, p5_model, 64, p5_wave },
    { "THE NOISE CIPHER", "SEQUENCE ACCEPTED", "MASK", mask_l, 3, "-", dash_l, 1, p6_hw, p6_model, 1, p6_wave },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_splice(void)
{
    EarStats st, loop, on, off;
    int busy;
    if (ear_splice(3, &busy, &st))
    {
        tw_res[TV_SHORT_BUSY] = (u32)busy;
        tw_res[TV_SHORT_PEAK] = (u32)st.peak;
    }
    if (ear_dc(0, 127, 0, &loop) && ear_hold(true, &on) && ear_hold(false, &off) && loop.mean > 0)
    {
        tw_res[TV_LOOP_LEVEL] = (u32)loop.mean;
        tw_res[TV_HELD_LEVEL] = (u32)on.mean;
        tw_res[TV_HOLD_ON] = (u32)(abs(on.mean) * 100 / loop.mean);
        tw_res[TV_HOLD_OFF] = (u32)(abs(off.mean) * 100 / loop.mean);
    }
    int run, phase;
    if (ear_noise(0x6000, &run, &phase))
    {
        tw_res[TV_NOISE_RUN] = (u32)run;
        tw_res[TV_NOISE_PHASE] = (u32)phase;
    }
}

const Room room_splice = {
    "THE SPLICE", "UPPER TOWER", "SOUND CHANNELS", splice_puzzles,
    (int)(sizeof(splice_puzzles) / sizeof(splice_puzzles[0])), audit_splice,
};
