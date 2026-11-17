// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Pendulane side of the auto QA mode (core/qa.h). The run takes the normal flow: title -> credits
// -> title, where the epilogue runs the bench once and the run passes. Nothing is tuned (qa_tune.h).

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

const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

// The combined block in the rehearsal's layout (game/rehearsal.h).
static u32 res[PD_VALUES];
static bool credits_seen, save_proved, judged;

const char *qa_game_name(void)
{
    return "pendulane";
}

void qa_game_reset(void)
{
    memset(res, 0, sizeof(res));
    credits_seen = save_proved = judged = false;
}

u32 qa_game_hash(void)
{
    u32 w[2] = { credits_seen, judged };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = 0;
    w[1] = 0;
    w[2] = credits_seen;
    w[3] = 0;
    w[4] = cv_frame;
    w[5] = judged;
}

// The bench once, after the credits: the same measurement the rehearsal makes.
static void judge_epilogue(void)
{
    bench_fill(res);
    judged = true;
    qa_checkpoint("judge", qa_game_hash());
}

void qa_game_scene(int id)
{
    switch (id)
    {
        case SC_TITLE:
            if (!save_proved)
            {
                qa_save_written(save_write());   // the game saves nothing itself: prove the path once
                save_proved = true;
            }
            if (credits_seen)
            {
                judge_epilogue();
                qa_checkpoint("credits", qa_game_hash());
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
            }
            break;
        case SC_CREDITS:
            credits_seen = true;
            break;
        default:
            break;
    }
}

// The rehearsal's block layout plus the QA words after it (values[48..54]).
void qa_game_publish(bool pass)
{
    volatile CvResult *r = CV_RESULT;
    u32 common[7];
    for (int i = 0; i < 7; i++)
        common[i] = r->values[i];
    r->magic = 0;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < PD_VALUES ? res[i] : 0;
    for (int i = 0; i < 7; i++)
        r->values[48 + i] = common[i];
    (void)pass;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
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
