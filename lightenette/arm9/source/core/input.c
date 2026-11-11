// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors

#include <nds.h>
#include <string.h>

#include "core/input.h"
#include "core/sys.h"

#define REPLAY_MAGIC 0x50525643u  // "CVRP"
#define RF_TOUCH 1
#define RF_LID   2
#define MAX_RUNS 3700   // 16 + 8 * 3700 bytes fit CV_REPLAY_MAX; 8 KB chips hold fewer (replay_blob)

typedef struct {
    u32 magic;
    u32 seed;
    u16 count;
    u8 stage;
    u8 hour;     // RTC hour the run started with (stage 2 day/night)
    u8 lives;    // game state the run started from
    u8 bombs;
    u16 reserved;
} __attribute__((packed)) ReplayHeader;

typedef struct {
    u16 frames;
    u16 keys;
    u8 tx, ty;
    u8 flags;
    u8 pad;
} ReplayRun;

CvInput cv_in;

static enum { R_OFF, R_PLAY, R_REC } mode;
static bool finished;

// Playback
static const ReplayHeader *play_hdr;
static const ReplayRun *play_runs;
static u32 play_index, play_left;

// Recording (single static buffer, header + runs)
static struct {
    ReplayHeader hdr;
    ReplayRun runs[MAX_RUNS];
} rec __attribute__((aligned(4)));

static u16 repeat_timer;

static void read_live(u16 *keys, bool *touch, int *tx, int *ty, bool *lid)
{
    u16 k = ~REG_KEYINPUT & 0x03FF;
    u16 xy = cv_arm7.keyxy;
    if (!(xy & CV_KEYXY_X))
        k |= KEY_X;
    if (!(xy & CV_KEYXY_Y))
        k |= KEY_Y;
    *touch = !(xy & CV_KEYXY_PEN) && cv_arm7_packets > 0;
    *tx = cv_arm7.px;
    *ty = cv_arm7.py;
    *lid = (xy & CV_KEYXY_LID) != 0;
    *keys = k;
}

void input_update(void)
{
    u16 keys;
    bool touch, lid;
    int tx, ty;

    if (mode == R_PLAY)
    {
        if (play_left == 0)
        {
            if (play_index >= play_hdr->count)
            {
                finished = true;
                keys = 0;
                touch = lid = false;
                tx = ty = 0;
                goto apply;
            }
            play_left = play_runs[play_index].frames;
            play_index++;
        }
        const ReplayRun *r = &play_runs[play_index - 1];
        keys = r->keys;
        touch = (r->flags & RF_TOUCH) != 0;
        lid = (r->flags & RF_LID) != 0;
        tx = r->tx;
        ty = r->ty;
        play_left--;
    }
    else
    {
        read_live(&keys, &touch, &tx, &ty, &lid);
        if (mode == R_REC)
        {
            u8 flags = (touch ? RF_TOUCH : 0) | (lid ? RF_LID : 0);
            u8 bx = touch ? (u8)tx : 0, by = touch ? (u8)ty : 0;
            u16 n = rec.hdr.count;
            ReplayRun *last = n ? &rec.runs[n - 1] : NULL;
            if (last && last->keys == keys && last->flags == flags
                && last->tx == bx && last->ty == by && last->frames < 0xFFFF)
            {
                last->frames++;
            }
            else if (n < MAX_RUNS)
            {
                ReplayRun *r = &rec.runs[n];
                r->frames = 1;
                r->keys = keys;
                r->tx = bx;
                r->ty = by;
                r->flags = flags;
                r->pad = 0;
                rec.hdr.count = n + 1;
            }
        }
    }

apply:
    cv_in.pressed = keys & ~cv_in.held;
    cv_in.released = cv_in.held & ~keys;
    cv_in.held = keys;
    cv_in.touching = touch;
    if (touch)
    {
        cv_in.tx = tx;
        cv_in.ty = ty;
    }
    cv_in.lid = lid;

    // Auto-repeat for menus: first after 18 frames, then every 5.
    if (cv_in.pressed)
    {
        cv_in.repeat = cv_in.pressed;
        repeat_timer = 18;
    }
    else if (keys && --repeat_timer == 0)
    {
        cv_in.repeat = keys;
        repeat_timer = 5;
    }
    else
    {
        cv_in.repeat = 0;
    }
}

int input_rtc_hour(void)
{
    if (mode == R_PLAY)
        return play_hdr->hour;
    if (mode == R_REC)
        return rec.hdr.hour;
    return cv_arm7.hour;
}

void replay_record_start(u32 seed, int stage, int lives, int bombs)
{
    memset(&rec.hdr, 0, sizeof(rec.hdr));
    rec.hdr.magic = REPLAY_MAGIC;
    rec.hdr.seed = seed;
    rec.hdr.stage = (u8)stage;
    rec.hdr.hour = cv_arm7.hour;
    rec.hdr.lives = (u8)lives;
    rec.hdr.bombs = (u8)bombs;
    mode = R_REC;
    finished = false;
}

bool replay_play_start(const void *blob, u32 size)
{
    const ReplayHeader *h = blob;
    if (size < sizeof(ReplayHeader) || h->magic != REPLAY_MAGIC)
        return false;
    if (sizeof(ReplayHeader) + h->count * sizeof(ReplayRun) > size)
        return false;
    play_hdr = h;
    play_runs = (const ReplayRun *)(h + 1);
    play_index = 0;
    play_left = 0;
    mode = R_PLAY;
    finished = false;
    memset(&cv_in, 0, sizeof(cv_in));
    return true;
}

void replay_stop(void)
{
    mode = R_OFF;
}

bool replay_playing(void)   { return mode == R_PLAY; }
bool replay_recording(void) { return mode == R_REC; }
bool replay_finished(void)  { return finished; }

u32 replay_seed(void)
{
    return mode == R_PLAY ? play_hdr->seed : rec.hdr.seed;
}

int replay_stage(void)
{
    return mode == R_PLAY ? play_hdr->stage : rec.hdr.stage;
}

int replay_lives(void)
{
    return mode == R_PLAY ? play_hdr->lives : rec.hdr.lives;
}

int replay_bombs(void)
{
    return mode == R_PLAY ? play_hdr->bombs : rec.hdr.bombs;
}

bool replay_peek(const void *blob, u32 size, int *stage)
{
    const ReplayHeader *h = blob;
    if (!blob || size < sizeof(*h) || h->magic != REPLAY_MAGIC)
        return false;
    if (stage)
        *stage = h->stage;
    return true;
}

const void *replay_blob(u32 max, u32 *size)
{
    u32 fit = max > sizeof(rec.hdr) ? (max - sizeof(rec.hdr)) / sizeof(ReplayRun) : 0;
    if (rec.hdr.count > fit)
        rec.hdr.count = (u16)fit;
    *size = sizeof(rec.hdr) + rec.hdr.count * sizeof(ReplayRun);
    return &rec;
}
