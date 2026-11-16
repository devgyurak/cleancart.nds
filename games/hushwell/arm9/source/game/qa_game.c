// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
//
// Hushwell side of the auto QA mode (core/qa.h): tuning, the autopilot and
// the scene hooks that judge the run.
//
// The pilot reads the loaded chapter map (W.map): it walks a breadth-first
// path to the next thing worth taking (tapes, keys, the key that starts the
// chase last), opens doors it holds the key for, sets the chapter 3 dial lock
// on the touch screen, reaches the exit (the well in chapter 5) and takes the
// well ending with every tape. Taps A through every other scene.

#include <nds.h>
#include <string.h>

#include "core/cvmath.h"
#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/sys.h"
#include "game/game.h"
#include "game/qa_tune.h"
#include "game/world.h"

// As shipped.
const QaTune qa_tune_off = { .hurt_damage = 1, .seed = 0 };

// QA run: the creature's hits cost no health (flash, scream and screen stay),
// and a fixed run seed makes the creature's choices repeat.
const QaTune qa_tune_on = { .hurt_damage = 0, .seed = 0x51A5E5u };

const char *qa_game_name(void)
{
    return "hushwell";
}

static u32 chapters_cleared;
static bool ending_seen, credits_seen;
static u32 progress_frame;     // scene frame of the last progress
static u32 progress_mark;      // what counted as progress then

void qa_game_publish(bool pass)
{
    (void)pass;
}

static u32 latched_tapes, latched_ending;

void qa_game_reset(void)
{
    chapters_cleared = 0;
    ending_seen = credits_seen = false;
    progress_frame = progress_mark = 0;
    latched_tapes = latched_ending = 0;
    // Settings that change the run come from the save; the run uses the defaults
    // (the save is restored when it ends).
    cv_save.hud_swap = 0;
    cv_save.look_touch = 0;
    cv_save.night_mode = 0;
    cv_save.mic_on = 0;
}

u32 qa_game_hash(void)
{
    u32 v[11] = { (u32)cv_game.chapter, (u32)cv_game.health, (u32)cv_game.flares,
                  (u32)cv_game.battery, cv_game.tapes, cv_game.keys, cv_game.taken_lo,
                  cv_game.taken_hi, cv_game.chapter_frames, rng_state(), (u32)cv_game.ending };
    return qalog_fnv(QALOG_FNV_INIT, v, sizeof(v));
}

void qa_game_words(u32 *w)
{
    w[0] = chapters_cleared;
    w[1] = latched_tapes | cv_game.tapes;
    w[2] = latched_ending ? latched_ending : (u32)cv_game.ending;   // the credits clear it
    w[3] = (u32)cv_game.health;
    w[4] = cv_frame;
    w[5] = (u32)cv_game.deaths;
}

static void fail(const char *reason)
{
    u32 w[QA_GAME_WORDS];
    qa_game_words(w);
    qa_fail(reason, w, QA_GAME_WORDS);
}

void qa_game_scene(int id)
{
    switch (id)
    {
        case SC_CHAPTER:
            qalog_line("game", "chapter", "n=%d health=%d tapes=%02x keys=%x radio=%d", cv_game.chapter,
                       cv_game.health, cv_game.tapes, cv_game.keys, cv_game.radio);
            break;
        case SC_CLEAR:
        {
            char name[8] = { 'c', 'h', (char)('0' + cv_game.chapter), 0 };
            qa_checkpoint(name, qa_game_hash());
            qalog_line("game", "chapter_end", "n=%d cleared=%d health=%d tapes=%02x frames=%lu",
                       cv_game.chapter, cv_game.cleared, cv_game.health, cv_game.tapes,
                       (unsigned long)cv_game.chapter_frames);
            if (cv_game.cleared)
                chapters_cleared++;
            else
                fail("chapter-not-cleared");
            break;
        }
        case SC_ENDING:
            ending_seen = true;
            latched_tapes = cv_game.tapes;
            latched_ending = (u32)cv_game.ending;
            qalog_line("game", "ending", "kind=%d tapes=%02x", cv_game.ending, cv_game.tapes);
            if (cv_game.ending != END_WELL)
                fail("ending-not-well");   // the pilot takes every tape: the full ending
            break;
        case SC_CREDITS:
            credits_seen = ending_seen && chapters_cleared >= CV_CHAPTERS;
            break;
        case SC_RECORDS:   // after the credits of an ending the game shows the records
            if (credits_seen)
            {
                u32 w[QA_GAME_WORDS];
                qa_game_words(w);
                qa_pass("credits", w, QA_GAME_WORDS);
            }
            else
            {
                fail("unexpected-scene");
            }
            break;
        case SC_DEATH:
            fail("player-died");
            break;
        case SC_SETTINGS:
        case SC_SOUNDTEST:
        case SC_DEMOPLAY:
            fail("unexpected-scene");
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Autopilot

#define U(x) ((int32_t)((x) * 4096))
#define CELLS (MAP_MAX * MAP_MAX)

static int cell_of(int32_t v)
{
    return v >> 12;
}

static int32_t center(int c)
{
    return c * 4096 + 2048;
}

static int obj_door_at(int x, int y)
{
    for (int i = 0; i < W.map.nobj; i++)
    {
        const CvObj *o = &W.map.obj[i];
        if ((o->type == O_DOOR || o->type == O_CODEDOOR) && o->x == x && o->y == y
            && !world_obj_taken(i))
            return i;
    }
    return -1;
}

// 0 blocked, 1 free, 2 a door we can open (key held), 3 the dial lock.
static int cell_kind(int x, int y, bool allow_code)
{
    if (x < 0 || y < 0 || x >= W.map.w || y >= W.map.h)
        return 0;
    int d = obj_door_at(x, y);
    if (d >= 0)
    {
        const CvObj *o = &W.map.obj[d];
        if (o->type == O_CODEDOOR)
            return allow_code ? 3 : 0;
        return (cv_game.keys & (1 << o->param)) ? 2 : 0;
    }
    return world_cell_solid(x, y) ? 0 : 1;
}

static u8 par[CELLS];     // BFS parent direction (0..3) + 1, 0 = unseen
static u16 dist[CELLS];

static const int DX[4] = { 0, 1, 0, -1 };
static const int DZ[4] = { -1, 0, 1, 0 };

static void bfs(int sx, int sy, bool allow_code)
{
    static u16 queue[CELLS];
    memset(par, 0, sizeof(par));
    int head = 0, tail = 0;
    queue[tail++] = (u16)(sy * W.map.w + sx);
    par[sy * W.map.w + sx] = 5;
    dist[sy * W.map.w + sx] = 0;
    while (head < tail)
    {
        int c = queue[head++];
        int x = c % W.map.w, y = c / W.map.w;
        for (int d = 0; d < 4; d++)
        {
            int nx = x + DX[d], ny = y + DZ[d];
            int n = ny * W.map.w + nx;
            if (nx < 0 || ny < 0 || nx >= W.map.w || ny >= W.map.h || par[n])
                continue;
            if (!cell_kind(nx, ny, allow_code))
                continue;
            par[n] = (u8)(d + 1);
            dist[n] = dist[c] + 1;
            queue[tail++] = (u16)n;
        }
    }
}

// Tier 0: tapes and ordinary keys. 1: the chase key. 2: the way out.
static int tier_of(const CvObj *o)
{
    if (o->type == O_TAPE)
        return 0;
    if (o->type == O_KEY)
        return o->param == KEY_CHASE ? 1 : 0;
    if (o->type == O_EXIT || o->type == O_WELL)
        return 2;
    return -1;
}

// Picks the target cell; returns false when nothing is reachable.
static bool choose_target(int sx, int sy, int *tx, int *ty, bool *via_code)
{
    for (int pass = 0; pass < 2; pass++)
    {
        bfs(sx, sy, pass == 1);
        for (int tier = 0; tier < 3; tier++)
        {
            int best = -1, bd = 0xFFFF;
            for (int i = 0; i < W.map.nobj; i++)
            {
                const CvObj *o = &W.map.obj[i];
                if (tier_of(o) != tier || world_obj_taken(i))
                    continue;
                int c = o->y * W.map.w + o->x;
                if (!par[c] || dist[c] >= bd)
                    continue;
                bd = dist[c];
                best = i;
            }
            if (best >= 0)
            {
                *tx = W.map.obj[best].x;
                *ty = W.map.obj[best].y;
                *via_code = pass == 1;
                return true;
            }
        }
    }
    return false;
}

// Turns toward and walks to `(wx, wz)`. True when facing it within about 20 degrees.
static bool steer(int32_t wx, int32_t wz, QaInput *out, bool run)
{
    int32_t fx = m_sin(W.pl.yaw), fz = -m_cos(W.pl.yaw);        // 20.12 unit vector
    int32_t dx = wx - W.pl.pos.x, dz = wz - W.pl.pos.z;
    int32_t len = m_len(m_vec(dx, 0, dz));
    if (len < U(0.05))
        return true;
    int64_t cross = (int64_t)fx * dz - (int64_t)fz * dx;      // > 0: turn right
    int64_t dot = (int64_t)fx * dx + (int64_t)fz * dz;
    int64_t lim = (int64_t)len * 1400;                        // sin(20 degrees) of 4096
    if (cross > 0)
        out->keys |= KEY_RIGHT;
    else
        out->keys |= KEY_LEFT;
    bool aligned = dot > 0 && (cross < lim && cross > -lim);
    if (aligned)
    {
        out->keys &= ~(KEY_LEFT | KEY_RIGHT);
        if (cross > (int64_t)len * 250)
            out->keys |= KEY_RIGHT;
        else if (cross < -(int64_t)len * 250)
            out->keys |= KEY_LEFT;
        out->keys |= KEY_UP;
        if (run && len > U(1.5))
            out->keys |= KEY_B;
    }
    return aligned;
}

// Chapter 3 dial lock on the touch screen: taps the digits to the code, then OK.
static void dial(u32 f, QaInput *out)
{
    static int x, y;
    static bool tap_now;
    u32 phase = f % 6;
    if (phase == 0)
    {
        const int want[3] = { CODE_A, CODE_B, CODE_C };
        tap_now = true;
        x = 128;
        y = 130;   // OK, unless a digit is wrong
        for (int i = 0; i < 3; i++)
        {
            int cur = W.code_digits[i], diff = (want[i] - cur + 10) % 10;
            if (diff == 0)
                continue;
            x = (7 + i * 7) * 8 + 8;
            y = diff <= 5 ? 34 : 98;
            break;
        }
    }
    if (phase < 2 && tap_now)
    {
        out->touch = true;
        out->tx = x;
        out->ty = y;
    }
}

static void chapter_pilot(u32 vb, QaInput *out)
{
    static u32 ticks, last_vb;
    if (vb < last_vb)
        ticks = 0;
    last_vb = vb;
    u32 f = ++ticks;   // pilot calls, not VBlanks: a chapter frame can take two
    if (W.intro)
    {
        progress_frame = f;
        if ((f & 15) == 8)
            out->keys = KEY_A;   // the scripted openings skip on A
        return;
    }
    if (W.pl.dead || W.peek)
    {
        progress_frame = f;   // waiting is not being stuck
        return;
    }

    if (W.code_mode)
    {
        progress_frame = f;
        dial(f, out);
        return;
    }

    int px = cell_of(W.pl.pos.x), py = cell_of(W.pl.pos.z);
    if (px < 0 || py < 0 || px >= W.map.w || py >= W.map.h)
        return;
    // Standing on the well with every tape: "A: go down".
    for (int i = 0; i < W.map.nobj; i++)
        if (W.map.obj[i].type == O_WELL && W.map.obj[i].x == px && W.map.obj[i].y == py && (f & 1))
            out->keys |= KEY_A;
    if (f < progress_frame)
        progress_frame = f;
    u32 mark = (u32)px * 97 + (u32)py * 31 + cv_game.taken_lo + cv_game.taken_hi * 7 + cv_game.keys;
    if (mark != progress_mark)
    {
        progress_mark = mark;
        progress_frame = f;
    }
    else if (f - progress_frame > 900)
    {
        int stx = -1, sty = -1;
        bool sv;
        choose_target(px, py, &stx, &sty, &sv);
        qalog_line("pilot", "stuck", "pos=%d,%d cell=%d,%d target=%d,%d code=%d keys=%x yaw=%d peek=%d", (int)W.pl.pos.x,
                   (int)W.pl.pos.z, px, py, stx, sty, sv, cv_game.keys, W.pl.yaw, W.peek);
        fail("pilot-stuck");
        return;
    }

    int tx, ty;
    bool via_code;
    if (!choose_target(px, py, &tx, &ty, &via_code))
    {
        if (f - progress_frame > 300)
            fail("no-reachable-target");
        return;
    }
    bfs(px, py, via_code);
    int t = ty * W.map.w + tx;
    int cur = py * W.map.w + px;
    if (cur == t)
        return;   // pickups() fires on the cell itself
    // Walk the parent chain back to the cell after ours.
    int c = t;
    for (;;)
    {
        if (par[c] == 0 || par[c] > 4)
        {
            fail("pilot-path");   // cannot happen: the target was reached by this BFS
            return;
        }
        int d = par[c] - 1;
        int prev = (c / W.map.w - DZ[d]) * W.map.w + (c % W.map.w - DX[d]);
        if (prev == cur)
            break;
        c = prev;
    }
    int nx = c % W.map.w, ny = c / W.map.w;
    int ax = nx - px;
    // Stay on the line to the next cell: pull back to the cell centre if off it.
    int32_t lat = ax ? W.pl.pos.z - center(py) : W.pl.pos.x - center(px);
    bool off_line = lat > U(0.14) || lat < -U(0.14);
    int32_t wx = off_line ? center(px) : center(nx);
    int32_t wz = off_line ? center(py) : center(ny);
    int kind = cell_kind(nx, ny, via_code);
    if (kind >= 2 && lat < U(0.3) && lat > -U(0.3))
    {
        // The next cell is a door: stop in front of it, face its centre, interact.
        int32_t ddx = center(nx) - W.pl.pos.x, ddz = center(ny) - W.pl.pos.z;
        if (m_len(m_vec(ddx, 0, ddz)) < U(1.1))   // interact() reaches 0.75 ahead of the eye
        {
            bool facing = steer(center(nx), center(ny), out, false);
            out->keys &= ~(KEY_UP | KEY_B);
            if (facing && (f & 1))
                out->keys |= KEY_A;
            return;
        }
    }
    steer(wx, wz, out, kind == 1);
}

void qa_game_pilot(int scene, u32 f, QaInput *out)
{
    switch (scene)
    {
        case SC_CHAPTER:
            chapter_pilot(f, out);
            break;
        case SC_CREDITS:
            out->keys = KEY_A;   // holding A scrolls faster
            break;
        default:
            // Title, radio, clear, ending, records: tap A.
            if ((f & 15) == 8)
                out->keys = KEY_A;
            break;
    }
}
