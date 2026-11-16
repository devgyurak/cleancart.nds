// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Paneveil side of the auto QA mode (core/qa.h). The run takes the normal flow: title ->
// window list -> every window of every chapter, in order -> back to the list -> title ->
// credits -> title, then passes. Nothing is tuned (qa_tune.h); the game saves nothing
// itself except the one write the run uses to prove the save path.

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/qa_tune.h"
#include "game/scenes.h"
#include "game/script.h"
#include "game/window.h"
#include "judge/bench.h"
#include "judge/lightbox.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

// The combined block in the rehearsal's layout (game/rehearsal.h): values[0..30].
static u32 res[31];
static bool credits_seen, save_proved, judged;
static unsigned chapters_done;   // bit n-1: chapter n was left through the window list
static int prev_scene = -1;

const char *qa_game_name(void)
{
    return "paneveil";
}

void qa_game_reset(void)
{
    memset(res, 0, sizeof(res));
    credits_seen = save_proved = judged = false;
    chapters_done = 0;
    prev_scene = -1;
    // A run starts from the same place whatever the player did before: first window, 3D view,
    // nothing baked (the PASS condition is that every window is baked during the run).
    pv_baked_bits = 0;
    pv_chapter = 1;
    pv_index = 1;
    pv_view3d = true;
}

static unsigned baked(void)
{
    return (unsigned)win_baked_count();
}

u32 qa_game_hash(void)
{
    u32 w[4] = { baked(), chapters_done, credits_seen, judged };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = baked();
    w[1] = (u32)win_count();
    w[2] = credits_seen;
    w[3] = chapters_done;
    w[4] = cv_frame;
    w[5] = judged;
}

// The kiln check and the light box once, after the credits: the same measurement the rehearsal makes.
static void judge_epilogue(void)
{
    u32 kiln[KILN_ITEMS];
    bool kiln_complete = kiln_run(kiln);
    for (int i = 0; i < KILN_ITEMS; i++)
        res[3 + i] = kiln[i];
    LightboxResult lb;
    lightbox_run(&lb);
    for (int i = 0; i < LB_ITEMS; i++)
        res[16 + i] = lb.v[i];
    res[0] = (kiln_complete ? 1u : 0) | (kiln_ok(kiln) ? 2u : 0) | lb.done << 8;
    res[1] = (u32)__builtin_popcount(chapters_done);
    res[2] = chapters_done;
    judged = true;
    qa_checkpoint("judge", qa_game_hash());
}

void qa_game_scene(int id)
{
    if (prev_scene == SC_PLAY && id == SC_SELECT)
    {
        // A chapter ends here (the last window returns to the list).
        static const char *const names[] = { "ch0", "ch1", "ch2", "ch3", "ch4", "ch5" };
        int ch = pv_chapter >= 1 && pv_chapter <= PV_CHAPTERS ? pv_chapter : 0;
        if (ch)
            chapters_done |= 1u << (ch - 1);
        qa_checkpoint(names[ch], qa_game_hash());
        qalog_line("game", "chapter_end", "n=%d baked=%u", ch, baked());
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
            credits_seen = baked() >= (unsigned)win_count();
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

// The rehearsal's block layout (values[0..30]) plus the QA words after it (values[48..54]).
void qa_game_publish(bool pass)
{
    volatile CvResult *r = CV_RESULT;
    u32 common[7];
    for (int i = 0; i < 7; i++)
        common[i] = r->values[i];
    r->magic = 0;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < 31 ? res[i] : 0;
    for (int i = 0; i < 7; i++)
        r->values[48 + i] = common[i];
    (void)pass;
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
    r->magic = CV_RESULT_MAGIC;   // last
    DC_FlushRange((void *)CV_RESULT_ADDR, sizeof(CvResult));
}

// The first chapter that still has a window that was not baked during this run (0: none).
static int chapter_for(void)
{
    for (int ch = 1; ch <= PV_CHAPTERS; ch++)
        for (int i = 1, n = win_chapter_size(ch); i <= n; i++)
            if (!win_was_baked(ch, i))
                return ch;
    return 0;
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
            if (baked() >= (unsigned)win_count() && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;   // the credits, as a player would
            break;
        case SC_SELECT:
        {
            if (f < 10 || (f & 7) != 4)
                break;
            int want = chapter_for();
            if (want == 0)
                out->keys = KEY_B;           // everything baked: back to the title
            else if (pv_chapter != want)
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
        case SC_CREDITS:
            out->keys = KEY_A;       // holding A scrolls faster
            if (f > 700)
                out->keys = KEY_B;   // the credits loop: leave after one pass
            break;
        default:
            break;
    }
}
