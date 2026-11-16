// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// District 3, the echo room: the ADPCM loop and the capture units (GBATEK "DS Sound Channels", "DS Sound
// Capture"): the loop reloads its decoder state, a capture length of 0 is one word, the channel source.

#include <nds.h>

#include "game/ear.h"
#include "game/lab.h"
#include "game/rooms.h"

static u32 q(s32 v)
{
    return (u32)((v + 128) >> 8);
}

static const char *const dash_l[] = { "-" };

// ---------------------------------------------------------------------------
// Line 7, the echo memory: an ADPCM loop started in the data part repeats exactly (the decoder state is
// reloaded with the loop). Result 0 = no drift from one pass to the next.

static const char *const loop_l[] = { "WORD 1", "WORD 9", "WORD 17" };
static const int loop_v[] = { 1, 9, 17 };

static u32 p7_hw(int a, int b)
{
    int drift;
    if (!ear_adpcm(loop_v[a], &drift))
        return EAR_FAIL;
    return drift <= 30 ? 0u : 1u;
}

static u32 p7_model(int a, int b)
{
    return 0;
}

// ---------------------------------------------------------------------------
// Line 8, the one-word tape: a capture length of 0 is read as 1 word. Result: words written.

static const char *const len_l[] = { "LENGTH 0", "LENGTH 1", "LENGTH 2" };

static u32 p8_hw(int a, int b)
{
    int n;
    return ear_cap_len(a, &n) ? (u32)n : EAR_FAIL;
}

static u32 p8_model(int a, int b)
{
    return a < 1 ? 1u : (u32)a;
}

// ---------------------------------------------------------------------------
// Line 9, the channel tap: capture 0 takes the mixer (bit 1 clear) or channel 0 alone (bit 1 set). Channel 0
// plays +2000h; channel 1 is silent or plays +2000h too. Result: the capture, in 256ths.

static const char *const src_l[] = { "MIXER", "CHANNEL 0" };
static const char *const other_l[] = { "SILENT", "PLAYS 2000" };

static u32 p9_hw(int a, int b)
{
    s32 mean;
    return ear_cap_chan(a == 1, 1, b == 1 ? 1 : -1, false, &mean) ? q(mean) : EAR_FAIL;
}

static u32 p9_model(int a, int b)
{
    s32 mixer = 0x2000 + (b == 1 ? 0x2000 : 0);
    return q(a == 1 ? 0x2000 : mixer);
}

static const Puzzle echo_puzzles[] = {
    { "THE ECHO MEMORY", "NO DRIFT", "LOOP AT", loop_l, 3, "-", dash_l, 1, p7_hw, p7_model, 0 },
    { "THE ONE-WORD TAPE", "ONE WORD WRITTEN", "LENGTH", len_l, 3, "-", dash_l, 1, p8_hw, p8_model, 1 },
    { "THE CHANNEL TAP", "THE TAP SHOWS 2000", "SOURCE", src_l, 2, "CHAN 1", other_l, 2, p9_hw, p9_model, 32 },
};

// ---------------------------------------------------------------------------
// Audit

static void audit_echo(void)
{
    int drift, n, byte;
    s32 mean;
    if (ear_adpcm(1, &drift))
        tw_res[TV_ADPCM] = (u32)drift;
    if (ear_cap_len(0, &n))
        tw_res[TV_LEN0] = (u32)n;
    if (ear_cap_chan(true, 1, -1, false, &mean))
        tw_res[TV_CAP_SRC] = (u32)(mean + 32768);
    if (ear_pcm8(&byte))
        tw_res[TV_PCM8] = (u32)byte;
}

const Room room_echo = {
    "THE ECHO ROOM", "LOWER TOWER", "DS SOUND CAPTURE", echo_puzzles,
    (int)(sizeof(echo_puzzles) / sizeof(echo_puzzles[0])), audit_echo,
};
