// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 1, the trestle: the PSG and PCM output stage at its edges (GBATEK "DS Sound Notes", "DS Sound
// Channels"): duty 7, the divider, volume register 127.
//
// A line's `hw` runs the experiment on the real sound block and returns a coarse class (so that a
// correct machine gives exactly the model's number); `model` is the same class from the GBATEK rules.

#include <nds.h>

#include "game/ear.h"
#include "game/lab.h"
#include "game/rooms.h"
#include "game/scope.h"

// Rounds a mean or swing to 256ths of full scale; floors (arithmetic shift) so +-0.5 steps stay stable.
static u32 q(s32 v)
{
    return (u32)((v + 128) >> 8);
}

// ---------------------------------------------------------------------------
// Line 1, the floor line: duty 7 is 0.0 % HIGH, a wave that never leaves LOW.

static const char *const duty_l[] = { "DUTY 2", "DUTY 6", "DUTY 7" };
static const int duty_v[] = { 2, 6, 7 };
static const char *const dash_l[] = { "-" };

static u32 duty_class(s32 mean)
{
    return mean < -16384 ? 0 : mean > 16384 ? 2 : 1;   // floor / mid / ceiling
}

static u32 p1_hw(int a, int b)
{
    EarStats st;
    return ear_psg(duty_v[a], 0, &st) ? duty_class(st.mean) : EAR_FAIL;
}

static u32 p1_model(int a, int b)
{
    int d = duty_v[a];
    int high = d == 7 ? 0 : d + 1;           // eighths HIGH: (N+1) * 12.5 %, duty 7 is 0.0 %
    s32 mean = (high * 2 - 8) * 32767 / 8;
    return duty_class(mean);
}

// ---------------------------------------------------------------------------
// Line 2, the sixteenth: volume divider 3 divides by 16 (not by 8).

static const char *const damper_l[] = { "DAMPER 0", "DAMPER 1", "DAMPER 2", "DAMPER 3" };

static u32 p2_hw(int a, int b)
{
    EarStats st;
    return ear_psg(3, a, &st) ? q(st.p2p) : EAR_FAIL;
}

static u32 p2_model(int a, int b)
{
    static const int shift[4] = { 0, 1, 2, 4 };   // divider 0..3 = /1 /2 /4 /16
    return q(2 * (32767 >> shift[a]));
}

// ---------------------------------------------------------------------------
// Line 3, the full dial: volume register 127 counts as 128, so it is full scale.

static const char *const vol_l[] = { "126", "127", "64" };
static const int vol_v[] = { 126, 127, 64 };

static u32 p3_hw(int a, int b)
{
    EarStats st;
    return ear_dc(0, vol_v[a], 0, &st) ? q(st.mean) : EAR_FAIL;
}

static u32 p3_model(int a, int b)
{
    int n = vol_v[a] == 127 ? 128 : vol_v[a];
    return q(0x4000 * n / 128);
}

static void p1_wave(int a, int b, s16 *out)
{
    int d = duty_v[a];
    scope_square(out, d == 7 ? 0 : d + 1, 32767);
}

static void p2_wave(int a, int b, s16 *out)
{
    static const int shift[4] = { 0, 1, 2, 4 };
    scope_square(out, 4, 32767 >> shift[a]);
}

static void p3_wave(int a, int b, s16 *out)
{
    scope_dc(out, 0x4000 * (vol_v[a] == 127 ? 128 : vol_v[a]) / 128);
}

static const Puzzle trestle_puzzles[] = {
    { "THE FLOOR LINE", "FLAT AT THE FLOOR", "LEVER", duty_l, 3, "-", dash_l, 1, p1_hw, p1_model, 0, p1_wave },
    { "THE SIXTEENTH", "ONE SIXTEENTH OF FULL", "DAMPER", damper_l, 4, "-", dash_l, 1, p2_hw, p2_model, 16, p2_wave },
    { "THE FULL DIAL", "FULL SWING OF THE TAPE", "DIAL", vol_l, 3, "-", dash_l, 1, p3_hw, p3_model, 64, p3_wave },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_trestle(void)
{
    EarStats st, st3;
    if (ear_psg(3, 0, &st))
    {
        tw_res[TV_DUTY3_EDGES] = (u32)st.edges;
        tw_res[TV_P2P0] = (u32)st.p2p;
    }
    if (ear_psg(7, 0, &st))
        tw_res[TV_DUTY7] = (u32)(st.mean + 32768);
    if (ear_psg(3, 3, &st3) && tw_res[TV_P2P0] != TV_UNSET && st3.p2p > 0)
    {
        tw_res[TV_P2P3] = (u32)st3.p2p;
        tw_res[TV_DIV_RATIO] = tw_res[TV_P2P0] * 10 / (u32)st3.p2p;
    }
    EarStats v127, v64;
    if (ear_dc(0, 127, 0, &v127) && ear_dc(0, 64, 0, &v64))
    {
        tw_res[TV_VOL127] = (u32)v127.mean;
        tw_res[TV_VOL64] = (u32)v64.mean;
        if (v64.mean > 0)
            tw_res[TV_VOL_RATIO] = (u32)v127.mean * 1000 / (u32)v64.mean;
    }
}

const Room room_trestle = {
    "THE TRESTLE", "UPPER TOWER", "DS SOUND NOTES", trestle_puzzles,
    (int)(sizeof(trestle_puzzles) / sizeof(trestle_puzzles[0])), audit_trestle,
};
