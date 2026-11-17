// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Pendulane side of the auto QA mode (core/qa.h). The run takes the normal flow: title -> contraption list -> every
// contraption of every chapter, each chapter ended by its rhythm piece -> list -> title -> credits -> title, where the epilogue
// runs the bench once and the run passes. Nothing is tuned (qa_tune.h).

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/device.h"
#include "game/ledger.h"
#include "game/play.h"
#include "game/qa_tune.h"
#include "game/rehearsal.h"
#include "game/rhythm.h"
#include "game/scenes.h"
#include "game/script.h"

const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

// The combined block in the rehearsal's layout (game/rehearsal.h).
static u32 res[PD_VALUES];
static bool credits_seen, save_proved, judged;
static unsigned chapters_done;   // bit n-1: chapter n was left through its rhythm piece
static int prev_scene = -1;
static int hits_total;

const char *qa_game_name(void)
{
    return "pendulane";
}

void qa_game_reset(void)
{
    memset(res, 0, sizeof(res));
    credits_seen = save_proved = judged = false;
    chapters_done = 0;
    prev_scene = -1;
    hits_total = 0;
    // A run starts from the same place whatever the player did before.
    dev_solved_clear();
    ledger_forget();
    pd_chapter = 1;
    pd_index = 1;
}

u32 qa_game_hash(void)
{
    u32 w[5] = { (u32)dev_solved_count(), chapters_done, credits_seen, judged, (u32)hits_total };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = (u32)dev_solved_count();
    w[1] = (u32)dev_count();
    w[2] = credits_seen;
    w[3] = chapters_done;
    w[4] = cv_frame;
    w[5] = judged;
}

// The bench once, after the credits: the same measurement the rehearsal makes.
static void judge_epilogue(void)
{
    bench_fill(res);
    res[1] = (u32)chapters_done;
    res[2] = (u32)dev_solved_count();
    judged = true;
    qa_checkpoint("judge", qa_game_hash());
}

void qa_game_scene(int id)
{
    if (prev_scene == SC_RHYTHM && id == SC_SELECT)
    {
        static const char *const names[] = { "ch0", "ch1", "ch2", "ch3", "ch4", "ch5" };
        int ch = pd_chapter >= 1 && pd_chapter <= PD_CHAPTERS ? pd_chapter : 0;
        if (ch)
            chapters_done |= 1u << (ch - 1);
        hits_total += pd_last_hits;
        qa_checkpoint(names[ch], qa_game_hash());
        qalog_line("game", "chapter_end", "n=%d solved=%d hits=%d", ch, dev_solved_count(), pd_last_hits);
    }
    prev_scene = id;
    switch (id)
    {
        case SC_PLAY:
            if (!save_proved)
            {
                qa_save_written(save_write());   // the game saves nothing itself: prove the path once
                save_proved = true;
            }
            break;
        case SC_CREDITS:
            credits_seen = dev_solved_count() >= dev_count();
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                judge_epilogue();
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

// The first chapter that still has a contraption that was not solved during this run (0: none).
static int chapter_for(void)
{
    for (int ch = 1; ch <= PD_CHAPTERS; ch++)
        if (!dev_chapter_done(ch))
            return ch;
    return 0;
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
            if (dev_solved_count() >= dev_count() && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;   // the credits, as a player would
            break;
        case SC_SELECT:
        {
            if (f < 10 || (f & 7) != 4)
                break;
            int want = chapter_for();
            if (want == 0)
                out->keys = KEY_B;           // everything solved: back to the title
            else if (pd_chapter != want)
                out->keys = KEY_RIGHT;
            else
                out->keys = KEY_A;
            break;
        }
        case SC_PLAY:
        {
            u16 keys;
            script_step(&keys, true);
            out->keys = keys;
            break;
        }
        case SC_RHYTHM:
            for (int i = 0; i < RHYTHM_NOTES; i++)
                if ((int)f == rhythm_note_frame(i))
                    out->keys = KEY_A;
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
