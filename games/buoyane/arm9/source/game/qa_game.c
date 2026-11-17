// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Buoyane side of the auto QA mode (core/qa.h). Nothing is tuned (qa_tune.h). The run takes the normal
// flow; the voyage scenes join it in the route PRs. At the end the light check runs once, the block is
// published in the rehearsal's layout (game/rehearsal.h) plus the QA words, and the judge suite
// (core/qasuite.h) runs the five check ROMs' scenes.

#include <nds.h>
#include <string.h>

#include "assets.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/sys.h"
#include "game/qa_tune.h"
#include "game/rehearsal.h"
#include "game/scenes.h"
#include "game/voyage.h"
#include "game/model.h"
#include "core/save.h"
#include "gfx/gl3d.h"
#include "judge/lightcheck.h"

const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

static u32 res[W_WORDS + 4];
static bool credits_seen, judged, save_proved;
static unsigned chapters_done;   // bit n-1: every approach of chapter n was finished
static int prev_scene = -1;

const char *qa_game_name(void)
{
    return "buoyane";
}

void qa_game_reset(void)
{
    memset(res, 0, sizeof(res));
    credits_seen = judged = save_proved = false;
    chapters_done = 0;
    prev_scene = -1;
    by_cleared_bits = 0;   // a run starts from the same place whatever the player did before
    by_chapter = 1;
    by_approach = 0;
}

u32 qa_game_hash(void)
{
    u32 w[4] = { (u32)by_cleared_bits, chapters_done, credits_seen, judged };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = (u32)by_cleared_count();
    w[1] = BY_CHAPTERS * BY_APPROACHES;
    w[2] = credits_seen;
    w[3] = chapters_done;
    w[4] = cv_frame;
    w[5] = judged;
}

static void suite_check(const char *name)
{
    for (int s = 0; s < LC_SCENES; s++)
        if (strcmp(name + 6, lightcheck_scene_name(s)) == 0)
            check_run(s);
}

static const QaSuiteDemo suite[LC_SCENES] = {
    { "check-dot", suite_check }, { "check-fog", suite_check }, { "check-edge", suite_check },
    { "check-depth", suite_check }, { "check-blend", suite_check },
};

void qa_game_scene(int id)
{
    if (prev_scene == SC_VOYAGE && id == SC_CHART)
    {
        // An approach ended (or was left): a chapter is done when all its approaches are.
        static const char *const names[] = { "ch0", "ch1", "ch2", "ch3", "ch4", "ch5" };
        int ch = by_chapter >= 1 && by_chapter <= BY_CHAPTERS ? by_chapter : 0;
        if (ch && by_chapter_cleared(ch) == BY_APPROACHES && !(chapters_done & (1u << (ch - 1))))
        {
            chapters_done |= 1u << (ch - 1);
            qa_checkpoint(names[ch], qa_game_hash());
            qalog_line("game", "chapter_end", "n=%d cleared=%d", ch, by_cleared_count());
        }
    }
    prev_scene = id;
    switch (id)
    {
        case SC_VOYAGE:
            if (!save_proved)
            {
                qa_save_written(save_write());   // the game saves nothing itself: prove the path once
                save_proved = true;
            }
            break;
        case SC_CREDITS:
            credits_seen = by_cleared_count() >= BY_CHAPTERS * BY_APPROACHES;
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                lightcheck_all(res);
                res[19] = (u32)by_cleared_count();
                res[20] = by_plan_hash();
                judged = true;
                qa_checkpoint("judge", qa_game_hash());
                qa_checkpoint("credits", qa_game_hash());
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
                qa_suite_run("buoyane", suite, LC_SCENES, 600);
            }
            break;
        default:
            break;
    }
}

u32 qa_game_gx_acks(void)
{
    return g3_overflow_acks();
}

void qa_game_publish(bool pass)
{
    volatile CvResult *r = CV_RESULT;
    u32 common[7];
    for (int i = 0; i < 7; i++)
        common[i] = r->values[i];
    r->magic = 0;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < (int)(sizeof(res) / sizeof(*res)) ? res[i] : 0;
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
            if (by_cleared_count() >= BY_CHAPTERS * BY_APPROACHES && !credits_seen && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;   // the credits, as a player would
            break;
        case SC_CHART:
        {
            if (f < 10 || (f & 7) != 4)
                break;
            // the first approach that was not finished, row by row; all done: back to the title
            int wc = 0, wa = 0;
            for (int ch = 1; ch <= BY_CHAPTERS && !wc; ch++)
                for (int ap = 0; ap < BY_APPROACHES; ap++)
                    if (!((by_cleared_bits >> ((ch - 1) * BY_APPROACHES + ap)) & 1))
                    {
                        wc = ch;
                        wa = ap;
                        break;
                    }
            if (!wc)
                out->keys = KEY_B;
            else if (by_chapter != wc)
                out->keys = KEY_DOWN;
            else if (by_approach != wa)
                out->keys = KEY_RIGHT;
            else
                out->keys = KEY_A;
            break;
        }
        case SC_VOYAGE:
            out->keys = by_pilot_keys(f);
            break;
        case SC_CREDITS:
            out->keys = KEY_A;
            if (f > 700)
                out->keys = KEY_B;
            break;
        default:
            break;
    }
}
