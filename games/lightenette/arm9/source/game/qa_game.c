// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Lightenette side of the auto QA mode (core/qa.h). The run takes the normal
// flow: title -> acts 1-5 -> credits -> title. The pilot plays each act with the
// rehearsal's input scripts (game/rehearsal.c) through the live scenes. Nothing
// the judge measures is tuned or touched: each act publishes its own result block
// as always, the hook below copies those blocks at the scene changes and, after
// the credits, runs the accuracy judge once (judge_run, the same code the test
// menu runs) so that one block in the rehearsal's layout (RHEL, see
// rehearsal.c) can be graded by tools/judge.py (boot "qa").

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/qa_tune.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "judge/judge.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

// "ACT1" .. "ACT5": the per-act result blocks (act1.c .. act5.c)
#define ACTN_ID(n) (0x30544341u + ((u32)(n) << 24))

// The combined block in the rehearsal's layout, filled act by act.
static u32 res[61];
static u32 acts_done;      // bit n-1: act n was harvested
static bool credits_seen;
static bool judged;
static int prev_scene = -1;
static u32 ticks;          // pilot calls in this scene: frame t of the act's script
static u32 ready_ticks;    // ticks since the act reported clear and measured

const char *qa_game_name(void)
{
    return "lightenette";
}

void qa_game_reset(void)
{
    memset(res, 0, sizeof(res));
    acts_done = 0;
    credits_seen = judged = false;
    prev_scene = -1;
    ticks = ready_ticks = 0;
}

u32 qa_game_hash(void)
{
    return qalog_fnv(QALOG_FNV_INIT, res, sizeof(res));
}

void qa_game_words(u32 *w)
{
    w[0] = acts_done;
    w[1] = res[10] | res[13] << 1 | res[18] << 2 | res[27] << 3 | res[32] << 4;   // acts cleared
    w[2] = credits_seen;
    w[3] = judged;
    w[4] = cv_frame;
    w[5] = 0;
}

// The act's own block, while its loop (or the one that just ended) owns it.
static bool act_block(int act, u32 *v, int nv)
{
    volatile CvResult *r = CV_RESULT;
    if (r->magic != CV_RESULT_MAGIC || r->demo_id != ACTN_ID(act))
        return false;
    for (int i = 0; i < nv; i++)
        v[i] = r->values[i];
    return true;
}

static void harvest(int act)
{
    u32 v[13];
    memset(v, 0, sizeof(v));
    if (!act_block(act, v, 13))
        return;
    switch (act)
    {
        case 1:
            res[9] = v[0];
            res[10] = v[1];
            break;
        case 2:
            res[13] = v[0];
            res[14] = v[1];
            res[15] = v[2];
            res[17] = v[4];
            break;
        case 3:
            res[18] = v[0];
            res[19] = v[1];
            res[20] = v[2];
            res[21] = v[3];
            res[22] = v[4];
            res[23] = v[5];
            res[24] = v[6];
            res[25] = v[7];
            break;
        case 4:
            res[27] = v[0];
            res[29] = v[1];
            res[30] = v[2];
            res[31] = v[5];
            break;
        case 5:
            res[32] = v[0];
            res[34] = v[2];
            res[35] = v[3];
            res[36] = v[4];
            res[37] = v[5];
            res[38] = v[6];
            res[39] = v[7];
            res[40] = v[8];
            res[41] = v[9];
            res[42] = v[10];
            res[43] = v[11];
            res[44] = v[12];
            break;
    }
    acts_done |= 1u << (act - 1);
    char name[6] = { 'a', 'c', 't', (char)('0' + act), 0 };
    qa_checkpoint(name, qa_game_hash());
    qalog_line("game", "act_end", "n=%d words=%lu", act, (unsigned long)v[0]);
}

static void fail(const char *reason)
{
    u32 w[QA_GAME_WORDS];
    qa_game_words(w);
    qa_fail(reason, w, QA_GAME_WORDS);
}

// The accuracy judge, once, after the credits: the same measurement the test menu
// runs, and the part of the block the acts do not fill.
static void judge_epilogue(void)
{
    JudgeResult jr;
    judge_run(&jr, false);
    res[0] = (u32)jr.seam[0] | (u32)jr.seam[1] << 8 | (u32)jr.seam[2] << 16;
    res[1] = jr.edges;
    res[2] = jr.lines;
    res[3] = jr.flags;
    res[4] = jr.lamp[0];
    res[5] = jr.lamp[1];
    res[6] = jr.lamp[2];
    res[7] = jr.out_none;
    res[8] = jr.out_dark;
    res[12] = 1;   // "rehearsal finished": the whole flow ran
    judged = true;
    qa_checkpoint("judge", qa_game_hash());
}

void qa_game_scene(int id)
{
    ticks = 0;
    ready_ticks = 0;
    if (prev_scene >= SC_ACT1 && prev_scene <= SC_ACT5)
        harvest(prev_scene - SC_ACT1 + 1);
    prev_scene = id;
    switch (id)
    {
        case SC_ACT1:
            if (acts_done == 0)
                qa_save_written(save_write());   // the game saves nothing itself: prove the path once
            qalog_line("game", "act", "n=1");
            break;
        case SC_ACT2: case SC_ACT3: case SC_ACT4: case SC_ACT5:
            qalog_line("game", "act", "n=%d", id - SC_ACT1 + 1);
            break;
        case SC_CREDITS:
            credits_seen = acts_done == 0x1F;
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                judge_epilogue();
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
            }
            break;
        case SC_JUDGE:
        case SC_MENU:
        case SC_REHEARSAL:
            fail("unexpected-scene");
            break;
        default:
            break;
    }
}

// The rehearsal's block layout, plus the QA words after it (values[48..54]).
void qa_game_publish(bool pass)
{
    volatile CvResult *r = CV_RESULT;
    u32 common[7];
    for (int i = 0; i < 7; i++)
        common[i] = r->values[i];
    r->magic = 0;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < 45 ? res[i] : 0;
    for (int i = 0; i < 7; i++)
        r->values[48 + i] = common[i];
    (void)pass;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

// ---------------------------------------------------------------------------
// Autopilot

// Whether the act's clear and its captures are done (ready for A).
static bool act_ready(int act)
{
    u32 v[13];
    if (!act_block(act, v, 13))
        return false;
    switch (act)
    {
        case 1: return v[1] != 0;
        case 2: return v[0] && v[4] != 0;
        case 3: return v[0] && v[7] == 7;
        case 4: return v[0] && v[5] != 0;
        default: return v[0] && v[12] == 7;
    }
}

static void act_pilot(int act, QaInput *out)
{
    u32 t = ++ticks;
    int n, end;
    switch (act)
    {
        case 1:
        {
            const Act1Step *s = rehearsal_act1_script(&n, &end);
            for (int i = 0; i < n && s[i].frame <= t; i++)
            {
                out->touch = s[i].ty != 0;
                out->tx = s[i].tx;
                out->ty = s[i].ty;
            }
            break;
        }
        case 2:
        {
            const Act2Step *s = rehearsal_act2_script(&n, &end);
            for (int i = 0; i < n; i++)
                if (s[i].frame == t)
                    out->keys |= s[i].keys;
            break;
        }
        case 3:
        {
            const Act3Step *s = rehearsal_act3_script(&n, &end);
            for (int i = 0; i < n; i++)
                if (t >= s[i].frame && t < (u32)s[i].frame + s[i].len)
                    out->keys |= s[i].keys;
            break;
        }
        case 4:
        {
            const Act4Step *s = rehearsal_act4_script(&n, &end);
            for (int i = 0; i < n; i++)
                if (s[i].frame == t)
                    out->keys |= s[i].keys;
            break;
        }
        default:
        {
            const Act5Step *s = rehearsal_act5_script(&n, &end);
            for (int i = 0; i < n; i++)
                if (t >= s[i].frame && t < (u32)s[i].frame + s[i].len)
                    out->keys |= s[i].keys;
            break;
        }
    }
    if (act_ready(act))
    {
        if (++ready_ticks > 20 && (ready_ticks & 7) == 4)
            out->keys = KEY_A;   // on to the next act (act 5: back to the title)
    }
    else if (t > (u32)end + 600)
    {
        fail("act-not-cleared");
        // The act's loop writes its own result block every frame and would bury
        // the QARN verdict: stop here, the log and the block stay as they are.
        for (;;)
            sys_vsync();
    }
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_ACT1: case SC_ACT2: case SC_ACT3: case SC_ACT4: case SC_ACT5:
            act_pilot(scene - SC_ACT1 + 1, out);
            break;
        case SC_TITLE:
            if (acts_done == 0x1F && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;   // the credits, as a player would
            break;
        case SC_CREDITS:
            out->keys = KEY_A;       // holding A scrolls faster
            if (f > 700)
                out->keys = KEY_B;   // the credits loop: leave after one pass
            break;
        default:
            break;
    }
}
