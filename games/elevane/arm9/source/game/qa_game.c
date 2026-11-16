// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Elevane side of the auto QA mode (core/qa.h). The run takes the normal flow: title ->
// puzzle list -> every puzzle of every chapter, in order -> back to the list -> title ->
// credits -> title, then passes. The pilot plays each puzzle with the scripted player
// (game/script.c: the stored solution, built with the keys a person uses) through the live
// play scene and starts the chapters from the puzzle list with the D-pad and A. Nothing is
// tuned (qa_tune.h); the game saves nothing itself except the one write the run uses to
// prove the save path.

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/play.h"
#include "game/puzzle.h"
#include "game/qa_tune.h"
#include "game/scenes.h"
#include "game/script.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

static bool credits_seen, save_proved;
static unsigned chapters_done;   // bit n-1: chapter n was left through the puzzle list
static int prev_scene = -1;

const char *qa_game_name(void)
{
    return "elevane";
}

void qa_game_reset(void)
{
    credits_seen = save_proved = false;
    chapters_done = 0;
    prev_scene = -1;
    // A run starts from the same place whatever the player did before: first puzzle, 3D view,
    // nothing delivered (the PASS condition is that every puzzle is delivered during the run).
    ev_delivered_bits = 0;
    ev_chapter = 1;
    ev_index = 1;
    ev_view3d = true;
}

static unsigned delivered(void)
{
    return play_delivered_count();
}

u32 qa_game_hash(void)
{
    u32 w[3] = { delivered(), chapters_done, credits_seen };
    return qalog_fnv(QALOG_FNV_INIT, w, sizeof(w));
}

void qa_game_words(u32 *w)
{
    w[0] = delivered();
    w[1] = (u32)pz_count();
    w[2] = credits_seen;
    w[3] = chapters_done;
    w[4] = cv_frame;
    w[5] = 0;
}

static void fail(const char *reason)
{
    u32 w[QA_GAME_WORDS];
    qa_game_words(w);
    qa_fail(reason, w, QA_GAME_WORDS);
}

void qa_game_scene(int id)
{
    if (prev_scene == SC_PLAY && id == SC_SELECT)
    {
        // A chapter ends here (the last puzzle returns to the list).
        static const char *const names[] = { "ch0", "ch1", "ch2", "ch3", "ch4", "ch5", "ch6" };
        int ch = ev_chapter >= 1 && ev_chapter <= 5 ? ev_chapter : 0;
        chapters_done |= 1u << (ch - 1 < 0 ? 0 : ch - 1);
        qa_checkpoint(names[ch], qa_game_hash());
        qalog_line("game", "chapter_end", "n=%d delivered=%u", ch, delivered());
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
            credits_seen = delivered() >= (unsigned)pz_count();
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
        case SC_SHOWCASE:
            fail("unexpected-scene");
            break;
        default:
            break;
    }
}

// The block stays in the common QA layout (core/qalog.c): nothing to add.
void qa_game_publish(bool pass)
{
    (void)pass;
}

// The first chapter that still has a puzzle that was not delivered during this run (0: none).
static int chapter_for(void)
{
    for (int ch = 1; ch <= 5; ch++)
        for (int i = 1, n = pz_chapter_size(ch); i <= n; i++)
            if (!play_was_delivered(ch, i))
                return ch;
    return 0;
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_TITLE:
            if (delivered() >= (unsigned)pz_count() && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;   // the credits, as a player would
            break;
        case SC_SELECT:
        {
            if (f < 10 || (f & 7) != 4)
                break;
            int want = chapter_for();
            if (want == 0)
                out->keys = KEY_B;           // everything delivered: back to the title
            else if (ev_chapter != want)
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
