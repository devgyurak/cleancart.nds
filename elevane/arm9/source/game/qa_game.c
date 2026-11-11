// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Elevane side of the auto QA mode (core/qa.h). The run takes the normal flow:
// title -> credits -> title, then passes. The puzzle chapters join the flow with
// their scenes; nothing is tuned (qa_tune.h) and the game saves nothing itself
// except the one write the run uses to prove the save path.

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/qa_tune.h"
#include "game/scenes.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

static bool credits_seen;
static bool save_proved;

const char *qa_game_name(void)
{
    return "elevane";
}

void qa_game_reset(void)
{
    credits_seen = save_proved = false;
}

u32 qa_game_hash(void)
{
    u32 w[2] = { credits_seen, save_proved };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = credits_seen;
    w[1] = save_proved;
    w[2] = cv_frame;
    w[3] = w[4] = w[5] = 0;
}

void qa_game_scene(int id)
{
    switch (id)
    {
        case SC_CREDITS:
            if (!save_proved)
            {
                qa_save_written(save_write());   // prove the save path once
                save_proved = true;
            }
            credits_seen = true;
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                qa_checkpoint("credits", qa_game_hash());
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
            }
            break;
        default:
            break;
    }
}

// The block stays in the common QA layout (core/qalog.c): nothing to add yet.
void qa_game_publish(bool pass)
{
    (void)pass;
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
            if (f > 20 && (f & 15) == 8)
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
