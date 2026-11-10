// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Per-frame input: A/B/L/R/D-pad/Start/Select from REG_KEYINPUT, X/Y, pen
// and lid from the ARM7 packet. Everything external the game reads (keys,
// touch, lid, RTC hour) passes through here so it can be logged and
// replayed frame by frame.

#ifndef CV_INPUT_H
#define CV_INPUT_H

#include <nds.h>
#include <stdbool.h>

typedef struct {
    u16 held, pressed, released, repeat;
    bool touching;
    int tx, ty;
    bool lid;
} CvInput;

extern CvInput cv_in;

void input_update(void);

// RTC hour (0-23): live, or the value stored in the replay being played.
int input_rtc_hour(void);

// State a recorded run starts from (a chapter start or an emergency radio).
typedef struct {
    u8 chapter, hour, health, flares;
    u8 radio, keys, battery, tapes;
    u32 taken_lo, taken_hi;
    u8 options;     // RUN_OPT_*: settings that change gameplay
    u8 pad[3];
} CvRunStart;

#define RUN_OPT_LOOK_TOUCH  1   // touch drag turns the view
#define RUN_OPT_NIGHT_SHIFT 1   // bits 1-2: night visit mode (0 RTC, 1 always, 2 never)

// Replay logs. Recording keeps runs in RAM (replay_blob()).
void replay_record_start(u32 seed, const CvRunStart *start);
bool replay_play_start(const void *blob, u32 size);
void replay_stop(void);
// While suspended, input is read live but not logged (pause menu).
void replay_suspend(bool on);
bool replay_playing(void);
bool replay_recording(void);
bool replay_finished(void);
u32  replay_seed(void);
const CvRunStart *replay_run_start(void);
bool replay_peek(const void *blob, u32 size, int *chapter);
const void *replay_blob(u32 *size);

#endif // CV_INPUT_H
