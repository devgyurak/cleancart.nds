// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Tonewick side of the auto QA mode (core/qa.h). The run takes the normal flow:
// title -> districts 1-4 -> credits -> title. The pilot only feeds keys through the live
// scenes; the audits the rooms run, and what they measure, are never touched. After
// the credits the run publishes one block in the rehearsal's layout (TWRH, lab.h)
// plus the QA words at values[48..54], so tools/judge.py grades it as boot "qa".

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/lab.h"
#include "game/qa_tune.h"
#include "game/rooms.h"
#include "game/scenes.h"

// No tunable numbers (see qa_tune.h).
const QaTune qa_tune_off = { 0 };
const QaTune qa_tune_on = { 0 };

static u32 rooms_done;     // bit n: district n was audited
static bool credits_seen;
static int prev_scene = -1;
static int prev_room;

const char *qa_game_name(void)
{
    return "tonewick";
}

void qa_game_reset(void)
{
    rooms_done = 0;
    credits_seen = false;
    prev_scene = -1;
    prev_room = 0;
}

u32 qa_game_hash(void)
{
    // Only the completion bits and the lines solved: the measured values move a little from run to run on a console.
    u32 k[2] = { tw_res[TV_DONE], tw_res[TV_LINES] };
    return qalog_fnv(QALOG_FNV_INIT, k, sizeof(k));
}

void qa_game_words(u32 *w)
{
    w[0] = rooms_done;
    w[1] = tw_res[TV_DONE];
    w[2] = credits_seen;
    w[3] = 0;
    w[4] = cv_frame;
    w[5] = 0;
}

void qa_game_scene(int id)
{
    if (prev_scene == SC_ROOM && (tw_res[TV_DONE] >> prev_room & 1))
    {
        rooms_done |= 1u << prev_room;
        char name[8] = { 'd', 'i', 's', 't', (char)('0' + prev_room + 1), 0 };
        qa_checkpoint(name, qa_game_hash());
    }
    prev_scene = id;
    switch (id)
    {
        case SC_ROOM:
            prev_room = tw_room;
            if (rooms_done == 0 && tw_room == 0)
                qa_save_written(save_write());   // the game saves nothing itself: prove the path once
            qalog_line("game", "district", "n=%d", tw_room + 1);
            break;
        case SC_CREDITS:
            credits_seen = rooms_done == (1u << TW_ROOMS) - 1;
            break;
        case SC_TITLE:
            if (credits_seen)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
                qa_suite_run("tonewick", NULL, 0, 0);   // the QARN block as a res file (no extra demos)
            }
            break;
        case SC_MENU:
        case SC_REHEARSAL:
        case SC_LOG:
        {
            u32 w[QA_GAME_WORDS];
            qa_game_words(w);
            qa_fail("unexpected-scene", w, QA_GAME_WORDS);
            break;
        }
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
    memcpy(res, tw_res, sizeof(res));
    res[TV_REHEARSED] = pass ? 1 : 0;   // the QA run reached its end
    r->magic = 0;
    for (int i = 0; i < 61; i++)
        r->values[i] = i < 48 ? res[i] : 0;
    for (int i = 0; i < 7; i++)
        r->values[48 + i] = common[i];
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
        case SC_ROOM:
            if (f > 30 && (f & 3) == 0)
                out->keys = room_pilot_key();   // dials, listen, then end the shift
            break;
        case SC_TITLE:
            if (rooms_done == (1u << TW_ROOMS) - 1 && f > 20 && (f & 15) == 8)
                out->keys = KEY_X;       // the credits, as a player would
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
