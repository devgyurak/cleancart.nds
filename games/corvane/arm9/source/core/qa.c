// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)

#include <nds.h>

#include "core/input.h"
#include "core/qa.h"
#include "core/qalog.h"
#include "core/rng.h"
#include "core/save.h"
#include "core/sys.h"

static bool active;
static int combo;
static int scene_id = -1;
static u32 scene_start;        // cv_frame when the scene was entered
static u32 last_frame;         // cv_frame at the previous autopilot call
static u32 lag_since_cp;
static u32 run_start;
static CvSave save_backup;   // the cart's own save, put back when the run ends
// The last run's input log lives in its own area of the chip: a QA run records
// stage/chapter logs too, so that area is backed up and restored as well.
static u8 replay_backup[CV_REPLAY_MAX] __attribute__((aligned(4)));
static u32 replay_backup_size;

bool qa_active(void)
{
    return active;
}

const QaTune *qa_tune(void)
{
    return active ? &qa_tune_on : &qa_tune_off;
}

int qa_rtc_hour(void)
{
    return QA_RTC_HOUR;
}

bool qa_title_poll(void)
{
    if (active || replay_playing() || replay_recording())
    {
        combo = 0;
        return false;
    }
    if ((cv_in.held & QA_COMBO_KEYS) == QA_COMBO_KEYS)
        combo++;
    else
        combo = 0;
    if (combo < QA_COMBO_FRAMES)
        return false;
    combo = 0;
    active = true;
    run_start = cv_frame;
    last_frame = cv_frame;
    lag_since_cp = 0;
    save_backup = cv_save;
    replay_backup_size = save_load_replay(replay_backup, sizeof(replay_backup));
    qa_game_reset();
    qalog_open(qa_game_name(), 0);
    qalog_line("qa", "start", "combo=%d", QA_COMBO_FRAMES);
    return true;
}

bool qa_autopilot(QaInput *out)
{
    if (!active)
        return false;
    u32 now = cv_frame;
    if (now - last_frame > 1)
        lag_since_cp += now - last_frame - 1;   // frames the game didn't get to run
    last_frame = now;
    if (qa_scene_frames() > QA_SCENE_LIMIT)
    {
        u32 w[QA_GAME_WORDS];
        qa_game_words(w);
        qa_fail("scene-timeout", w, QA_GAME_WORDS);
        return false;
    }
    out->keys = 0;
    out->touch = false;
    out->tx = out->ty = 0;
    qa_game_pilot(scene_id, qa_scene_frames(), out);
    return true;
}

u32 qa_scene_frames(void)
{
    return cv_frame - scene_start;
}

int qa_scene_id(void)
{
    return scene_id;
}

void qa_scene(int id, const char *name)
{
    scene_id = id;
    scene_start = cv_frame;
    if (!active)
        return;
    qalog_line(name, "enter", "run_frames=%lu", (unsigned long)(cv_frame - run_start));
    qa_game_scene(id);
    qalog_flush();
}

void qa_checkpoint(const char *name, u32 hash)
{
    if (!active)
        return;
    qalog_checkpoint("qa", name, hash, lag_since_cp);
    lag_since_cp = 0;
}

static void end_run(bool pass, const char *reason, const u32 *words, int n)
{
    if (!active)
        return;
    // A QA run saves like a real one (the mirror proves the write path), then the
    // save that was on the cart goes back: a run never leaves boosted lives or
    // scores behind.
    // Keep the sequence number rising: the chip's newer slot wins at the next
    // boot, so the restored data has to be newer than the QA run's last write.
    u16 seq = cv_save.seq;
    cv_save = save_backup;
    cv_save.seq = seq;
    bool restored = save_write();
    bool replay_restored = save_replay(replay_backup, replay_backup_size);
    qalog_line("save", "restore", "chip_ok=%d replay_bytes=%lu replay_ok=%d", restored,
               (unsigned long)replay_backup_size, replay_restored);
    qalog_finish(pass, reason, words, n);
    qa_game_publish(pass);
    active = false;
}

void qa_pass(const char *reason, const u32 *words, int n)
{
    end_run(true, reason, words, n);
}

void qa_fail(const char *reason, const u32 *words, int n)
{
    end_run(false, reason, words, n);
}

void qa_save_written(bool chip_ok)
{
    if (!active)
        return;
    qalog_save_mirror(&cv_save, sizeof(cv_save), chip_ok);
}
