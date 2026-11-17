// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Corvane side of the auto QA mode (core/qa.h): tuning, autopilot, state hash
// and the scene hooks that judge the run.

#include <nds.h>
#include <stdio.h>

#include "core/qa.h"
#include "core/qalog.h"
#include "core/qasuite.h"
#include "core/qashot.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/timer.h"
#include "core/sys.h"
#include "demo/demo.h"
#include "game/game.h"
#include "game/qa_tune.h"
#include "gfx/gl3d.h"
#include "gfx/vram.h"
#include "game/stage.h"

// As shipped: every number at 1x.
const QaTune qa_tune_off = {
    .boss_hp_pct = 100, .lives_lost_per_hit = 1, .lives = 3, .bombs = 2,
};

// QA run: weak bosses, the ship takes hits (effects and sound stay), spare lives
// and bombs for the pilot.
const QaTune qa_tune_on = {
    .boss_hp_pct = 10, .lives_lost_per_hit = 0, .lives = 9, .bombs = 9,
};

static bool credits_seen;
static u32 stages_cleared;
static int shot_stage;           // stage whose 10-second shot was taken
static int pilot_scene = -1;     // scene the pilot call counter belongs to
static u32 pilot_calls;          // pilot calls since that scene started (not VBlanks)

// Checkpoint screenshots (core/qashot.c) need an A-D bank that is LCDC and unused. Of the
// layouts in gfx/vram.c only SPACE (stage 3) has one: bank D, which the boost afterimage
// uses as its capture buffer, so it is free while the ship is not boosting (the pilot never
// boosts). CORE (stage 5) and DUAL3D run a capture of their own; MENU, TERRAIN, SKY,
// CORRIDOR and MMEM put every bank A-D to use.
int qa_game_shot_bank(void)
{
    switch (vram_current())
    {
        case VL_SPACE:
            return S.pl.boost > 0 ? QA_SHOT_CAPTURE_BUSY : VB_D;
        case VL_CORE:
        case VL_DUAL3D:
            return QA_SHOT_CAPTURE_BUSY;
        default:
            return QA_SHOT_NO_BANK;
    }
}

// The game acknowledges the polygon/vertex RAM overflow flag itself (g3_overflowed()); core/qa.c
// counts those acknowledgements as frames with the flag up.
u32 qa_game_gx_acks(void)
{
    return g3_overflow_acks();
}

void qa_game_publish(bool pass)
{
    (void)pass;
}

void qa_game_reset(void)
{
    credits_seen = false;
    stages_cleared = 0;
    shot_stage = 0;
    pilot_scene = -1;
    // Settings that change the run come from the save; the run uses the defaults
    // (the save is restored when it ends).
    cv_save.difficulty = 0;
    cv_save.hud_swap = 0;
}

const char *qa_game_name(void)
{
    return "corvane";
}

#define U(x) ((int32_t)((x) * 4096))

u32 qa_game_hash(void)
{
    u32 h = QALOG_FNV_INIT;
    u32 v[8] = { (u32)cv_game.stage, cv_game.score, (u32)cv_game.lives, (u32)cv_game.bombs,
                 cv_game.stage_frames, cv_game.stage_kills, cv_game.stage_spawned,
                 rng_state() };
    h = qalog_fnv(h, v, sizeof(v));
    u32 c = cv_game.cleared ? 1 : 0;
    return qalog_fnv(h, &c, sizeof(c));
}

void qa_game_words(u32 *w)
{
    w[0] = cv_game.score;
    w[1] = stages_cleared;
    w[2] = (u32)cv_game.lives;
    w[3] = (u32)cv_game.bombs;
    w[4] = cv_frame;
    w[5] = (u32)cv_game.stage;
}

void qa_game_scene(int id)
{
    switch (id)
    {
        case SC_TITLE:
        case SC_RECORDS:   // after the final credits the game shows the records
            if (credits_seen)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
                // The verdict and the QARN block are out: the judged demos (core/qasuite.h).
                qa_suite_run("corvane", demo_suite, demo_suite_count, 900);
            }
            else if (id == SC_RECORDS)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_fail("unexpected-scene", w, QA_GAME_WORDS);
            }
            break;
        case SC_BRIEFING:
            qalog_line("game", "stage", "n=%d score=%lu lives=%d bombs=%d", cv_game.stage,
                       (unsigned long)cv_game.score, cv_game.lives, cv_game.bombs);
            break;
        case SC_RESULT:
        case SC_CONTINUE:
        {
            char name[12];
            name[0] = 's';
            name[1] = 't';
            name[2] = (char)('0' + cv_game.stage);
            name[3] = 0;
            qa_checkpoint(name, qa_game_hash());
            qalog_line("game", "stage_end", "n=%d cleared=%d score=%lu lives=%d frames=%lu kills=%lu/%lu",
                       cv_game.stage, cv_game.cleared, (unsigned long)cv_game.score, cv_game.lives,
                       (unsigned long)cv_game.stage_frames, (unsigned long)cv_game.stage_kills,
                       (unsigned long)cv_game.stage_spawned);
            if (cv_game.cleared)
                stages_cleared++;
            if (id == SC_CONTINUE || !cv_game.cleared)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_fail(id == SC_CONTINUE ? "player-lost" : "stage-not-cleared", w, QA_GAME_WORDS);
            }
            break;
        }
        case SC_CREDITS:
            credits_seen = stages_cleared >= CV_STAGES;
            break;
        case SC_SETTINGS:
        case SC_SOUNDTEST:
        case SC_DEMOPLAY:
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

// Steering: fly to the nearest threat (or the boss) and shoot. A taps every
// other frame (the ship's cooldown decides the rate), bombs on the boss.
static bool target_of(int32_t *tx, int32_t *ty)
{
    if (S.boss_active)
    {
        CvVec b;
        if (!boss_aim_point(&b))
            b = boss_pos();
        *tx = b.x;
        *ty = b.y;
        return true;
    }
    int best = -1;
    int32_t bd = U(40);
    for (int i = 0; i < MAX_ENEMIES; i++)
    {
        const Enemy *e = &S.en[i];
        if (!e->alive || e->type == E_METEOR)
            continue;
        int32_t dz = S.pl.pos.z - e->pos.z;   // ahead of the ship is -z
        if (dz < 0 || dz >= bd)
            continue;
        bd = dz;
        best = i;
    }
    if (best < 0)
        return false;
    *tx = S.en[best].pos.x;
    *ty = S.en[best].pos.y;
    return true;
}

// Shot points are counted in pilot calls so that a frame that takes two VBlanks (or a capture
// that delays one) never moves them.
static void shot_points(int scene)
{
    char name[16];
    if (scene != pilot_scene)
    {
        pilot_scene = scene;
        pilot_calls = 0;
    }
    pilot_calls++;
    if (scene == SC_STAGE && S.frame == 600 && shot_stage != cv_game.stage)
    {
        shot_stage = cv_game.stage;
        snprintf(name, sizeof(name), "st%d-600", cv_game.stage);
        qa_shot(name);
    }
    else if (scene == SC_RESULT && pilot_calls == 6)
    {
        snprintf(name, sizeof(name), "st%d-result", cv_game.stage);
        qa_shot(name);
    }
    else if (scene == SC_CREDITS && pilot_calls == 90)
    {
        qa_shot("credits");
    }
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    shot_points(scene);
    switch (scene)
    {
        case SC_STAGE:
        {
            int32_t tx, ty;
            if (!target_of(&tx, &ty))
            {
                tx = 0;
                ty = U(3);
            }
            const int32_t dead = U(0.2);
            if (S.pl.pos.x < tx - dead) out->keys |= KEY_RIGHT;
            if (S.pl.pos.x > tx + dead) out->keys |= KEY_LEFT;
            if (S.pl.pos.y < ty - dead) out->keys |= KEY_UP;
            if (S.pl.pos.y > ty + dead) out->keys |= KEY_DOWN;
            // Tap A every other game frame (S.frame, not VBlanks: a heavy frame can
            // take two VBlanks, which would pin the parity).
            if ((S.frame & 1) == 0)
                out->keys |= KEY_A;
            if (S.boss_active && cv_game.bombs > 0 && S.frame > 0 && (S.frame % 300) == 0)
                out->keys |= KEY_X;
            break;
        }
        case SC_CREDITS:
            out->keys = KEY_A;   // holding A scrolls faster
            break;
        default:
            // Title, briefing, result, ending: tap A.
            if ((f & 15) == 8)
                out->keys = KEY_A;
            break;
    }
}

// The suite starts every demo from boot's counters: the boss beat of the run counted into cv_beats.
void qa_suite_reset_game(void)
{
    cv_beats = 0;
    g3_init();   // geometry engine as boot leaves it (texture matrix, state a 3D demo set)
}
