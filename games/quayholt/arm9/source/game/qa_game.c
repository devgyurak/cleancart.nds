// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Quayholt side of the auto QA mode (core/qa.h). The run takes the normal flow: title -> rehearsal
// (every measurement the build has) -> credits -> title. The pilot only feeds keys through the live
// scenes; what the measurements find is never touched. After the credits the run publishes one block
// in the rehearsal's layout (QYRH, ledger.h) plus the QA words at values[48..54], so tools/judge.py
// grades it as boot "qa".

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/save.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "game/harbour.h"
#include "game/ledger.h"
#include "game/qa_tune.h"
#include "game/scenes.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

static bool rehearsal_seen;   // the measurements ran and the tally was published
static bool credits_seen;
static int prev_scene = -1;
static u32 piers_done;      // bit n: pier n's five jobs were all balanced when a checkpoint was taken

const char *qa_game_name(void)
{
    return "quayholt";
}

void qa_game_reset(void)
{
    rehearsal_seen = false;
    credits_seen = false;
    prev_scene = -1;
    piers_done = 0;
}

u32 qa_game_hash(void)
{
    // Only the flag words and the result codes (values[0..31]): the raw words are not hashed.
    // Result codes and which jobs are balanced; whether a box was crooked is emulator dependent and stays out.
    return qalog_fnv(qalog_fnv(QALOG_FNV_INIT, qy_res, 32 * sizeof(u32)), qy_job_done, sizeof(qy_job_done));
}

void qa_game_words(u32 *w)
{
    w[0] = rehearsal_seen;
    w[1] = qy_res[QV_DONE];
    w[2] = credits_seen;
    w[3] = (u32)jobs_cleared();
    w[4] = cv_frame;
    w[5] = 0;
}

void qa_game_scene(int id)
{
    if (prev_scene == SC_REHEARSAL && (qy_res[QV_DONE] & QD_REHEARSAL))
    {
        rehearsal_seen = true;
        qa_checkpoint("rehearsal", qa_game_hash());
    }
    for (int pier = 0; pier < QY_NUM_PIERS; pier++)
    {
        bool all = true;
        for (int k = 0; k < 5; k++)
            all = all && qy_job_done[pier * 5 + k];
        if (all && !(piers_done >> pier & 1))
        {
            piers_done |= 1u << pier;
            char name[8] = { 'p', 'i', 'e', 'r', (char)('a' + pier), 0 };
            qa_checkpoint(name, qa_game_hash());
        }
    }
    prev_scene = id;
    switch (id)
    {
        case SC_REHEARSAL:
            qa_save_written(save_write());   // the game saves nothing itself: prove the path once
            break;
        case SC_CREDITS:
            credits_seen = rehearsal_seen && jobs_cleared() == QY_NUM_JOBS;
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
                // The verdict and the QARN block are out: now the judged demos (core/qasuite.h).
                qa_suite_run("quayholt", demo_suite, demo_suite_count, 600);
            }
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
    u32 res[61];
    memcpy(res, qy_res, sizeof(res));
    r->magic = 0;
    if (rehearsal_seen)
        res[QV_DONE] |= QD_REHEARSAL;
    res[QV_JOBS] = (u32)jobs_cleared();
    res[QV_PLAN] = jobs_plan_hash();
    for (int i = 0; i < 61; i++)
        r->values[i] = i < 48 ? res[i] : 0;
    for (int i = 0; i < 7; i++)
        r->values[48 + i] = common[i];
    (void)pass;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

// ---------------------------------------------------------------------------
// Autopilot

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
            if (f > 20 && (f & 15) == 8)
            {
                if (jobs_cleared() < QY_NUM_JOBS)
                    out->keys = KEY_START;      // the harbour first
                else if (!rehearsal_seen)
                    out->keys = KEY_SELECT;     // the test menu, for the rehearsal
                else
                    out->keys = KEY_X;          // the credits, as a player would
            }
            break;
        case SC_MENU:
            if (f > 20 && (f & 7) == 4)
                out->keys = KEY_A;              // item 0: the rehearsal
            break;
        case SC_PIER:
        case SC_JOB:
            if (f > 20 && (f & 3) == 0)
                out->keys = harbour_pilot_key(scene);
            break;
        case SC_REHEARSAL:
            if (f > 90)
                out->keys = KEY_B;       // the tally has been up long enough
            break;
        case SC_CREDITS:
            out->keys = KEY_A;           // holding A scrolls faster
            if (f > 700)
                out->keys = KEY_B;       // the credits loop: leave after one pass
            break;
        default:
            break;
    }
}
