// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Corvane side of the auto QA mode (core/qa.h): tuning, autopilot, state hash
// and the scene hooks that judge the run.

#include <nds.h>

#include "core/qa.h"
#include "core/qalog.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/game.h"
#include "game/qa_tune.h"
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

void qa_game_publish(bool pass)
{
    (void)pass;
}

void qa_game_reset(void)
{
    credits_seen = false;
    stages_cleared = 0;
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

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
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
