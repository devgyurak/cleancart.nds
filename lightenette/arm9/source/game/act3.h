// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: 2026 Corvane contributors
//
// Act 3 with scripted key input, used by the rehearsal.

#ifndef LT_ACT3_H
#define LT_ACT3_H

#include <nds.h>
#include <stdbool.h>

// Key script: the keys are held for `len` frames starting at `frame` (counted
// from the start of the act).
typedef struct {
    u16 frame;
    u16 len;
    u16 keys;
} Act3Step;

typedef struct {
    bool cleared;
    int clear_frame;   // frame the third mark was found, -1 if not
    bool measured;     // all three lantern captures finished
    int found;         // marks found (0-3)
    u16 lamp[3];       // lantern pixel with BLDCNT 0100 / 0140 / 01C1
    u16 out_none;      // backdrop pixel outside the lantern, BLDCNT 0100
    u16 out_dark;      // ... BLDCNT 01C1
} Act3Outcome;

// Plays the act from the key script (no pen, no save, no NitroFS). Returns
// when the measurement is done or `end_frame` is reached.
void act3_replay(const Act3Step *steps, int n, int end_frame, Act3Outcome *out);

#endif // LT_ACT3_H
