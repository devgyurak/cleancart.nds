// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Auto QA mode: the game plays itself to the end and writes a QA log (see
// qalog.h). It lives in the same ROM as the release build and stays hidden:
// hold L+R+X+Y on the title screen for 120 frames. Nothing is drawn for it.
//
// QA mode may only
//   - feed input (the autopilot, through core/input.c, like replay playback),
//   - change numbers through qa_tune() (boss HP, damage taken, lives, ...),
//   - fix the RTC hour and log.
// It must not branch inside rendering (gfx/), the save code (core/save.c),
// judging or capture. tools/qa/lint.py enforces that. A run saves like a real
// one (game saves + SD mirror), then restores the save the cart had.
// Keep this file identical across games (core/ is copied, not shared); the
// game side is game/qa_game.c plus game/qa_tune.h.

#ifndef CV_QA_H
#define CV_QA_H

#include <nds.h>
#include <stdbool.h>

#define QA_COMBO_KEYS    (KEY_L | KEY_R | KEY_X | KEY_Y)
#define QA_COMBO_FRAMES  120
#define QA_SCENE_LIMIT   (60 * 60 * 12)   // frames one scene may last before FAIL
#define QA_RTC_HOUR      12               // fixed in QA runs: same run on any clock

// One frame of injected input.
typedef struct {
    u16 keys;
    bool touch;
    int tx, ty;
} QaInput;

// Numbers QA may change. The struct is the game's (game/qa_tune.h); with QA
// off qa_tune() returns the "off" values (1x), so callers always read
// qa_tune()->field and never test qa_active() themselves.
typedef struct QaTune QaTune;
const QaTune *qa_tune(void);

bool qa_active(void);
// Title screen: call once per frame after input_update(). True on the frame
// the combo completes (QA is active from then on); never while a replay plays.
bool qa_title_poll(void);

// core/input.c: fills `out` and returns true while QA owns the input.
bool qa_autopilot(QaInput *out);
int  qa_rtc_hour(void);

// Main loop: a scene is entered (logs, flushes, lets the game judge).
void qa_scene(int id, const char *name);
int  qa_scene_id(void);
u32  qa_scene_frames(void);   // frames spent in the current scene

void qa_checkpoint(const char *name, u32 hash);
// Ends the run: RESULT line, SD flush, RAM result block. words[0..n) go to
// result words 7.. (see qalog.h). QA is off afterwards.
void qa_pass(const char *reason, const u32 *words, int n);
void qa_fail(const char *reason, const u32 *words, int n);

// After every save_write(): mirrors the image on the SD card and logs it.
void qa_save_written(bool chip_ok);

// Game side (game/qa_game.c).
extern const QaTune qa_tune_on, qa_tune_off;
void qa_game_pilot(int scene, u32 scene_frame, QaInput *out);
// After the RESULT line and the QARN block: a game whose judge reads a block of
// its own layout (lightenette) rewrites it here. Others leave it empty.
void qa_game_publish(bool pass);
void qa_game_reset(void);            // new run: clear the game side's statics
void qa_game_scene(int id);          // called by qa_scene(); may checkpoint / pass / fail
u32  qa_game_hash(void);             // game state only, never tuning values
const char *qa_game_name(void);
#define QA_GAME_WORDS 6
void qa_game_words(u32 *words);      // QA_GAME_WORDS values for the result block

#endif // CV_QA_H
