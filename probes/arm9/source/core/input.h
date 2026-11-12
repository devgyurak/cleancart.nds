// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 devgyurak (cleancart.nds maintainer)
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

// Replay logs. Recording keeps runs in RAM (input_replay_blob()).
void replay_record_start(u32 seed, int stage, int lives, int bombs);
bool replay_play_start(const void *blob, u32 size);
void replay_stop(void);
bool replay_playing(void);
bool replay_recording(void);
bool replay_finished(void);
u32  replay_seed(void);
int  replay_stage(void);
int  replay_lives(void);
int  replay_bombs(void);
bool replay_peek(const void *blob, u32 size, int *stage);
// The recorded log, cut to its first runs so it fits `max` bytes (the save
// chip's replay area; playback then ends where the log ends).
const void *replay_blob(u32 max, u32 *size);

#endif // CV_INPUT_H
